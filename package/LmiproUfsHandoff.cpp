// SPDX-License-Identifier: BSD-2-Clause-Patent
// SM8250 UFS handoff: a read-only register observation (mode 1) plus a bounded
// one-command adoption of the running controller (mode 2/3). The CDB set is
// read-class only; no WRITE command exists in this file.
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
volatile uint32_t lmipro_ufs_read_result[16];
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

// mode 23: read back the block the firmware staged at the tail of its payload
// ramdisk window. Proves whether the vendor UFS driver could read the device
// before handoff, independently of the kernel's own controller state.
extern "C" void lmipro_ufs_blob_probe(void)
{
  OSSharedPtr<IOMemoryMap> blobMap;
  uintptr_t blob = mapWindow(0x097200000ULL, 0x2000, blobMap);
  if (!blob) panic("LMIPRO UFS BLOB: map failed");
  const volatile uint32_t *words = reinterpret_cast<const volatile uint32_t *>(blob);
  uint32_t magic   = words[0];
  uint32_t version = words[1];
  uint32_t gpt0    = readWord(blob, 8 + 0x200);
  uint32_t gpt1    = readWord(blob, 8 + 0x204);
  panic("LMIPRO UFS BLOB\nmagic=%08x version=%08x\ngpt=%08x %08x\n"
        "mbr=%08x %08x %08x %08x",
        magic, version, gpt0, gpt1,
        words[2], words[3], words[4], words[5]);
}

extern "C" void lmipro_ufs_read_probe(bool firmwareWindow);
extern "C" void lmipro_ufs_alloc_probe(void);
extern "C" void lmipro_ufs_window_probe(void);
extern "C" void lmipro_ufs_state_probe(void);
extern "C" void lmipro_ufs_uic_probe(void);
extern "C" void lmipro_ufs_wake_probe(void);
extern "C" void lmipro_ufs_clock_probe(void);
extern "C" void lmipro_ufs_reset_probe(void);
extern "C" void lmipro_ufs_blob_probe(void);
extern "C" void lmipro_ufs_uic2_probe(void);
extern "C" void lmipro_ufs_wake2_probe(void);
extern "C" void lmipro_ufs_pwr_probe(void);
extern "C" void lmipro_ufs_phy_probe(void);
extern "C" void lmipro_ufs_phyinit_probe(void);

// 0: commands, 1: stop/restart only, 2: stop/restart + wake + TUR.
static unsigned gReadLevel = 0;
static uint32_t gDescDw0 = 0x10000000U;
static bool gHceReset = false;

extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode = 0;
  bool parsed = PE_parse_boot_argn("lmipro-ufs-handoff", &mode, sizeof(mode));
  // mode 1: read-only register observation, no writes.
  // mode 2: full read on kernel-allocated contiguous DMA.
  // mode 3: full read on the firmware's own UTRLBA window.
  // mode 4: contiguous DMA allocation only.
  // mode 5: allocate, stop and restart the request list, then report.
  // mode 6: allocate, stop/restart, wake the device and TUR, then report.
  // mode 8: read the first words of the firmware UTRLBA window, write nothing.
  // mode 12: submit one NOP OUT, which the controller completes without the device.
  if (parsed) {
    if (mode == 4) { lmipro_ufs_alloc_probe(); return; }
    if (mode == 8) { lmipro_ufs_window_probe(); return; }
    if (mode == 15) { lmipro_ufs_state_probe(); return; }
    if (mode == 17) { lmipro_ufs_uic_probe(); return; }
    if (mode == 18) { lmipro_ufs_wake_probe(); return; }
    if (mode == 21) { lmipro_ufs_clock_probe(); return; }
    if (mode == 22) { lmipro_ufs_reset_probe(); return; }
    if (mode == 23) { lmipro_ufs_blob_probe(); return; }
    if (mode == 24) { lmipro_ufs_uic2_probe(); return; }
    if (mode == 25) { gDescDw0 = 0x01000000U; gReadLevel = 7; lmipro_ufs_read_probe(true); return; }
    if (mode == 26) { gDescDw0 = 0x01000000U; gReadLevel = 7; lmipro_ufs_read_probe(false); return; }
    if (mode == 27) { lmipro_ufs_wake2_probe(); return; }
    if (mode == 28) { gDescDw0 = 0x10000000U; gHceReset = true; gReadLevel = 7; lmipro_ufs_read_probe(false); return; }
    if (mode == 29) { lmipro_ufs_pwr_probe(); return; }
    if (mode == 30) { lmipro_ufs_phy_probe(); return; }
    if (mode == 31) { lmipro_ufs_phyinit_probe(); return; }
    if (mode == 12) { gReadLevel = 6; lmipro_ufs_read_probe(true); return; }
    if (mode == 13) { gReadLevel = 7; lmipro_ufs_read_probe(true); return; }
    if (mode == 9 || mode == 10 || mode == 11) {
      gReadLevel = (mode == 9) ? 3 : (mode == 10) ? 4 : 5;
      lmipro_ufs_read_probe(true);
      return;
    }
    if (mode == 2 || mode == 3 || mode == 5 || mode == 6) {
      gReadLevel = (mode == 5) ? 1 : (mode == 6) ? 2 : 0;
      lmipro_ufs_read_probe(mode == 3);
      return;
    }
  }
  if (!parsed || mode != 1) return;
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

