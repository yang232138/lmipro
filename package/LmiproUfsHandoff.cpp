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

extern "C" void lmipro_ufs_read_probe(bool firmwareWindow);

extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode = 0;
  bool parsed = PE_parse_boot_argn("lmipro-ufs-handoff", &mode, sizeof(mode));
  // mode 2: one READ(10) on kernel-allocated contiguous DMA (the shipping path).
  // mode 3: same command, DMA window reused from the firmware's own UTRLBA.
  // mode 1: read-only register observation, no command.
  if (parsed && (mode == 2 || mode == 3)) { lmipro_ufs_read_probe(mode == 3); return; }
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

extern "C" void lmipro_ufs_read_probe(bool firmwareWindow)
{
  OSSharedPtr<IOMemoryMap> hcMap, regionMap;
  uintptr_t hc = mapWindow(0x01d84000ULL, 0x3000, hcMap);
  if (!hc) panic("LMIPRO UFS READ: controller map failed");
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
    // Shipping path: physically contiguous kernel DMA, the same allocation a real
    // block driver uses. apps_smmu is disabled, so this needs no stream mapping.
    IOPhysicalAddress dmaPhys = 0;
    virt = static_cast<uint8_t*>(IOMallocContiguous(16384, 16384, &dmaPhys));
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
  writeWord(hc, 0x24, 0);
  writeWord(hc, 0x60, 0);
  unsigned guard = 10000;
  while ((readWord(hc, 0x60) & 1) && guard--) IODelay(10);
  uint32_t runStop = readWord(hc, 0x60);
  if (runStop & 1) panic("LMIPRO UFS READ: list did not stop rsr=%08x hcs=%08x", runStop, readWord(hc, 0x30));

  writeWord(hc, 0x50, uint32_t(phys));
  writeWord(hc, 0x54, uint32_t(phys >> 32));
  writeWord(hc, 0x20, 0xffffffffU);
  writeWord(hc, 0x60, 1);
  bool listReady = false;
  for (unsigned loops = 0; loops < 100000 && !listReady; loops++) {
    listReady = (readWord(hc, 0x30) & 0x2) != 0;
    if (!listReady) IODelay(1);
  }
  uint32_t hcsAfter = readWord(hc, 0x30);

  auto command = [&](uint8_t lun, const uint8_t* cdb, unsigned cdbLength, bool toHost, uint32_t length) -> LmiproUfsReply {
    uint8_t* utrd = virt;
    uint8_t* ucd = virt + ucdOffset;
    zeroRange(utrd, 32);
    zeroRange(ucd, 1040);
    if (length) zeroRange(virt + dataOffset, length > 8192 ? 8192 : ((length + 31u) & ~31u));
    ucd[0] = 0x01;                  // COMMAND UPIU
    ucd[1] = toHost ? 0x40 : 0x00;  // flags: data-in
    ucd[2] = lun;
    ucd[3] = 0;                     // task tag
    putBe32(ucd + 12, length);
    for (unsigned index = 0; index < cdbLength; index++) ucd[16 + index] = cdb[index];
    uint64_t ucdPhys = phys + ucdOffset;
    uint32_t direction = length ? (toHost ? 2U : 1U) : 0U;
    *reinterpret_cast<uint32_t*>(utrd + 0) = 0x10000000U | (direction << 25);
    *reinterpret_cast<uint32_t*>(utrd + 8) = 0x0fU;    // OCS: never completed
    *reinterpret_cast<uint32_t*>(utrd + 16) = uint32_t(ucdPhys);
    *reinterpret_cast<uint32_t*>(utrd + 20) = uint32_t(ucdPhys >> 32);
    *reinterpret_cast<uint32_t*>(utrd + 24) = (512U >> 2) | ((rspOffset >> 2) << 16);
    if (length) {
      uint64_t dataPhys = phys + dataOffset;
      uint8_t* prd = ucd + prdtOffset;
      *reinterpret_cast<uint32_t*>(prd + 0) = uint32_t(dataPhys);
      *reinterpret_cast<uint32_t*>(prd + 4) = uint32_t(dataPhys >> 32);
      *reinterpret_cast<uint32_t*>(prd + 12) = length - 1;
    }
    *reinterpret_cast<uint32_t*>(utrd + 28) = (length ? 1U : 0U) | ((prdtOffset >> 2) << 16);
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

  // The firmware parks the device in UFS-PowerDown; wake it through the UFS
  // Device well-known LU (0xD0), power condition ACTIVE. No user data is touched.
  uint8_t startStop[6] = { 0x1b, 0, 0, 0, 0x10, 0 };
  LmiproUfsReply wake = command(0xd0, startStop, 6, false, 0);
  for (unsigned settle = 0; settle < 30; settle++) IODelay(1000);

  uint8_t testUnit[6] = { 0, 0, 0, 0, 0, 0 };
  LmiproUfsReply ready = command(0, testUnit, 6, false, 0);
  for (unsigned attempt = 0; attempt < 8 && (ready.ocs || ready.scsi); attempt++) {
    IODelay(5000);
    ready = command(0, testUnit, 6, false, 0);
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
