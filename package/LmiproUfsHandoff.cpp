// SPDX-License-Identifier: BSD-2-Clause-Patent
// Read-only SM8250 firmware handoff observation. No request submission or MMIO writes.
// Mapping idiom: UniXNU/ShengUfs e2b32cc429e8cf1501161c1d8c517f8994ceb9b2.
// Register facts: Linux v6.12 gcc-sm8250.c and sm8250.dtsi, confirmed board DT windows.
#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>
#include <kern/debug.h>

extern "C" {
volatile uint32_t lmipro_ufs_handoff_observation[24];
}

static uintptr_t mapWindow(uint64_t phys, uint64_t length, OSSharedPtr<IOMemoryMap>& keep)
{
  IODeviceMemory* memory = IODeviceMemory::withRange(phys, length);
  if (!memory) return 0;
  keep = OSSharedPtr<IOMemoryMap>(memory->map(kIOMapAnywhere | kIOMapInhibitCache), OSNoRetain);
  memory->release();
  return keep ? static_cast<uintptr_t>(keep->getVirtualAddress()) : 0;
}

static uint32_t readWord(uintptr_t base, unsigned offset)
{
  return *reinterpret_cast<const volatile uint32_t*>(base + offset);
}

static bool branchAvailable(uint32_t value)
{
  // CLK_OFF can be asserted while a branch with HWCG bit 1 is idle.
  return (value & 1U) && (!(value & 0x80000000U) || (value & 2U));
}

extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode = 0;
  if (!PE_parse_boot_argn("lmipro-ufs-handoff", &mode, sizeof(mode)) || mode != 1) return;
  OSSharedPtr<IOMemoryMap> gccMap, hcMap, smmuMap;
  // GCC + 0x77000 covers only the known SM8250 UFS clock branch page.
  uintptr_t gcc = mapWindow(0x00177000ULL, 0x1000, gccMap);
  if (!gcc) panic("LMIPRO UFS RO: GCC mapping failed; no hardware writes");
  uint32_t pwr = readWord(gcc, 0x04);
  uint32_t axi = readWord(gcc, 0x10);
  uint32_t ahb = readWord(gcc, 0x18);
  uint32_t uni = readWord(gcc, 0x5c);
  uint32_t cap=0, ver=0, hcs=0, hce=0, db=0, rs=0, listlo=0, listhi=0, hcRead=0;
  if ((pwr & 0x80000000U) && branchAvailable(axi) && branchAvailable(ahb)) {
    uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
    if (hc) {
      cap=readWord(hc,0x00); ver=readWord(hc,0x08);
      hcs=readWord(hc,0x30); hce=readWord(hc,0x34);
      listlo=readWord(hc,0x50); listhi=readWord(hc,0x54);
      db=readWord(hc,0x58); rs=readWord(hc,0x60); hcRead=1;
    }
  }
  uint32_t scr=0, idr=0, match[2]={0,0}, smr[2]={0,0}, s2cr[2]={0,0};
  // Only SMMUv2 global registers are inspected; no context, stream or mapping is changed.
  uintptr_t smmu = mapWindow(0x15000000ULL, 0x1000, smmuMap);
  if (smmu) {
    scr=readWord(smmu,0); idr=readWord(smmu,0x20);
    if (idr && idr != 0xffffffffU) {
      unsigned count=idr & 0xffU;
      if (count>128) count=128;
      for (unsigned i=0;i<count;i++) {
        uint32_t value=readWord(smmu,0x800+4*i);
        if (!(value & 0x80000000U)) continue;
        uint32_t mask=(value >> 16) & 0x7fffU;
        const uint32_t sid[2]={0x0e0,0x4e0};
        for (unsigned k=0;k<2;k++) {
          if (((value ^ sid[k]) & ~mask & 0x7fffU) != 0) continue;
          match[k]++;
          if (match[k]==1) { smr[k]=value; s2cr[k]=readWord(smmu,0xc00+4*i); }
        }
      }
    }
  }
  const uint32_t values[24]={1,pwr,axi,ahb,uni,hcRead,cap,ver,hcs,hce,db,rs,listlo,listhi,
                            scr,idr,match[0],smr[0],s2cr[0],match[1],smr[1],s2cr[1],0,0};
  for (unsigned i=0;i<24;i++) lmipro_ufs_handoff_observation[i]=values[i];
  panic("LMIPRO UFS RO (no writes)\n"
        "pwr=%08x axi=%08x ahb=%08x uni=%08x\n"
        "hc=%u cap=%08x ver=%08x\n"
        "hcs=%08x hce=%08x db=%08x rs=%08x\n"
        "list=%08x:%08x smmu=%08x idr=%08x\n"
        "e0 n=%u smr=%08x s2cr=%08x\n"
        "4e0 n=%u smr=%08x s2cr=%08x",
        pwr,axi,ahb,uni,hcRead,cap,ver,hcs,hce,db,rs,listhi,listlo,scr,idr,
        match[0],smr[0],s2cr[0],match[1],smr[1],s2cr[1]);
}