// ---------------------------------------------------------------------------
// mode 2: one READ(10) through the firmware's own transfer request list.
// Read-only by construction: the CDB set is TUR(0x00), START STOP UNIT(0x1b),
// READ CAPACITY(0x25) and READ(10)(0x28). No WRITE CDB exists in this file.
// The queue and its DMA window are the firmware's own UTRLBA pages, the only
// region this controller's DMA is known to reach on this board.
// ---------------------------------------------------------------------------

static inline void writeWord(uintptr_t base, unsigned offset, uint32_t value)
{
  *reinterpret_cast<volatile uint32_t*>(base + offset) = value;
  __asm__ volatile("dsb sy" ::: "memory");
}

static void cacheCleanRange(const void* start, size_t length)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(start) & ~uintptr_t(63);
  uintptr_t end = reinterpret_cast<uintptr_t>(start) + length;
  for (; address < end; address += 64) __asm__ volatile("dc cvac, %0" :: "r"(address) : "memory");
  __asm__ volatile("dsb sy" ::: "memory");
}

static void cacheInvalidateRange(const void* start, size_t length)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(start) & ~uintptr_t(63);
  uintptr_t end = reinterpret_cast<uintptr_t>(start) + length;
  for (; address < end; address += 64) __asm__ volatile("dc civac, %0" :: "r"(address) : "memory");
  __asm__ volatile("dsb sy" ::: "memory");
}

static void zeroRange(void* start, size_t length)
{
  volatile uint32_t* word = static_cast<volatile uint32_t*>(start);
  for (size_t index = 0; index < length / 4; index++) word[index] = 0;
}

static void putBe32(uint8_t* at, uint32_t value)
{
  at[0] = uint8_t(value >> 24); at[1] = uint8_t(value >> 16);
  at[2] = uint8_t(value >> 8); at[3] = uint8_t(value);
}

static uint32_t getBe32(const uint8_t* at)
{
  return (uint32_t(at[0]) << 24) | (uint32_t(at[1]) << 16) | (uint32_t(at[2]) << 8) | at[3];
}

static uint32_t getLe32(const uint8_t* at)
{
  return uint32_t(at[0]) | (uint32_t(at[1]) << 8) | (uint32_t(at[2]) << 16) | (uint32_t(at[3]) << 24);
}

static uint64_t getLe64(const uint8_t* at)
{
  return uint64_t(getLe32(at)) | (uint64_t(getLe32(at + 4)) << 32);
}

struct LmiproUfsReply {
  uint32_t ocs = 0xf, response = 0xff, scsi = 0xff;
};

static inline uint32_t replyTriplet(const LmiproUfsReply& reply)
{
  return (reply.ocs << 16) | (reply.response << 8) | reply.scsi;
}

// mode 4: prove a physically contiguous kernel allocation is writable and that
// its physical address is usable, without touching the controller at all.
extern "C" void lmipro_ufs_alloc_probe(void)
{
  IOPhysicalAddress dmaPhys = 0;
  uint8_t* virt = nullptr;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  virt = static_cast<uint8_t*>(IOMallocContiguous(16384, 16384, &dmaPhys));
#pragma clang diagnostic pop
  if (!virt) panic("LMIPRO UFS ALLOC: allocation failed");
  zeroRange(virt, 16384);
  volatile uint32_t* words = reinterpret_cast<volatile uint32_t*>(virt);
  for (unsigned index = 0; index < 4096; index++) words[index] = 0xa5a50000U | index;
  cacheCleanRange(virt, 16384);
  cacheInvalidateRange(virt, 16384);
  unsigned mismatch = 0;
  for (unsigned index = 0; index < 4096; index++) {
    if (words[index] != (0xa5a50000U | index)) mismatch++;
  }
  panic("LMIPRO UFS ALLOC\nphys=%llx low=%08x mismatch=%u\nfirst=%08x last=%08x",
        static_cast<uint64_t>(dmaPhys), static_cast<uint32_t>(dmaPhys), mismatch,
        words[0], words[4095]);
}

// mode 8: read the first words of the firmware UTRLBA window without writing.
// Tells whether the queue memory the firmware left behind is still intact or
// has been reused by the kernel.
extern "C" void lmipro_ufs_window_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap, regionMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UFS WINDOW: controller map failed");
  uint64_t phys = (uint64_t(readWord(hc, 0x54)) << 32) | readWord(hc, 0x50);
  if (!phys) panic("LMIPRO UFS WINDOW: no UTRLBA");
  IOMemoryDescriptor* region = IOMemoryDescriptor::withPhysicalAddress(phys, 16384, kIODirectionIn);
  if (region) {
    regionMap = OSSharedPtr<IOMemoryMap>(region->map(kIOMapAnywhere), OSNoRetain);
    region->release();
  }
  if (!regionMap) panic("LMIPRO UFS WINDOW: window map failed %llx", phys);
  const volatile uint32_t* words = reinterpret_cast<const volatile uint32_t*>(regionMap->getVirtualAddress());
  panic("LMIPRO UFS WINDOW\nphys=%llx runstop=%08x hcs=%08x\n"
        "%08x %08x %08x %08x\n%08x %08x %08x %08x",
        phys, readWord(hc, 0x60), readWord(hc, 0x30),
        words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7]);
}

