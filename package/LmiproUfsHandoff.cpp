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
  uint32_t scanned, valid, nonzero, allones, types[4], hash, hash2, changed, globalChanged;
  uint32_t sampleCount, sampleIndex[4], sampleSmr[4], sampleS2cr[4];
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
  // Qualcomm limits stream matching registers to the architected first 128.
  // Keep the advertised count separate from the bounded count actually read.
  r.scanned=r.groups>128 ? 128 : r.groups;
  uint32_t savedSmr[128]={}, savedS2cr[128]={};
  r.hash=2166136261U;
  for (uint32_t i=0;i<r.scanned;++i) {
    uint32_t smr=io.read32(0x800+4*i), s2cr=io.read32(0xc00+4*i);
    savedSmr[i]=smr; savedS2cr[i]=s2cr;
    r.hash=(r.hash^smr)*16777619U; r.hash=(r.hash^s2cr)*16777619U;
    const bool valid=r.extended ? (s2cr & (1U<<10)) : (smr & (1U<<31));
    if (smr || s2cr) {
      ++r.nonzero;
      if (r.sampleCount<4) {
        unsigned n=r.sampleCount++;
        r.sampleIndex[n]=i; r.sampleSmr[n]=smr; r.sampleS2cr[n]=s2cr;
      }
    }
    if (smr==0xffffffffU && s2cr==0xffffffffU) ++r.allones;
    if (!valid) continue;
    ++r.valid; ++r.types[(s2cr>>16)&3U];
    uint32_t width=r.extended ? 0xffffU : 0x7fffU;
    uint32_t mask=(smr>>16)&width, id=smr&width;
    for (unsigned j=0;j<2;++j) {
      LmiproStream& s=r.stream[j];
      if (((s.sid^id)&~mask&width)!=0) continue;
      if (++s.matches==1) { s.index=i; s.smr=smr; s.s2cr=s2cr; }
    }
  }
  // A second exact comparison detects changing entries; fingerprints are only
  // compact screen summaries, never a security hash or a substitute for compare.
  r.hash2=2166136261U;
  for (uint32_t i=0;i<r.scanned;++i) {
    uint32_t smr=io.read32(0x800+4*i), s2cr=io.read32(0xc00+4*i);
    r.hash2=(r.hash2^smr)*16777619U; r.hash2=(r.hash2^s2cr)*16777619U;
    if (smr!=savedSmr[i] || s2cr!=savedS2cr[i]) ++r.changed;
  }
  r.globalChanged=(io.read32(0)!=r.scr0 ? 1U:0U) |
                  (io.read32(0x20)!=r.id0 ? 2U:0U) |
                  (io.read32(0x24)!=r.id1 ? 4U:0U) |
                  (io.read32(0x28)!=r.id2 ? 8U:0U) |
                  (io.read32(0x48)!=r.gfsr ? 16U:0U);
  // No context banks, page tables, UFS controller or data buffers are accessed.
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
    if ((off&3U) || (off>0x58 && !(off>=0x800 && off<=0x9fc) && !(off>=0xc00 && off<=0xdfc))) panic("LMIPRO SMMU RO2: invalid offset %x",off);
    uint32_t v=*reinterpret_cast<const volatile uint32_t*>(base+off);
    __asm__ volatile("dsb sy" ::: "memory");
    return v;
  }
};
// Only source-backed read-only windows. No queue, clock, SMMU or disk writes.
struct LmiproHostState {
  uint32_t mapped, pwr, axi, ahb, uni, cap, ver, hcs, hce, db, rs, lo, hi;
};
static LmiproHostState lmiproHostState(void) {
  LmiproHostState s = {};
  IODeviceMemory *m = IODeviceMemory::withRange(0x00177000ULL, 0x1000);
  if (!m) return s;
  OSSharedPtr<IOMemoryMap> gcc(m->map(kIOMapAnywhere|kIOMapInhibitCache),OSNoRetain);
  m->release();
  if (!gcc) return s;
  auto read = [](uintptr_t base, unsigned off) {
    uint32_t v=*reinterpret_cast<const volatile uint32_t*>(base+off);
    __asm__ volatile("dsb sy" ::: "memory");
    return v;
  };
  uintptr_t g=static_cast<uintptr_t>(gcc->getVirtualAddress());
  s.mapped=1; s.pwr=read(g,4); s.axi=read(g,0x10);
  s.ahb=read(g,0x18); s.uni=read(g,0x5c);
  // Enable bits are authoritative under HW clock gating; never probe a powered-off host.
  if (!(s.pwr & 0x80000000U) || !(s.axi & 1) || !(s.ahb & 1)) return s;
  m=IODeviceMemory::withRange(0x01d84000ULL,0x1000);
  if (!m) return s;
  OSSharedPtr<IOMemoryMap> host(m->map(kIOMapAnywhere|kIOMapInhibitCache),OSNoRetain);
  m->release();
  if (!host) return s;
  uintptr_t h=static_cast<uintptr_t>(host->getVirtualAddress());
  s.mapped=2; s.cap=read(h,0); s.ver=read(h,8); s.hcs=read(h,0x30);
  s.hce=read(h,0x34); s.db=read(h,0x58); s.rs=read(h,0x60);
  s.lo=read(h,0x50); s.hi=read(h,0x54);
  return s;
}
extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode=0;
  if (!PE_parse_boot_argn("lmipro-ufs-handoff",&mode,sizeof(mode))) return;
  if (mode!=25) panic("LMIPRO SMMU RO2: only mode 25 enabled; mode=%u",mode);
  IODeviceMemory* mem=IODeviceMemory::withRange(0x15000000ULL,0x100000);
  if (!mem) panic("LMIPRO SMMU RO2: descriptor failed");
  OSSharedPtr<IOMemoryMap> keep(mem->map(kIOMapAnywhere|kIOMapInhibitCache),OSNoRetain);
  mem->release();
  if (!keep) panic("LMIPRO SMMU RO2: mapping failed");
  LmiproReadOnlyMmio io={static_cast<uintptr_t>(keep->getVirtualAddress())};
  LmiproSmmuResult r=lmiproSmmuAudit(io);
  LmiproHostState h=lmiproHostState();
  const LmiproStream& a=r.stream[0]; const LmiproStream& b=r.stream[1];
  lmipro_ufs_handoff_observation[0]=r.stage;
  lmipro_ufs_handoff_observation[1]=r.scr0;
  lmipro_ufs_handoff_observation[2]=r.id0;
  lmipro_ufs_handoff_observation[3]=r.id1;
  lmipro_ufs_handoff_observation[4]=r.gfsr;
  panic("LMIPRO STORAGE PRECHECK v1 (reads only)\n"
        "host=%u pwr=%08x axi=%08x ahb=%08x uni=%08x\n"
        "cap=%08x ver=%08x hcs=%08x hce=%08x\n"
        "doorbell=%08x run=%08x list=%08x:%08x\n"
        "stage=%u cr0=%08x id0=%08x\nid1=%08x id2=%08x ext=%u\n"
        "groups=%u scan=%u valid=%u nz=%u\n"
        "T/B/F/R=%u/%u/%u/%u allff=%u\n"
        "hash=%08x/%08x changed=%u gchg=%x\n"
        "e0=%u 4e0=%u (candidate SIDs)\n"
        "sample count=%u (unused rows zero)\n"
        "%u: %08x %08x\n%u: %08x %08x\n"
        "%u: %08x %08x\n%u: %08x %08x\n"
        "gfsr=%08x syn=%08x/%08x/%08x\n"
        "No DMA or disk-read qualification",
        h.mapped,h.pwr,h.axi,h.ahb,h.uni,h.cap,h.ver,h.hcs,h.hce,h.db,h.rs,h.hi,h.lo,
        r.stage,r.scr0,r.id0,r.id1,r.id2,r.extended,r.groups,r.scanned,r.valid,r.nonzero,
        r.types[0],r.types[1],r.types[2],r.types[3],r.allones,
        r.hash,r.hash2,r.changed,r.globalChanged,a.matches,b.matches,r.sampleCount,
        r.sampleIndex[0],r.sampleSmr[0],r.sampleS2cr[0],
        r.sampleIndex[1],r.sampleSmr[1],r.sampleS2cr[1],
        r.sampleIndex[2],r.sampleSmr[2],r.sampleS2cr[2],
        r.sampleIndex[3],r.sampleSmr[3],r.sampleS2cr[3],
        r.gfsr,r.syn0,r.syn1,r.syn2);
}
#endif
