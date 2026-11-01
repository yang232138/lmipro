// SPDX-License-Identifier: BSD-2-Clause-Patent
// Diagnostic-only local DME_GET. No UTP, doorbell, HCE, PHY or SMMU writes.
#include <stdint.h>

// BEGIN HOST-TESTED UIC CORE
struct LmiproUicResult {
  uint32_t stage, opcode, arg1, beforeHcs, beforeIs, savedIe;
  uint32_t polls, done, is, arg2, arg3, hcs;
};

template<class Ops>
static LmiproUicResult lmiproLocalDmeGet(Ops& io, uint32_t attribute, uint32_t limit)
{
  LmiproUicResult r = {};
  r.opcode = 1;
  r.arg1 = (attribute & 0xffffU) << 16;
  r.beforeHcs = io.read32(0x30);
  r.beforeIs = io.read32(0x20);
  r.savedIe = io.read32(0x24);
  r.hcs = r.beforeHcs;
  r.is = r.beforeIs;
  r.stage = 1; // Not ready; no writes performed.
  if (!(r.beforeHcs & 8U) || !limit) return r;
  if (r.beforeIs & 0x70084U) { r.stage = 2; return r; } // Existing error; do not submit.
  io.write32(0x24, 0);
  io.write32(0x20, 0x400U); // Acknowledge only the old UIC completion bit.
  io.write32(0x94, r.arg1);
  io.write32(0x98, 0);
  io.write32(0x9c, 0);
  io.write32(0x90, r.opcode);
  r.stage = 4; // Timed out unless completion/error is observed below.
  for (uint32_t i = 0; i < limit; ++i) {
    r.polls = i + 1;
    r.is = io.read32(0x20);
    if (r.is & 0x400U) { r.done = 1; r.stage = 3; break; }
    if (r.is & 0x70084U) { r.stage = 5; break; } // Fatal, link lost or UIC error.
    io.delayUs(1);
  }
  r.arg2 = io.read32(0x98);
  r.arg3 = io.read32(0x9c);
  r.hcs = io.read32(0x30);
  io.write32(0x24, r.savedIe);
  return r;
}
// END HOST-TESTED UIC CORE

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

struct LmiproUicMmio {
  uintptr_t base;
  uint32_t read32(unsigned offset) {
    uint32_t value = *reinterpret_cast<const volatile uint32_t*>(base + offset);
    __asm__ volatile("dsb sy" ::: "memory");
    return value;
  }
  void write32(unsigned offset, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(base + offset) = value;
    __asm__ volatile("dsb sy" ::: "memory");
  }
  void delayUs(unsigned us) { IODelay(us); }
};

extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode = 0;
  if (!PE_parse_boot_argn("lmipro-ufs-handoff", &mode, sizeof(mode))) return;
  if (mode != 17) panic("LMIPRO UIC FIX1: only mode 17 is enabled; mode=%u", mode);
  IODeviceMemory* memory = IODeviceMemory::withRange(0x01d84000ULL, 0x3000);
  if (!memory) panic("LMIPRO UIC FIX1: descriptor failed");
  OSSharedPtr<IOMemoryMap> keep(memory->map(kIOMapAnywhere | kIOMapInhibitCache), OSNoRetain);
  memory->release();
  if (!keep) panic("LMIPRO UIC FIX1: mapping failed");
  LmiproUicMmio io = { static_cast<uintptr_t>(keep->getVirtualAddress()) };
  LmiproUicResult r = lmiproLocalDmeGet(io, 0x1560U, 500000U);
  const uint32_t snapshot[12] = {r.stage,r.opcode,r.arg1,r.beforeHcs,r.beforeIs,r.savedIe,
                                r.polls,r.done,r.is,r.arg2,r.arg3,r.hcs};
  for (unsigned i = 0; i < 12; ++i) lmipro_ufs_handoff_observation[i] = snapshot[i];
  panic("LMIPRO UIC FIX1 (local DME_GET)\n"
        "stage=%u done=%u polls=%u limit=500000us\n"
        "op=%08x arg1=%08x\n"
        "hcsBefore=%08x isBefore=%08x ie=%08x\n"
        "is=%08x result=%02x data=%08x hcs=%08x",
        r.stage,r.done,r.polls,r.opcode,r.arg1,r.beforeHcs,r.beforeIs,r.savedIe,
        r.is,r.arg2 & 0xffU,r.arg3,r.hcs);
}
#endif