// mode 15: read-only snapshot of the apps-SMMU globals and the UFS controller
// state the firmware left behind. No write, no command, no doorbell.
extern "C" void lmipro_ufs_state_probe(void)
{
  OSSharedPtr<IOMemoryMap> smmuMap, hcMap;
  uintptr_t smmu = mapWindow(0x15000000ULL, 0x100000, smmuMap);
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!smmu || !hc) panic("LMIPRO UFS STATE: map failed smmu=%lx hc=%lx", smmu, hc);
  panic("LMIPRO UFS STATE\n"
        "smmu scr0=%08x scr2=%08x gfsr=%08x\n"
        "idr0=%08x cbar0=%08x s2cr0=%08x cb0sctlr=%08x\n"
        "ufs hcs=%08x is=%08x hce=%08x\n"
        "utrlba=%08x rsr=%08x uecpa=%08x uecda=%08x\n"
        "uiccmd=%08x arg1=%08x arg2=%08x",
        readWord(smmu, 0x00), readWord(smmu, 0x08), readWord(smmu, 0x60),
        readWord(smmu, 0x20), readWord(smmu, 0x1000), readWord(smmu, 0xc00), readWord(smmu, 0x2000),
        readWord(hc, 0x30), readWord(hc, 0x20), readWord(hc, 0x34),
        readWord(hc, 0x50), readWord(hc, 0x60), readWord(hc, 0x38), readWord(hc, 0x3c),
        readWord(hc, 0x74), readWord(hc, 0x78), readWord(hc, 0x7c));
}

// mode 17: exercise the UIC path only. DME_GET reads one MIB attribute from the
// device over the UniPro link; it never touches the UTRD, the doorbell or the
// data path. A timeout means the link does not answer at all.
extern "C" void lmipro_ufs_uic_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UFS UIC: map failed");
  writeWord(hc, 0x24, 0);
  writeWord(hc, 0x20, 0xffffffffU);   // clear pending UIC completion
  // UIC_ARG_1 carries (attribute << 16) | selector. The plain attribute value
  // selects an invalid MIB and reads back zero, which is what made the link
  // look dead in the earlier rounds.
  writeWord(hc, 0x94, 0x00001560U << 16);   // PA_ActiveTxDataLanes
  writeWord(hc, 0x98, 0);
  writeWord(hc, 0x9c, 0);
  writeWord(hc, 0x90, 0x00000001U);   // UIC_CMD_DME_GET
  bool done = false;
  for (unsigned index = 0; index < 200000; index++) {
    if (readWord(hc, 0x20) & 0x400U) { done = true; break; }
    IODelay(1);
  }
  panic("LMIPRO UFS UIC\ndone=%u is=%08x arg2=%08x arg3=%08x hcs=%08x",
        done ? 1U : 0U, readWord(hc, 0x20), readWord(hc, 0x98), readWord(hc, 0x9c), readWord(hc, 0x30));
}

// mode 24: same UIC read as mode 17 but with the corrected argument encoding
// (attribute << 16). Reproduces the lane count the Codex probe reported, and
// samples the receive lane count and power mode for context.
extern "C" void lmipro_ufs_uic2_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UIC2: map failed");
  writeWord(hc, 0x24, 0);
  auto uic = [&](uint32_t attr, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, attr << 16);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t lane = uic(0x00001560U, 0x01);   // PA_ActiveTxDataLanes
  uint32_t rx   = uic(0x00001580U, 0x01);   // PA_ActiveRxDataLanes
  uint32_t pwr  = uic(0x00001571U, 0x01);   // PA_PWRMODE
  panic("LMIPRO UIC2\nlane=%08x rx=%08x pwr=%08x\nis=%08x hcs=%08x arg2=%08x",
        lane, rx, pwr, readWord(hc, 0x20), readWord(hc, 0x30), readWord(hc, 0x98));
}

// mode 27: wake sequence with the corrected UIC argument encoding. Every
// earlier wake attempt passed the bare attribute value, which the controller
// treats as an invalid MIB, so the link was never actually asked to leave
// hibernate.
extern "C" void lmipro_ufs_wake2_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO WAKE2: map failed");
  writeWord(hc, 0x24, 0);
  auto uic = [&](uint32_t arg1, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, arg1);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t laneBefore = uic(0x00001560U << 16, 0x01);   // DME_GET PA_ActiveTxDataLanes
  uint32_t hibernate  = uic(0x00000000U, 0x18);         // DME_HIBERNATE_EXIT
  for (unsigned settle = 0; settle < 20; settle++) IODelay(1000);
  uint32_t laneAfter  = uic(0x00001560U << 16, 0x01);
  uint32_t startup    = uic(0x00000000U, 0x16);         // DME_LINK_STARTUP
  for (unsigned settle = 0; settle < 20; settle++) IODelay(1000);
  uint32_t laneFinal  = uic(0x00001560U << 16, 0x01);
  panic("LMIPRO WAKE2\n"
        "laneBefore=%08x hibernate=%08x laneAfter=%08x\n"
        "startup=%08x laneFinal=%08x\n"
        "is=%08x hcs=%08x hce=%08x",
        laneBefore, hibernate, laneAfter, startup, laneFinal,
        readWord(hc, 0x20), readWord(hc, 0x30), readWord(hc, 0x34));
}

// mode 29: read PA_PWRMODE before and after the wake commands. A value stuck at
// the PWM/hibernate encoding means the link never entered high-speed active
// state, which would explain why a UTP command tears the controller down.
extern "C" void lmipro_ufs_pwr_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO PWR: map failed");
  writeWord(hc, 0x24, 0);
  auto uic = [&](uint32_t arg1, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, arg1);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t pwrBefore = uic(0x00001571U << 16, 0x01);   // DME_GET PA_PWRMODE
  uint32_t laneBefore = uic(0x00001560U << 16, 0x01);
  uint32_t hibernate  = uic(0x00000000U, 0x18);         // DME_HIBERNATE_EXIT
  for (unsigned settle = 0; settle < 20; settle++) IODelay(1000);
  uint32_t pwrAfter   = uic(0x00001571U << 16, 0x01);
  uint32_t laneAfter  = uic(0x00001560U << 16, 0x01);
  panic("LMIPRO PWR\n"
        "pwrBefore=%08x laneBefore=%08x hibernate=%08x\n"
        "pwrAfter=%08x laneAfter=%08x\n"
        "is=%08x hcs=%08x hce=%08x",
        pwrBefore, laneBefore, hibernate, pwrAfter, laneAfter,
        readWord(hc, 0x20), readWord(hc, 0x30), readWord(hc, 0x34));
}

