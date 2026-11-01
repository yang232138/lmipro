// SPDX-License-Identifier: BSD-2-Clause-Patent
// SM8250 SMMUv2 audit: MMIO reads only. No UIC, UTP, DMA or reset submissions.
// Definitions: Linux v6.12 sm8250.dtsi and arm-smmu/{arm-smmu.h,arm-smmu.c}.
#include <stdint.h>

struct LmiproStream {
  uint32_t sid, matches, index, smr, s2cr, contextRead, cbar, sctlr, fsr, farLo, farHi;
};
struct LmiproSmmuResult {
  uint32_t stage, scr0, id0, id1, id2, gfsr, syn0, syn1, syn2;
  uint32_t pgshift, cbBase, banks, groups, extended;
  LmiproStream stream[2];
};

template<class Ops>
static LmiproSmmuResult lmiproSmmuAudit(Ops& io)
{
  LmiproSmmuResult r = {};
  r.stream[0].sid=0xe0; r.stream[1].sid=0x4e0;
  r.scr0=io.read32(0); r.id0=io.read32(0x20);
  r.id1=io.read32(0x24); r.id2=io.read32(0x28);
  r.gfsr=io.read32(0x48); r.syn0=io.read32(0x50);
  r.syn1=io.read32(0x54); r.syn2=io.read32(0x58);
  r.stage=1;
  if (!r.id0 || r.id0==0xffffffffU || r.id1==0xffffffffU) return r;
  r.stage=2; // Unsupported indexing or inconsistent extended-ID configuration.
  if (!(r.id0 & (1U<<27))) return r;
  r.extended=(r.scr0>>3)&1U;
  if (r.extended && !(r.id0 & (1U<<8))) return r;
  r.groups=r.id0 & 0xffU;
  if (!r.groups) return r;
  r.pgshift=(r.id1 & (1U<<31)) ? 16 : 12;
  r.cbBase=1U << (((r.id1>>28)&7U)+1+r.pgshift);
  r.banks=r.id1 & 0xffU;
  const bool geometry=(r.cbBase==0x80000U && r.banks>0 && r.banks<=128 &&
                       ((r.id1>>16)&0xffU)<=r.banks &&
                       r.banks<=(0x100000U-r.cbBase)>>r.pgshift);
  r.stage=geometry ? 3 : 4; // Bad geometry still permits bounded global table reads.
  for (uint32_t i=0;i<r.groups;++i) {
    uint32_t smr=io.read32(0x800+4*i), s2cr=io.read32(0xc00+4*i);
    if (r.extended ? !(s2cr & (1U<<10)) : !(smr & (1U<<31))) continue;
    uint32_t width=r.extended ? 0xffffU : 0x7fffU;
    uint32_t mask=(smr>>16)&width, id=smr&width;
    for (unsigned j=0;j<2;++j) {
      LmiproStream& s=r.stream[j];
      if (((s.sid^id)&~mask&width)!=0) continue;
      if (++s.matches==1) { s.index=i; s.smr=smr; s.s2cr=s2cr; }
    }
  }
  for (unsigned j=0;j<2;++j) {
    LmiproStream& s=r.stream[j];
    uint32_t bank=s.s2cr&0xffU;
    if (!geometry || s.matches!=1 || ((s.s2cr>>16)&3U)!=0 || bank>=r.banks) continue;
    uint32_t cb=r.cbBase+(bank<<r.pgshift);
    // Read only the matched translation bank, using ID1-derived page geometry.
    s.cbar=io.read32((1U<<r.pgshift)+4*bank);
    s.sctlr=io.read32(cb); s.fsr=io.read32(cb+0x58);
    s.farLo=io.read32(cb+0x60); s.farHi=io.read32(cb+0x64);
    s.contextRead=1;
  }
  return r;
}

#ifndef LMIPRO_UIC_HOST_TEST
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <kern/debug.h>
extern "C" {
volatile uint32_t lmipro_ufs_handoff_observation[24];
volatile uint32_t lmipro_ufs_read_result[16];
}
struct LmiproReadOnlyMmio {
  uintptr_t base;
  uint32_t read32(unsigned off) {
    if (off>0xffffcU || (off&3U)) panic("LMIPRO SMMU RO1: invalid offset %x",off);
    uint32_t v=*reinterpret_cast<const volatile uint32_t*>(base+off);
    __asm__ volatile("dsb sy" ::: "memory");
    return v;
  }
};
extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode=0;
  if (!PE_parse_boot_argn("lmipro-ufs-handoff",&mode,sizeof(mode))) return;
  if (mode!=24) panic("LMIPRO SMMU RO1: only mode 24 enabled; mode=%u",mode);
  IODeviceMemory* mem=IODeviceMemory::withRange(0x15000000ULL,0x100000);
  if (!mem) panic("LMIPRO SMMU RO1: descriptor failed");
  OSSharedPtr<IOMemoryMap> keep(mem->map(kIOMapAnywhere|kIOMapInhibitCache),OSNoRetain);
  mem->release();
  if (!keep) panic("LMIPRO SMMU RO1: mapping failed");
  LmiproReadOnlyMmio io={static_cast<uintptr_t>(keep->getVirtualAddress())};
  LmiproSmmuResult r=lmiproSmmuAudit(io);
  const LmiproStream& a=r.stream[0]; const LmiproStream& b=r.stream[1];
  lmipro_ufs_handoff_observation[0]=r.stage;
  lmipro_ufs_handoff_observation[1]=r.scr0;
  lmipro_ufs_handoff_observation[2]=r.id0;
  lmipro_ufs_handoff_observation[3]=r.id1;
  lmipro_ufs_handoff_observation[4]=r.gfsr;
  panic("LMIPRO SMMU RO1 (no register writes)\n"
        "stage=%u cr0=%08x id0=%08x\nid1=%08x id2=%08x ext=%u\n"
        "pg=%u cb=%08x banks=%u groups=%u\ngfsr=%08x syn=%08x/%08x/%08x\n"
        "e0 n=%u i=%u smr=%08x s2cr=%08x\n"
        "e0 ctx=%u cbar=%08x sctlr=%08x\ne0 fsr=%08x far=%08x:%08x\n"
        "4e0 n=%u i=%u smr=%08x s2cr=%08x\n"
        "4e0 ctx=%u cbar=%08x sctlr=%08x\n4e0 fsr=%08x far=%08x:%08x",
        r.stage,r.scr0,r.id0,r.id1,r.id2,r.extended,r.pgshift,r.cbBase,r.banks,r.groups,
        r.gfsr,r.syn0,r.syn1,r.syn2,
        a.matches,a.index,a.smr,a.s2cr,a.contextRead,a.cbar,a.sctlr,a.fsr,a.farHi,a.farLo,
        b.matches,b.index,b.smr,b.s2cr,b.contextRead,b.cbar,b.sctlr,b.fsr,b.farHi,b.farLo);
}
#endif