// mode 30: PHY power-up attempt. Linux keeps the QMP UFS PCS at PHY+0xc00 with
// POWER_DOWN_CONTROL at +0x004, PHY_START at +0x008 and READY_STATUS at +0x00c.
// Bring the PHY out of power-down, start it, wait for ready, then re-read the
// link state to see whether the device responds.
extern "C" void lmipro_ufs_phy_probe(void)
{
  OSSharedPtr<IOMemoryMap> phyMap, hcMap;
  uintptr_t phy = mapWindow(0x01d87000ULL, 0x2000, phyMap);
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!phy || !hc) panic("LMIPRO PHY: map failed");
  writeWord(phy, 0xc04, 0x00);
  IODelay(1000);
  writeWord(phy, 0xc08, 0x01);
  uint32_t ready = 0;
  for (unsigned index = 0; index < 20000; index++) {
    ready = readWord(phy, 0xc0c);
    if (ready & 0x01U) break;
    IODelay(100);
  }
  writeWord(hc, 0x24, 0);
  auto uic = [&](uint32_t arg1, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, arg1);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t startup = uic(0x00000000U, 0x16);
  for (unsigned settle = 0; settle < 20; settle++) IODelay(1000);
  uint32_t pwr = uic(0x00001571U << 16, 0x01);
  uint32_t lane = uic(0x00001560U << 16, 0x01);
  panic("LMIPRO PHY\nready=%08x startup=%08x\npwr=%08x lane=%08x\n"
        "pcs004=%08x pcs008=%08x pcs00c=%08x is=%08x hcs=%08x",
        ready, startup, pwr, lane,
        readWord(phy, 0xc04), readWord(phy, 0xc08), readWord(phy, 0xc0c),
        readWord(hc, 0x20), readWord(hc, 0x30));
}


// mode 31: full QMP UFS PHY bring-up using the Linux sm8150_ufsphy tables
// (SM8250 reuses them). The PHY is programmed while still powered down, then
// started, and only then is the link asked to start.
extern "C" void lmipro_ufs_phyinit_probe(void)
{
  static const uint32_t kPhyTable[][2] = {
  {0x0294, 0xd9},   // serdes
  {0x0358, 0x11},   // serdes
  {0x035c, 0x00},   // serdes
  {0x02a4, 0x01},   // serdes
  {0x030c, 0x02},   // serdes
  {0x0258, 0x0f},   // serdes
  {0x0324, 0x00},   // serdes
  {0x03bc, 0x11},   // serdes
  {0x02bc, 0x82},   // serdes
  {0x0274, 0x06},   // serdes
  {0x027c, 0x16},   // serdes
  {0x0284, 0x36},   // serdes
  {0x02ac, 0xff},   // serdes
  {0x02b0, 0x0c},   // serdes
  {0x03ac, 0xac},   // serdes
  {0x03b0, 0x1e},   // serdes
  {0x02c4, 0x98},   // serdes
  {0x0278, 0x06},   // serdes
  {0x0280, 0x16},   // serdes
  {0x0288, 0x36},   // serdes
  {0x02b4, 0x32},   // serdes
  {0x02b8, 0x0f},   // serdes
  {0x03b4, 0xdd},   // serdes
  {0x03b8, 0x23},   // serdes
  {0x00d8, 0x06},   // tx
  {0x00dc, 0x03},   // tx
  {0x00e0, 0x01},   // tx
  {0x00e4, 0x00},   // tx
  {0x0084, 0x05},   // tx
  {0x00b8, 0x0c},   // tx
  {0x0520, 0x24},   // rx
  {0x051c, 0x0f},   // rx
  {0x0524, 0x1e},   // rx
  {0x0528, 0x18},   // rx
  {0x0430, 0x0a},   // rx
  {0x0434, 0x4b},   // rx
  {0x0444, 0xf1},   // rx
  {0x043c, 0x80},   // rx
  {0x0448, 0x80},   // rx
  {0x0408, 0x0c},   // rx
  {0x0414, 0x04},   // rx
  {0x0480, 0x1b},   // rx
  {0x04ec, 0x06},   // rx
  {0x04f0, 0x04},   // rx
  {0x04f4, 0x1d},   // rx
  {0x0514, 0x00},   // rx
  {0x0500, 0x10},   // rx
  {0x04f8, 0xc0},   // rx
  {0x04fc, 0x00},   // rx
  {0x0570, 0x36},   // rx
  {0x0574, 0x36},   // rx
  {0x0578, 0xf6},   // rx
  {0x057c, 0x3b},   // rx
  {0x0580, 0x3d},   // rx
  {0x0584, 0xe0},   // rx
  {0x0588, 0xc8},   // rx
  {0x058c, 0xc8},   // rx
  {0x0590, 0x3b},   // rx
  {0x0594, 0xb1},   // rx
  {0x0598, 0xe0},   // rx
  {0x059c, 0xc8},   // rx
  {0x05a0, 0xc8},   // rx
  {0x05a4, 0x3b},   // rx
  {0x05a8, 0xb1},   // rx
  {0x0d58, 0x6d},   // pcs
  {0x0c30, 0x0a},   // pcs
  {0x0c38, 0x02},   // pcs
  {0x0dd8, 0x43},   // pcs
  {0x0d24, 0x1f},   // pcs
  {0x0d50, 0xff},   // pcs
  {0x0de0, 0x02},   // pcs
  };
  OSSharedPtr<IOMemoryMap> phyMap, hcMap;
  uintptr_t phy = mapWindow(0x01d87000ULL, 0x2000, phyMap);
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!phy || !hc) panic("LMIPRO PHYINIT: map failed");
  // Hold the PHY in power-down while its tables are programmed.
  writeWord(phy, 0xc04, 0x01);
  IODelay(1000);
  for (unsigned index = 0; index < sizeof(kPhyTable) / sizeof(kPhyTable[0]); index++) {
    writeWord(phy, kPhyTable[index][0], kPhyTable[index][1]);
  }
  writeWord(phy, 0xc04, 0x00);            // power on
  IODelay(100);
  writeWord(phy, 0xc08, 0x01);            // PHY_START
  uint32_t ready = 0;
  for (unsigned index = 0; index < 20000; index++) {
    ready = readWord(phy, 0xc0c);
    if ((ready & 0x0cU) == 0x0cU) break;  // PLL + ready latched
    IODelay(100);
  }
  writeWord(hc, 0x24, 0);
  auto uic = [&](uint32_t arg1, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, arg1);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t startup = uic(0x00000000U, 0x16);
  for (unsigned settle = 0; settle < 30; settle++) IODelay(1000);
  uint32_t pwr = uic(0x00001571U << 16, 0x01);
  uint32_t lane = uic(0x00001560U << 16, 0x01);
  panic("LMIPRO PHYINIT\nready=%08x startup=%08x\npwr=%08x lane=%08x\n"
        "pcs004=%08x pcs008=%08x pcs00c=%08x is=%08x hcs=%08x",
        ready, startup, pwr, lane,
        readWord(phy, 0xc04), readWord(phy, 0xc08), readWord(phy, 0xc0c),
        readWord(hc, 0x20), readWord(hc, 0x30));
}

// mode 18: wake the UniPro link through UIC only. Reads the active lane count,
// issues DME_HIBERNATE_EXIT and reads the lane count again, so a reset here is
// attributable to the link wake-up itself rather than to any UTP command.
extern "C" void lmipro_ufs_wake_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UFS WAKE: map failed");
  writeWord(hc, 0x24, 0);
  auto uic = [&](uint32_t arg1, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, arg1);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t laneBefore = uic(0x00001560U, 0x01);   // DME_GET PA_ActiveTxDataLanes
  uint32_t hibernate = uic(0x00000000U, 0x18);    // DME_HIBERNATE_EXIT
  uint32_t laneAfter = uic(0x00001560U, 0x01);
  uint32_t startup = 0, laneFinal = 0, isFinal = 0;
  if (laneAfter == 0) {
    startup = uic(0x00000000U, 0x16);             // DME_LINK_STARTUP
    for (unsigned settle = 0; settle < 20; settle++) IODelay(1000);
    laneFinal = uic(0x00001560U, 0x01);
    isFinal = readWord(hc, 0x20);
  }
  panic("LMIPRO UFS WAKE\n"
        "laneBefore=%08x hibernate=%08x laneAfter=%08x\n"
        "startup=%08x laneFinal=%08x isFinal=%08x\n"
        "is=%08x hcs=%08x hce=%08x",
        laneBefore, hibernate, laneAfter, startup, laneFinal, isFinal,
        readWord(hc, 0x20), readWord(hc, 0x30), readWord(hc, 0x34));
}

// mode 21: read-only snapshot of the GCC branches that feed the UFS PHY. A
// CLK_OFF/haltime bit here means the firmware gated the PHY clocks, which the
// XNU side could in principle re-enable; a dead GDSC would not be recoverable.
extern "C" void lmipro_ufs_clock_probe(void)
{
  OSSharedPtr<IOMemoryMap> gccMap;
  uintptr_t gcc = mapWindow(0x00177000ULL, 0x1000, gccMap);
  if (!gcc) panic("LMIPRO UFS CLK: map failed");
  panic("LMIPRO UFS CLK\ngdsc=%08x\n"
        "axi=%08x ahb=%08x uni=%08x\n"
        "tx=%08x rx0=%08x rx1=%08x aux=%08x\n"
        "ice=%08x",
        readWord(gcc, 0x04),
        readWord(gcc, 0x10), readWord(gcc, 0x18), readWord(gcc, 0x5c),
        readWord(gcc, 0x1c), readWord(gcc, 0x20), readWord(gcc, 0xb8), readWord(gcc, 0x9c),
        readWord(gcc, 0x64));
}

// mode 22: last in-kernel attempt. Disable auto-hibernate, cycle the controller
// enable bit (HCE 0 -> 1) and run one link startup, then read the lane count.
// This is the ufshcd_hba_enable path reduced to its essentials.
extern "C" void lmipro_ufs_reset_probe(void)
{
  OSSharedPtr<IOMemoryMap> hcMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UFS RESET: map failed");
  writeWord(hc, 0x24, 0);
  writeWord(hc, 0x18, 0);            // auto-hibernate timer: off
  writeWord(hc, 0x34, 0);            // HCE off
  for (unsigned index = 0; index < 20000; index++) {
    if (!(readWord(hc, 0x34) & 1U)) break;
    IODelay(1);
  }
  uint32_t hceOff = readWord(hc, 0x34);
  writeWord(hc, 0x34, 1);            // HCE on
  bool hceUp = false;
  for (unsigned index = 0; index < 200000; index++) {
    if (readWord(hc, 0x34) & 1U) { hceUp = true; break; }
    IODelay(1);
  }
  auto uic = [&](uint32_t arg1, uint32_t opcode) -> uint32_t {
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x94, arg1);
    writeWord(hc, 0x98, 0);
    writeWord(hc, 0x9c, 0);
    writeWord(hc, 0x90, opcode);
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x20) & 0x400U) break;
      IODelay(1);
    }
    return readWord(hc, 0x9c);
  };
  uint32_t startup = uic(0x00000000U, 0x16);   // DME_LINK_STARTUP
  for (unsigned settle = 0; settle < 20; settle++) IODelay(1000);
  uint32_t lane = uic(0x00001560U, 0x01);
  panic("LMIPRO UFS RESET\nhceOff=%08x hceUp=%u startup=%08x lane=%08x\nis=%08x hcs=%08x",
        hceOff, hceUp ? 1U : 0U, startup, lane, readWord(hc, 0x20), readWord(hc, 0x30));
}


extern "C" void lmipro_ufs_read_probe(bool firmwareWindow)
{
  OSSharedPtr<IOMemoryMap> hcMap, regionMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UFS READ: controller map failed");
  if (gHceReset) {
    // Linux programs the request list while the controller is disabled, then
    // enables it. Reproduce that order instead of editing a live queue.
    writeWord(hc, 0x34, 0);
    for (unsigned index = 0; index < 20000; index++) {
      if (!(readWord(hc, 0x34) & 1U)) break;
      IODelay(1);
    }
    writeWord(hc, 0x34, 1);
    bool hceUp = false;
    for (unsigned index = 0; index < 200000; index++) {
      if (readWord(hc, 0x34) & 1U) { hceUp = true; break; }
      IODelay(1);
    }
    if (!hceUp) panic("LMIPRO UFS READ: HCE did not come back");
  }
  uint64_t phys = 0;
  uint8_t* virt = nullptr;
  if (firmwareWindow) {
    // The firmware's own list window. Known to be DMA-reachable, but neither the
    // live FDT nor firmware.dtb reserves it, so ownership is only probable.
    phys = (uint64_t(readWord(hc, 0x54)) << 32) | readWord(hc, 0x50);
    if (!phys || (phys & 0xfff)) panic("LMIPRO UFS READ: unusable UTRLBA %llx", phys);
    IOMemoryDescriptor* region = IOMemoryDescriptor::withPhysicalAddress(phys, 16384, kIODirectionInOut);
    if (region) {
      regionMap = OSSharedPtr<IOMemoryMap>(region->map(kIOMapAnywhere), OSNoRetain);
      region->release();
    }
    if (!regionMap) panic("LMIPRO UFS READ: DMA window map failed base=%llx", phys);
    virt = reinterpret_cast<uint8_t*>(regionMap->getVirtualAddress());
  } else {
    // Physically contiguous kernel DMA. IOMallocContiguous is the only allocator
    // that returns a physical address directly; Apple deprecates it in favour of
    // IOBufferMemoryDescriptor, which the full driver will adopt once the window
    // grows past a single probe request.
    IOPhysicalAddress dmaPhys = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    virt = static_cast<uint8_t*>(IOMallocContiguous(16384, 16384, &dmaPhys));
#pragma clang diagnostic pop
    if (!virt) panic("LMIPRO UFS READ: contiguous DMA allocation failed");
    phys = dmaPhys;
    if (!phys || (phys & 0xfff)) panic("LMIPRO UFS READ: unusable DMA base %llx", phys);
  }

  // Layout inside the firmware window: [0,4096) request list, [4096,4608) command
  // UPIU + PRDT, [4608,5120) response UPIU, [8192,16384) data segment.
  const uint32_t ucdOffset = 4096, rspOffset = 512, prdtOffset = 1024, dataOffset = 8192;
  zeroRange(virt, 16384);
  cacheCleanRange(virt, 16384);

  uint32_t hcsBefore = readWord(hc, 0x30);
  uint32_t doorbell = readWord(hc, 0x58);
  if (doorbell != 0) panic("LMIPRO UFS READ: request outstanding db=%08x hcs=%08x", doorbell, hcsBefore);

  // Quiesce the firmware's list before touching its base address.
  writeWord(hc, 0x24, 0);            // mask interrupts
  writeWord(hc, 0x4c, 0);            // UTP interrupt aggregation off, as Linux does
  writeWord(hc, 0x60, 0);
  unsigned guard = 10000;
  while ((readWord(hc, 0x60) & 1) && guard--) IODelay(10);
  uint32_t runStop = readWord(hc, 0x60);
  if (runStop & 1) panic("LMIPRO UFS READ: list did not stop rsr=%08x hcs=%08x", runStop, readWord(hc, 0x30));

  writeWord(hc, 0x5c, 0xffffffffU);  // clear the transfer request list
  writeWord(hc, 0x50, uint32_t(phys));
  writeWord(hc, 0x54, uint32_t(phys >> 32));
  writeWord(hc, 0x20, 0xffffffffU);
  writeWord(hc, 0x80, 1);            // UTMRLRSR: task management list run
  writeWord(hc, 0x60, 1);
  bool listReady = false;
  for (unsigned loops = 0; loops < 100000 && !listReady; loops++) {
    listReady = (readWord(hc, 0x30) & 0x1) != 0;
    if (!listReady) IODelay(1);
  }
  uint32_t hcsAfter = readWord(hc, 0x30);
  if (gReadLevel == 1) {
    panic("LMIPRO UFS STOP/RESTART\ndma=%llx runstop=%08x hcs=%08x->%08x ready=%u",
          phys, runStop, hcsBefore, hcsAfter, listReady ? 1U : 0U);
  }

  auto command = [&](uint8_t lun, const uint8_t* cdb, unsigned cdbLength, bool toHost, uint32_t length, bool nop = false) -> LmiproUfsReply {
    uint8_t* utrd = virt;
    uint8_t* ucd = virt + ucdOffset;
    zeroRange(utrd, 32);
    zeroRange(ucd, 1040);
    if (length) zeroRange(virt + dataOffset, length > 8192 ? 8192 : ((length + 31u) & ~31u));
    if (nop) {
      ucd[0] = 0x00;                // NOP OUT: the controller completes it locally
      ucd[3] = 1;                   // task tag
    } else {
      ucd[0] = 0x01;                // COMMAND UPIU
      ucd[1] = toHost ? 0x40 : 0x00; // flags: data-in
      ucd[2] = lun;
      ucd[3] = 0;                   // task tag
      putBe32(ucd + 12, length);
      for (unsigned index = 0; index < cdbLength; index++) ucd[16 + index] = cdb[index];
    }
    uint64_t ucdPhys = phys + ucdOffset;
    uint32_t direction = nop ? 0U : (length ? (toHost ? 2U : 1U) : 0U);
    // The descriptor is read by the controller: volatile word stores keep the
    // compiler from fusing neighbours (0x04/0x08) into an unaligned 64-bit store.
    volatile uint32_t* utrdWords = reinterpret_cast<volatile uint32_t*>(utrd);
    utrdWords[0] = 0x10000000U | (direction << 25);
    utrdWords[1] = 0U;                                  // reserved in the UTRD layout
    utrdWords[2] = 0x0fU;                               // OCS: never completed
    utrdWords[4] = uint32_t(ucdPhys);
    utrdWords[5] = uint32_t(ucdPhys >> 32);
    utrdWords[6] = (512U >> 2) | ((rspOffset >> 2) << 16);
    if (length) {
      uint64_t dataPhys = phys + dataOffset;
      volatile uint32_t* prdWords = reinterpret_cast<volatile uint32_t*>(ucd + prdtOffset);
      prdWords[0] = uint32_t(dataPhys);
      prdWords[1] = uint32_t(dataPhys >> 32);
      prdWords[3] = length - 1;
    }
    utrdWords[7] = (length ? 1U : 0U) | ((prdtOffset >> 2) << 16);
    cacheCleanRange(virt, 16384);
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x58, 1);
    bool done = false;
    for (unsigned loops = 0; loops < 100000 && !done; loops++) {
      done = (readWord(hc, 0x58) & 1) == 0;
      if (!done) IODelay(1);
    }
    cacheInvalidateRange(virt, 16384);
    LmiproUfsReply reply;
    if (!done) return reply;
    reply.ocs = *reinterpret_cast<volatile uint32_t*>(utrd + 8) & 0xff;
    const uint8_t* rsp = ucd + rspOffset;
    reply.response = rsp[6];
    reply.scsi = rsp[7];
    return reply;
  };

  // level 7: ring the doorbell for a NOP OUT and report immediately, without
  // waiting for completion, so a reset can be attributed to one of the two.
  if (gReadLevel == 7) {
    uint8_t* utrd = virt;
    uint8_t* ucd = virt + ucdOffset;
    zeroRange(utrd, 32);
    zeroRange(ucd, 1040);
    ucd[0] = 0x00;                  // NOP OUT
    ucd[3] = 1;                     // task tag
    uint64_t ucdPhys = phys + ucdOffset;
    volatile uint32_t* words = reinterpret_cast<volatile uint32_t*>(utrd);
    words[0] = gDescDw0;
    words[1] = 0U;
    words[2] = 0x0fU;
    words[4] = uint32_t(ucdPhys);
    words[5] = uint32_t(ucdPhys >> 32);
    words[6] = (512U >> 2) | ((rspOffset >> 2) << 16);
    words[7] = (prdtOffset >> 2) << 16;
    cacheCleanRange(virt, 16384);
    // Disable the UFS SPI at the GIC so an unserviced interrupt cannot reset the
    // SoC; both INTID candidates (265 and 297) are cleared, a no-op if unused.
    OSSharedPtr<IOMemoryMap> gicMap;
    uintptr_t gicd = mapWindow(0x017a00000ULL, 0x1000, gicMap);
    uint32_t enLow = 0, enHigh = 0;
    if (gicd) {
      enLow = readWord(gicd, 0x120);
      enHigh = readWord(gicd, 0x124);
      writeWord(gicd, 0x1a0, 1U << 9);
      writeWord(gicd, 0x1a4, 1U << 9);
    }
    writeWord(hc, 0x20, 0xffffffffU);
    writeWord(hc, 0x58, 1);
    panic("LMIPRO UFS DOORBELL\ngic en8=%08x en9=%08x\ndma=%llx hcs=%08x db=%08x is=%08x\nutrd=%08x %02x %08x %08x",
          enLow, enHigh, phys, readWord(hc, 0x30), readWord(hc, 0x58), readWord(hc, 0x20),
          words[0], words[2], words[4], words[6]);
  }

  // level 6: NOP OUT, a controller-local command that never reaches the device.
  if (gReadLevel == 6) {
    LmiproUfsReply nop = command(0, nullptr, 0, false, 0, true);
    panic("LMIPRO UFS NOP\ndma=%llx ocs=%02x resp=%02x scsi=%02x\nhcs=%08x db=%08x is=%08x",
          phys, nop.ocs, nop.response, nop.scsi, readWord(hc, 0x30), readWord(hc, 0x58), readWord(hc, 0x20));
  }

  // level 3: submit one TUR only, before any state-changing command.
  if (gReadLevel == 3) {
    uint8_t turOnly[6] = { 0, 0, 0, 0, 0, 0 };
    LmiproUfsReply only = command(0, turOnly, 6, false, 0);
    panic("LMIPRO UFS TUR-ONLY\ndma=%llx tur=%06x hcs=%08x db=%08x is=%08x",
          phys, replyTriplet(only), readWord(hc, 0x30), readWord(hc, 0x58), readWord(hc, 0x20));
  }

  // The firmware parks the device in UFS-PowerDown; wake it through the UFS
  // Device well-known LU (0xD0), power condition ACTIVE. No user data is touched.
  uint8_t powerCondition = (gReadLevel == 5) ? 0x01 : 0x10;
  uint8_t startStop[6] = { 0x1b, 0, 0, 0, powerCondition, 0 };
  LmiproUfsReply wake = command(0xd0, startStop, 6, false, 0);
  for (unsigned settle = 0; settle < 30; settle++) IODelay(1000);
  if (gReadLevel == 4 || gReadLevel == 5) {
    panic("LMIPRO UFS SSU\npc=%02x dma=%llx ssu=%06x hcs=%08x db=%08x is=%08x",
          powerCondition, phys, replyTriplet(wake), readWord(hc, 0x30), readWord(hc, 0x58), readWord(hc, 0x20));
  }

  uint8_t testUnit[6] = { 0, 0, 0, 0, 0, 0 };
  LmiproUfsReply ready = command(0, testUnit, 6, false, 0);
  for (unsigned attempt = 0; attempt < 8 && (ready.ocs || ready.scsi); attempt++) {
    IODelay(5000);
    ready = command(0, testUnit, 6, false, 0);
  }
  if (gReadLevel == 2) {
    panic("LMIPRO UFS WAKE/TUR\ndma=%llx wake=%06x tur=%06x hcs=%08x db=%08x",
          phys, replyTriplet(wake), replyTriplet(ready), readWord(hc, 0x30), readWord(hc, 0x58));
  }

  uint8_t capacity[10] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  LmiproUfsReply cap = command(0, capacity, 10, true, 8);
  uint32_t lastLba = 0, blockSize = 0;
  if (cap.ocs == 0 && cap.scsi == 0) {
    lastLba = getBe32(virt + dataOffset);
    blockSize = getBe32(virt + dataOffset + 4);
  }

  // The GPT header lives in LBA 1 for both 512- and 4096-byte logical blocks.
  uint32_t sig0 = 0, sig1 = 0, headerCrc = 0, entryCrc = 0, entryCount = 0;
  uint64_t entryLba = 0;
  LmiproUfsReply read = { 0xf, 0xff, 0xff };
  if (blockSize >= 512 && blockSize <= 8192) {
    uint8_t read10[10] = { 0x28, 0, 0, 0, 0, 1, 0, 0, 1, 0 };
    read = command(0, read10, 10, true, blockSize);
    if (read.ocs == 0 && read.scsi == 0) {
      const uint8_t* header = virt + dataOffset;
      sig0 = getLe32(header);
      sig1 = getLe32(header + 4);
      headerCrc = getLe32(header + 16);
      entryLba = getLe64(header + 72);
      entryCount = getLe32(header + 80);
      entryCrc = getLe32(header + 88);
    }
  }

  lmipro_ufs_read_result[0] = listReady ? 1U : 0U;
  lmipro_ufs_read_result[1] = runStop;
  lmipro_ufs_read_result[2] = hcsBefore;
  lmipro_ufs_read_result[3] = hcsAfter;
  lmipro_ufs_read_result[4] = replyTriplet(wake);
  lmipro_ufs_read_result[5] = replyTriplet(ready);
  lmipro_ufs_read_result[6] = replyTriplet(cap);
  lmipro_ufs_read_result[7] = lastLba;
  lmipro_ufs_read_result[8] = blockSize;
  lmipro_ufs_read_result[9] = replyTriplet(read);
  lmipro_ufs_read_result[10] = sig0;
  lmipro_ufs_read_result[11] = sig1;
  lmipro_ufs_read_result[12] = headerCrc;
  lmipro_ufs_read_result[13] = entryCrc;
  lmipro_ufs_read_result[14] = uint32_t(entryLba);
  lmipro_ufs_read_result[15] = entryCount;

  panic("LMIPRO UFS READ (storage untouched)\n"
        "list=%llx stop=%08x hcs=%08x->%08x\n"
        "wake=%06x tur=%06x cap=%06x\n"
        "last=%08x bs=%08x read=%06x\n"
        "sig=%08x:%08x crc=%08x ecrc=%08x\n"
        "elba=%llx cnt=%u",
        phys, runStop, hcsBefore, hcsAfter,
        replyTriplet(wake), replyTriplet(ready), replyTriplet(cap),
        lastLba, blockSize, replyTriplet(read),
        sig0, sig1, headerCrc, entryCrc,
        entryLba, entryCount);
}
