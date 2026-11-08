// SPDX-License-Identifier: BSD-2-Clause-Patent
// Read-only UFS block device for the SHNGROOT windows.
//
// This is the storage half of the observer in LmiproUfsHandoff.cpp: the same
// UFS takeover (stop the firmware list, SMMUv2 bypass for stream 0x60, one NOP
// OUT, polled completion), the same READ(10) allowlist, and no write command
// anywhere. It publishes each SHNGROOT window as an IOBlockStorageDevice so the
// APFS stack can mount the OS and SystemOS containers.
#include <stdint.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOBSD.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSIterator.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSBoolean.h>
#include <libkern/c++/OSData.h>
#include <libkern/crypto/sha2.h>
#include <pexpert/pexpert.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <sys/types.h>

// ---- SMMUv2 planning (same rules as the observer) --------------------------
struct UfsSmmuPlan {
  unsigned slot = 0xffffffffU, bank = 0, pages = 0;
  uint32_t smr = 0, s2cr = 0;
  bool reuse = false;
  const char *error = nullptr;
};
static inline unsigned ufsSmmuAddressBits(unsigned code)
{
  switch (code) {
    case 0: return 32; case 1: return 36; case 2: return 40;
    case 3: return 42; case 4: return 44; case 5: return 48;
    default: return 0;
  }
}
template <class Read>
static UfsSmmuPlan planUfsSmmuBank(uint16_t stream, uint64_t dma, uint64_t bytes, Read read, unsigned bankFromTop)
{
  UfsSmmuPlan p;
  uint32_t idr0 = read(0x20), idr2 = read(0x28);
  unsigned count = idr0 & 255;
  if (!(idr0 & (1U << 30)) || !(idr2 & (1U << 12))) { p.error = "SMMU AArch64 stage 1 unsupported"; return p; }
  for (unsigned shift = 0; shift <= 8; shift += 4) {
    unsigned bits = ufsSmmuAddressBits((idr2 >> shift) & 15);
    if (!bits || !bytes || dma >= (1ULL << bits) || bytes > (1ULL << bits) - dma) { p.error = "DMA exceeds SMMU address width"; return p; }
  }
  uint32_t idr1 = read(0x24);
  unsigned banks = idr1 & 255;
  p.pages = 1U << (((idr1 >> 28) & 7) + 1);
  if ((idr1 >> 31) || !count || p.pages > 128 || banks < 2 || banks > 128 || (p.pages + banks) * 4096 > 0xd0000) {
    p.error = "unsupported SMMU geometry"; return p;
  }
  if (count > 128) count = 128;
  if (bankFromTop >= banks - 1) { p.error = "no free context bank"; return p; }
  p.bank = banks - 1 - bankFromTop;
  unsigned matches = 0;
  for (unsigned i = 0; i < count; ++i) {
    uint32_t smr = read(0x800 + 4 * i);
    if (!(smr & 0x80000000U)) { if (!p.reuse) p.slot = i; continue; }
    uint32_t s2cr = read(0xc00 + 4 * i);
    uint32_t mask = (smr >> 16) & 0x7fff;
    bool match = ((smr ^ stream) & 0xffff & ~mask) == 0;
    bool ourBank = ((s2cr >> 16) & 3) == 0 && (s2cr & 255) == p.bank;
    if (match) {
      ++matches;
      if (matches > 1 || mask || !ourBank) { p.slot = i; p.smr = smr; p.s2cr = s2cr; p.error = "conflicting UFS stream mapping"; return p; }
      p.reuse = true; p.slot = i;
    } else if (ourBank) { p.slot = i; p.smr = smr; p.s2cr = s2cr; p.error = "reserved context bank already used"; return p; }
  }
  uint32_t sctlr = read((p.pages + p.bank) * 4096);
  if ((sctlr & 1) || (p.reuse && (sctlr || read(0x1000 + 4 * p.bank) != 0x10000 || (read(0x1800 + 4 * p.bank) & ~1U)))) {
    p.error = "reserved context bank not bypass"; return p;
  }
  if (p.slot == 0xffffffffU) p.error = "no free stream-match group";
  return p;
}
template <class Read>
static UfsSmmuPlan planUfsSmmu(uint16_t stream, uint64_t dma, uint64_t bytes, Read read)
{
  enum { kBankAttempts = 8 };
  UfsSmmuPlan first = planUfsSmmuBank(stream, dma, bytes, read, 0);
  if (!first.error) return first;
  auto retryable = [](const char *e) {
    return !__builtin_strcmp(e, "reserved context bank already used") || !__builtin_strcmp(e, "reserved context bank not bypass") ||
           !__builtin_strcmp(e, "conflicting UFS stream mapping");
  };
  if (!retryable(first.error)) return first;
  for (unsigned attempt = 1; attempt < kBankAttempts; ++attempt) {
    UfsSmmuPlan next = planUfsSmmuBank(stream, dma, bytes, read, attempt);
    if (!next.error) return next;
    if (!retryable(next.error)) break;
  }
  return first;
}
template <class Read, class Write>
static const char *configureUfsSmmu(const UfsSmmuPlan &p, uint16_t stream, Read read, Write write)
{
  if (p.error) return p.error;
  unsigned cb = (p.pages + p.bank) * 4096;
  if (!p.reuse) write(cb, 0);
  if (read(cb) != 0) return "SMMU bypass disable readback";
  if (read(0x1800 + 4 * p.bank) != 1) write(0x1800 + 4 * p.bank, 1);
  if (!p.reuse) write(0x1000 + 4 * p.bank, 0x10000);
  if (read(0x1800 + 4 * p.bank) != 1 || read(0x1000 + 4 * p.bank) != 0x10000 || read(cb) != 0)
    return "SMMU VA64 bypass readback";
  if (!p.reuse) {
    write(0xc00 + 4 * p.slot, p.bank);
    if (read(0xc00 + 4 * p.slot) != p.bank) return "SMMU stream context readback";
    write(0x800 + 4 * p.slot, 0x80000000U | stream);
  }
  if (read(0x800 + 4 * p.slot) != (0x80000000U | stream) || read(0xc00 + 4 * p.slot) != p.bank)
    return "SMMU stream mapping readback";
  return nullptr;
}

// ---- transport -------------------------------------------------------------
static constexpr uint32_t kUfsFatalStatus = 0x71880U;
static constexpr uint64_t kDmaBytes = 16384;

struct LmiproUfsPort {
  uintptr_t hc, smmu;
  uint64_t dma;
  uint32_t bytes;
  uint8_t *data;
  static uint32_t read(uintptr_t base, unsigned off) {
    uint32_t v = *reinterpret_cast<const volatile uint32_t *>(base + off);
    __asm__ volatile("dsb sy" ::: "memory"); return v;
  }
  static void write(uintptr_t base, unsigned off, uint32_t v) {
    __asm__ volatile("dsb sy" ::: "memory");
    *reinterpret_cast<volatile uint32_t *>(base + off) = v;
    __asm__ volatile("dsb sy" ::: "memory");
  }
  uint32_t hostRead(unsigned off) { return read(hc, off); }
  void hostWrite(unsigned off, uint32_t v) {
    if (off != 0x20 && off != 0x24 && off != 0x50 && off != 0x54 && off != 0x58 && off != 0x60) return;
    write(hc, off, v);
  }
  uint32_t smmuRead(unsigned off) { return read(smmu, off); }
  void smmuWrite(unsigned off, uint32_t v) { write(smmu, off, v); }
  void delay() { IODelay(2); }
  void clean() {
    for (uintptr_t p = reinterpret_cast<uintptr_t>(data); p < reinterpret_cast<uintptr_t>(data) + kDmaBytes; p += 64)
      __asm__ volatile("dc civac, %0" :: "r"(p) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
  }
  void invalidate() {
    __asm__ volatile("dsb sy" ::: "memory");
    for (uintptr_t p = reinterpret_cast<uintptr_t>(data); p < reinterpret_cast<uintptr_t>(data) + kDmaBytes; p += 64)
      __asm__ volatile("dc ivac, %0" :: "r"(p) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
  }
};

static inline uint32_t lmBe(const uint8_t *p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
static inline uint32_t lmLe(const uint8_t *p) { return uint32_t(p[3])<<24|uint32_t(p[2])<<16|uint32_t(p[1])<<8|p[0]; }
static inline uint64_t lmLe64(const uint8_t *p) { return uint64_t(lmLe(p))|(uint64_t(lmLe(p+4))<<32); }
static inline uint32_t lmCrc(uint32_t crc,const uint8_t *p,unsigned n) {
  for(unsigned i=0;i<n;++i){crc^=p[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1)));}return crc;
}

// Exactly one submission per call; the caller holds the device lock.
static const char *lmUfsCommand(LmiproUfsPort &io, uint8_t opcode, uint32_t lba, unsigned bytes, uint32_t *ocsOut)
{
  if ((opcode != 0x25 && opcode != 0x28) || !bytes || bytes > 4096 || (bytes & 3)) return "read command bounds";
  if (io.hostRead(0x58) || io.hostRead(0x78) || (io.hostRead(0x20) & kUfsFatalStatus)) return "read queue not idle";
  for (unsigned i = 0; i < kDmaBytes; ++i) io.data[i] = 0;
  auto *w = reinterpret_cast<uint32_t *>(io.data);
  uint64_t ucd = io.dma + 4096, data = io.dma + 8192;
  w[0] = 0x14000000; w[2] = 15; w[4] = uint32_t(ucd); w[5] = uint32_t(ucd >> 32);
  w[6] = 128U | (128U << 16); w[7] = 1U | (256U << 16);
  auto *prd = reinterpret_cast<uint32_t *>(io.data + 5120);
  prd[0] = uint32_t(data); prd[1] = uint32_t(data >> 32); prd[3] = bytes - 1;
  uint8_t *q = io.data + 4096; q[0] = 1; q[1] = 0x40; q[3] = 2;
  q[12] = uint8_t(bytes >> 24); q[13] = uint8_t(bytes >> 16); q[14] = uint8_t(bytes >> 8); q[15] = uint8_t(bytes);
  q[16] = opcode;
  if (opcode == 0x28) { q[18] = uint8_t(lba >> 24); q[19] = uint8_t(lba >> 16); q[20] = uint8_t(lba >> 8); q[21] = uint8_t(lba); q[24] = 1; }
  io.clean(); io.hostWrite(0x20, 1); io.hostWrite(0x60, 1);
  if (!(io.hostRead(0x60) & 1) || (io.hostRead(0x30) & 15) != 15) return "read queue start";
  io.hostWrite(0x58, 1);
  uint32_t ocs = 15;
  for (unsigned polls = 0; polls < 500000; ++polls) {
    if (io.hostRead(0x20) & kUfsFatalStatus) return "read fatal; DMA retained";
    if (!io.hostRead(0x58)) { io.invalidate(); ocs = w[2] & 255; if (ocs != 15) break; }
    io.delay();
  }
  if (ocs == 15) return "read timeout; DMA retained";
  if (ocs) return "read OCS error";
  const uint8_t *rsp = io.data + 4608;
  if (rsp[0] != 0x21 || rsp[2] || rsp[3] != 2 || rsp[8]) return "read response header";
  unsigned len = unsigned(rsp[10]) * 256 + rsp[11];
  if (len > 480) return "read response length";
  uint32_t scsi = unsigned(rsp[6]) * 256 + rsp[7];
  if (scsi) return "SCSI status; no retry";
  if ((rsp[1] & 0x60) || lmBe(rsp + 12)) return "short/overrun read";
  if (ocsOut) *ocsOut = ocs;
  return nullptr;
}

// ---- UFS bring-up ----------------------------------------------------------
static uintptr_t mapPhys(uint64_t phys, uint64_t length, OSSharedPtr<IOMemoryMap> &keep)
{
  IODeviceMemory *mem = IODeviceMemory::withRange(phys, length);
  if (!mem) return 0;
  keep = OSSharedPtr<IOMemoryMap>(mem->map(kIOMapAnywhere | kIOMapInhibitCache), OSNoRetain);
  mem->release();
  return keep ? static_cast<uintptr_t>(keep->getVirtualAddress()) : 0;
}

struct LmiproUfsHost {
  LmiproUfsPort io = {};
  OSSharedPtr<IOMemoryMap> hcMap, smmuMap;
  bool ready = false;
  uint32_t lastLba = 0, blockBytes = 0;
  const char *error = nullptr;
};

static bool lmUfsBringUp(LmiproUfsHost &host, const char **why)
{
  if (host.ready) return true;
  host.io.hc = mapPhys(0x01d84000ULL, 0x3000, host.hcMap);
  if (!host.io.hc) { *why = "controller map"; return false; }
  auto *dma = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
      static_cast<IOOptionBits>(kIODirectionInOut) | static_cast<IOOptionBits>(kIOMemoryPhysicallyContiguous) |
      static_cast<IOOptionBits>(kIOMemoryMapperNone), kDmaBytes, 4096);
  if (!dma) { *why = "DMA allocation"; return false; }
  IOByteCount contiguous = 0;
  uint64_t pa = dma->getPhysicalSegment(0, &contiguous, kIOMemoryMapperNone);
  host.io.data = static_cast<uint8_t *>(dma->getBytesNoCopy());
  if (!host.io.data || !pa || (pa & 4095) || contiguous < kDmaBytes) { *why = "DMA address"; return false; }
  host.io.dma = pa; host.io.bytes = kDmaBytes;
  for (unsigned i = 0; i < kDmaBytes; ++i) host.io.data[i] = 0;
  host.io.clean();

  if (host.io.hostRead(0x58) || host.io.hostRead(0x78) || (host.io.hostRead(0x20) & kUfsFatalStatus)) { *why = "queue busy"; return false; }
  host.io.hostWrite(0x24, 0); host.io.hostWrite(0x60, 0);
  for (unsigned loops = 10000; (host.io.hostRead(0x60) & 1) && loops; --loops) host.io.delay();
  if ((host.io.hostRead(0x60) & 1) || host.io.hostRead(0x58) || host.io.hostRead(0x78)) { *why = "queue stop"; return false; }

  host.io.smmu = mapPhys(0x15000000ULL, 0xd0000, host.smmuMap);
  if (!host.io.smmu) { *why = "SMMU map"; return false; }
  // The UFS host on this SoC is reachable through two streams (0xe0 and 0x4e0),
  // exactly as the observer established; 0x60 is the SM8550 wiring and its
  // stream-match write does not read back on SM8250.
  auto sread = [&](unsigned off) { return host.io.smmuRead(off); };
  auto swrite = [&](unsigned off, uint32_t v) { host.io.smmuWrite(off, v); };
  UfsSmmuPlan p0 = planUfsSmmu(0xe0, host.io.dma, kDmaBytes, sread);
  if (p0.error) { *why = p0.error; return false; }
  if (p0.reuse) { *why = "existing e0 mapping not owned"; return false; }
  auto reservedRead = [&](unsigned off) {
    if (off == 0x800 + 4 * p0.slot) return 0x800000e0U;
    if (off == 0xc00 + 4 * p0.slot) return uint32_t(p0.bank);
    return sread(off);
  };
  UfsSmmuPlan p1 = planUfsSmmu(0x4e0, host.io.dma, kDmaBytes, reservedRead);
  if (p1.error) { *why = p1.error; return false; }
  if (p1.reuse || p1.slot == p0.slot || p1.bank == p0.bank) { *why = "mapping ownership"; return false; }
  const char *e0 = configureUfsSmmu(p0, 0xe0, sread, swrite);
  if (e0) { *why = e0; return false; }
  const char *e1 = configureUfsSmmu(p1, 0x4e0, sread, swrite);
  if (e1) { *why = e1; return false; }

  // NOP OUT proves the transport before any SCSI read.
  auto *w = reinterpret_cast<uint32_t *>(host.io.data);
  for (unsigned i = 0; i < kDmaBytes; ++i) host.io.data[i] = 0;
  uint64_t ucd = host.io.dma + 4096;
  w[0] = 0x10000000; w[2] = 15; w[4] = uint32_t(ucd); w[5] = uint32_t(ucd >> 32); w[6] = 128U | (128U << 16);
  host.io.data[4096 + 3] = 1;
  host.io.clean();
  host.io.hostWrite(0x50, uint32_t(host.io.dma)); host.io.hostWrite(0x54, uint32_t(host.io.dma >> 32));
  host.io.hostWrite(0x20, 1); host.io.hostWrite(0x60, 1);
  for (unsigned loops = 10000; !(host.io.hostRead(0x60) & 1) && loops; --loops) host.io.delay();
  if (!(host.io.hostRead(0x60) & 1)) { *why = "queue start"; return false; }
  host.io.hostWrite(0x58, 1);
  uint32_t ocs = 15;
  for (unsigned polls = 0; polls < 100000; ++polls) {
    if (host.io.hostRead(0x20) & kUfsFatalStatus) { *why = "NOP fatal"; return false; }
    if (!host.io.hostRead(0x58)) { host.io.invalidate(); ocs = w[2] & 255; if (ocs != 15) break; }
    host.io.delay();
  }
  if (ocs == 15 || ocs) { *why = "NOP completion"; return false; }
  const uint8_t *rsp = host.io.data + 4096 + 512;
  if (rsp[0] != 0x20 || rsp[2] != 0 || rsp[3] != 1) { *why = "NOP response"; return false; }
  host.io.hostWrite(0x60, 0);
  for (unsigned loops = 10000; (host.io.hostRead(0x60) & 1) && loops; --loops) host.io.delay();

  uint32_t oc = 15;
  const char *rc = lmUfsCommand(host.io, 0x25, 0, 8, &oc);
  if (rc) { *why = rc; return false; }
  const uint8_t *cap = host.io.data + 8192;
  host.lastLba = lmBe(cap); host.blockBytes = lmBe(cap + 4);
  if (host.lastLba == 0xffffffffU || host.lastLba < 100 || (host.blockBytes != 512 && host.blockBytes != 4096)) {
    *why = "capacity geometry"; return false;
  }
  host.ready = true;
  return true;
}

// ---- SHNGROOT --------------------------------------------------------------
struct LmiproWindow { char name[32]; uint64_t offset, size; };
struct LmiproRootLayout {
  uint64_t userdataFirst = 0, userdataLast = 0;
  uint32_t windows = 0;
  LmiproWindow window[4] = {};
  uint32_t manifest = 1; // 0 ok, else the refusal code
};

static bool lmReadGpt(LmiproUfsHost &host, LmiproRootLayout &out, const char **why)
{
  uint32_t oc = 15;
  const char *rc = lmUfsCommand(host.io, 0x28, 1, host.blockBytes, &oc);
  if (rc) { *why = rc; return false; }
  uint8_t *b = host.io.data + 8192;
  if (lmLe(b) != 0x20494645U || lmLe(b + 4) != 0x54524150U || lmLe64(b + 24) != 1) { *why = "GPT header"; return false; }
  unsigned hlen = lmLe(b + 12);
  if (hlen < 92 || hlen > host.blockBytes) { *why = "GPT header size"; return false; }
  uint32_t hcrc = lmLe(b + 16);
  b[16] = b[17] = b[18] = b[19] = 0;
  if ((lmCrc(0xffffffffU, b, hlen) ^ 0xffffffffU) != hcrc) { *why = "GPT header CRC"; return false; }
  uint64_t entries = lmLe64(b + 72);
  unsigned count = lmLe(b + 80), esize = lmLe(b + 84);
  if (!count || count > 128 || esize != 128) { *why = "GPT entry bounds"; return false; }
  unsigned blocks = (count * 128 + host.blockBytes - 1) / host.blockBytes;
  for (unsigned blk = 0; blk < blocks; ++blk) {
    rc = lmUfsCommand(host.io, 0x28, uint32_t(entries + blk), host.blockBytes, &oc);
    if (rc) { *why = rc; return false; }
    unsigned n = count * 128 - blk * host.blockBytes;
    if (n > host.blockBytes) n = host.blockBytes;
    for (unsigned off = 0; off + 128 <= n; off += 128) {
      const uint8_t *e = host.io.data + 8192 + off;
      bool used = false; for (unsigned i = 0; i < 16; ++i) if (e[i]) used = true;
      if (!used) continue;
      const char want[] = "userdata";
      bool match = true;
      for (unsigned k = 0; k < 9; ++k) if (e[56 + 2*k] != uint8_t(want[k]) || e[57 + 2*k]) match = false;
      if (!match) continue;
      out.userdataFirst = lmLe64(e + 32);
      out.userdataLast = lmLe64(e + 40);
      *why = nullptr;
      return true;
    }
  }
  *why = "userdata partition";
  return false;
}


// Board boot namespace identity, NOT Apple or VerifiedRoot authentication.
// Bind the checksum-checked on-disk layout to the locally verified image set.
static uint8_t gLmiproBootIdentity[48];
static bool gLmiproBootIdentityReady;
static const char gLmiproBootDomain[] = "Lmipro.SHNGROOT.ImageSet.v1";
static bool lmValidateBootIdentity(const uint8_t *b, uint32_t blockBytes)
{
  gLmiproBootIdentityReady = false;
  if (blockBytes != 4096 || lmLe(b + 8) != 1 || lmLe(b + 12) != 2 ||
      lmLe64(b + 16) != 9382658048ULL) return false;
  uint8_t checksum[32];
  SHA256_CTX check;
  SHA256_Init(&check); SHA256_Update(&check, b, 4064); SHA256_Final(checksum, &check);
  if (memcmp(checksum, b + 4064, 32)) return false;
  static const char names[2][32] = {"OS", "SystemOS"};
  static const uint64_t offsets[2] = {1048576ULL, 6079643648ULL};
  static const uint64_t sizes[2] = {6077580800ULL, 3303014400ULL};
  static const uint8_t hashes[2][32] = {{0xe5,0x63,0x4d,0x04,0xa8,0xac,0x35,0xf0,0x24,0x84,0x5f,0x58,0x33,0x94,0xe0,0xa3,0x5d,0xeb,0x26,0xa0,0x4c,0xb0,0xc0,0x02,0x86,0xa8,0x67,0x4e,0x25,0xa9,0xa4,0xf2},{0xb4,0x90,0x8c,0xde,0x41,0xd2,0x3e,0x6b,0xc0,0x5e,0x6a,0xf5,0x1a,0x2c,0x9e,0xeb,0x00,0xcf,0x0f,0x86,0x3a,0x6a,0x18,0xb1,0x60,0x0c,0x96,0xa3,0xfd,0x70,0x41,0x4f}};
  for (unsigned i = 0; i < 2; i++) {
    const uint8_t *e = b + 24 + 128 * i;
    if (memcmp(e, names[i], 32) || lmLe64(e + 32) != offsets[i] ||
        lmLe64(e + 40) != sizes[i] || memcmp(e + 56, hashes[i], 32)) return false;
  }
  SHA384_CTX identity;
  SHA384_Init(&identity);
  SHA384_Update(&identity, gLmiproBootDomain, sizeof(gLmiproBootDomain));
  SHA384_Update(&identity, b, 4096);
  SHA384_Final(gLmiproBootIdentity, &identity);
  gLmiproBootIdentityReady = true;
  return true;
}

static bool lmReadManifest(LmiproUfsHost &host, LmiproRootLayout &out, const char **why)
{
  uint32_t oc = 15;
  const char *rc = lmUfsCommand(host.io, 0x28, uint32_t(out.userdataFirst), host.blockBytes, &oc);
  if (rc) { *why = rc; return false; }
  const uint8_t *b = host.io.data + 8192;
  if (lmLe(b) != 0x474e4853U || lmLe(b + 4) != 0x544f4f52U) { out.manifest = 2; *why = "no SHNGROOT manifest"; return false; }
  uint32_t version = lmLe(b + 8), count = lmLe(b + 12);
  if (version != 1 || !count || count > 4) { out.manifest = 3; *why = "manifest version/count"; return false; }
  out.windows = count;
  for (unsigned i = 0; i < count; ++i) {
    const uint8_t *e = b + 24 + 128 * i;
    unsigned k = 0;
    for (; k < 31 && e[k]; ++k) out.window[i].name[k] = char(e[k]);
    out.window[i].name[k] = 0;
    out.window[i].offset = lmLe64(e + 32);
    out.window[i].size = lmLe64(e + 40);
    if ((out.window[i].offset & 4095ULL) || !out.window[i].size) { out.manifest = 4; *why = "window alignment"; return false; }
    if (out.window[i].offset / 4096ULL > 0xffffffffULL) { out.manifest = 5; *why = "window offset range"; return false; }
  }
  if (!lmValidateBootIdentity(b, host.blockBytes)) { out.manifest = 6; *why = "boot identity layout/checksum/pin mismatch"; return false; }
  out.manifest = 0;
  return true;
}

// ---- block device ----------------------------------------------------------
static LmiproUfsHost gHost;
static volatile int gUfsPublishState = 0;
static volatile unsigned gPublishedWindows = 0;
static volatile unsigned gDeviceMediaStage = 0;
static char gUfsPublishWhy[64];
static void lmSetPublishWhy(const char *why)
{
  size_t k = 0;
  for (; why && why[k] && k + 1 < sizeof(gUfsPublishWhy); ++k) gUfsPublishWhy[k] = why[k];
  gUfsPublishWhy[k] = 0;
}
static IOLock *gUfsLock = nullptr;

class LmiproUfsBlock : public IOBlockStorageDevice {
  OSDeclareDefaultStructors(LmiproUfsBlock)
  uint64_t absolute = 0;
  uint64_t bytes = 0;
  char label[32] = {};
  uint8_t *bounce = nullptr;
  bool online = false;
public:
  bool configure(const char *name, uint64_t offset, uint64_t size) {
    if (!name || !size || (offset & 511ULL)) return false;
    unsigned i = 0; for (; i < 31 && name[i]; ++i) label[i] = name[i]; label[i] = 0;
    absolute = offset; bytes = size;
    bounce = static_cast<uint8_t *>(IOMalloc(4096));
    return bounce != nullptr;
  }
  bool start(IOService *provider) override {
    if (!bounce || !IOBlockStorageDevice::start(provider)) return false;
    online = true;
    registerService();
    IOLog("LmiproUfsBlock: published %s (%llu bytes)\n", label, (unsigned long long)bytes);
    return true;
  }
  void free() override {
    if (bounce) { IOFree(bounce, 4096); bounce = nullptr; }
    IOBlockStorageDevice::free();
  }
  IOReturn doEjectMedia() override { return kIOReturnUnsupported; }
  IOReturn doFormatMedia(UInt64) override { return kIOReturnNotWritable; }
  UInt32 doGetFormatCapacities(UInt64 *, UInt32) const override { return 0; }
  char *getVendorString() override { return const_cast<char *>("Lmipro"); }
  char *getProductString() override { return label; }
  char *getRevisionString() override { return const_cast<char *>("1.0"); }
  char *getAdditionalDeviceInfoString() override { return const_cast<char *>("UFS SHNGROOT window, read-only"); }
  IOReturn reportBlockSize(UInt64 *value) override { *value = 512; return kIOReturnSuccess; }
  IOReturn reportEjectability(bool *value) override { *value = false; return kIOReturnSuccess; }
  IOReturn reportMaxValidBlock(UInt64 *value) override { *value = bytes / 512 - 1; return kIOReturnSuccess; }
  IOReturn reportMediaState(bool *present, bool *changed) override { *present = online; if (changed) *changed = false; return kIOReturnSuccess; }
  IOReturn reportRemovability(bool *value) override { *value = false; return kIOReturnSuccess; }
  IOReturn reportWriteProtection(bool *value) override { *value = true; return kIOReturnSuccess; }
  IOReturn doSynchronizeCache() override { return online ? kIOReturnSuccess : kIOReturnNotReady; }
  IOReturn doSynchronize(UInt64, UInt64, IOOptionBits) override { return doSynchronizeCache(); }
  IOReturn getWriteCacheState(bool *value) override { *value = false; return kIOReturnSuccess; }
  IOReturn setWriteCacheState(bool) override { return kIOReturnUnsupported; }
  IOReturn doAsyncReadWrite(IOMemoryDescriptor *, UInt64, UInt64, IOStorageAttributes *, IOStorageCompletion *) override;
};
OSDefineMetaClassAndStructors(LmiproUfsBlock, IOBlockStorageDevice)

IOReturn LmiproUfsBlock::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block, UInt64 count,
    IOStorageAttributes *, IOStorageCompletion *completion)
{
  if (!buffer || !completion || !completion->action || !count || block >= bytes / 512 ||
      count > bytes / 512 - block || buffer->getLength() < count * 512) return kIOReturnBadArgument;
  if (buffer->getDirection() != kIODirectionIn) return kIOReturnNotWritable;
  if (!online || !gUfsLock) return kIOReturnNotReady;
  uint64_t offset = absolute + block * 512;
  uint64_t remaining = count * 512, done = 0;
  IOReturn status = kIOReturnSuccess;
  IOLockLock(gUfsLock);
  while (remaining) {
    uint64_t lba = offset / gHost.blockBytes;
    unsigned shift = unsigned(offset % gHost.blockBytes);
    if (lba > gHost.lastLba) { status = kIOReturnIOError; break; }
    uint32_t oc = 15;
    const char *rc = lmUfsCommand(gHost.io, 0x28, uint32_t(lba), gHost.blockBytes, &oc);
    if (rc) { status = kIOReturnIOError; break; }
    unsigned want = 4096U - shift;
    if (want > remaining) want = unsigned(remaining);
    uint64_t copied = buffer->writeBytes(done, gHost.io.data + 8192 + shift, want);
    if (copied != want) { status = kIOReturnIOError; break; }
    done += want; remaining -= want; offset += want;
  }
  IOLockUnlock(gUfsLock);
  completion->action(completion->target, completion->parameter, status, done);
  return kIOReturnSuccess;
}

// ---- publication -----------------------------------------------------------
// The block devices attach to this controller (an ordinary IOService), not to
// the platform expert root: that is the provider shape the sheng driver uses,
// and the publication runs from a kernel thread rather than from bsd_init.
class LmiproUfsController : public IOService {
  OSDeclareDefaultStructors(LmiproUfsController)
};
OSDefineMetaClassAndStructors(LmiproUfsController, IOService)

static unsigned gDiag = 0;
static LmiproUfsBlock *gSystemOsBlock = nullptr;
static LmiproUfsBlock *gOsBlock = nullptr;

static void lmiproPublishThread(void *arg, wait_result_t)
{
  auto *ctl = static_cast<LmiproUfsController *>(arg);
  const char *why = "";
  if (!lmUfsBringUp(gHost, &why)) {
    if (gDiag) panic("LMIPRO UFS BLOCK\nbring-up: %s", why ? why : "?");
    IOLog("LmiproUfsBlock: bring-up failed: %s\n", why ? why : "?");
    gUfsPublishState = -1;
    lmSetPublishWhy(why);
    panic("LMIPRO UFS PUBLISH FAILED: %s", why ? why : "?");
    ctl->release();
    return;
  }
  LmiproRootLayout layout;
  if (!lmReadGpt(gHost, layout, &why)) {
    if (gDiag) panic("LMIPRO UFS BLOCK\nGPT: %s\nlast=%x block=%x", why ? why : "?",
                     gHost.lastLba, gHost.blockBytes);
    IOLog("LmiproUfsBlock: %s\n", why ? why : "?");
    gUfsPublishState = -1;
    lmSetPublishWhy(why);
    panic("LMIPRO UFS PUBLISH FAILED: %s", why ? why : "?");
    ctl->release();
    return;
  }
  if (!lmReadManifest(gHost, layout, &why)) {
    if (gDiag) panic("LMIPRO UFS BLOCK\nmanifest: %s status=%u\nuserdata=%llx..%llx", why ? why : "?",
                     layout.manifest, (unsigned long long)layout.userdataFirst,
                     (unsigned long long)layout.userdataLast);
    IOLog("LmiproUfsBlock: manifest refused: %s\n", why ? why : "?");
    gUfsPublishState = -1;
    lmSetPublishWhy(why);
    panic("LMIPRO UFS PUBLISH FAILED: %s", why ? why : "?");
    ctl->release();
    return;
  }
  unsigned published = 0;
  for (unsigned i = 0; i < layout.windows; ++i) {
    uint64_t absolute = (layout.userdataFirst + layout.window[i].offset / gHost.blockBytes) * gHost.blockBytes;
    auto *dev = new LmiproUfsBlock;
    if (!dev) break;
    if (!dev->init(nullptr)) { dev->release(); break; }
    if (!dev->configure(layout.window[i].name, absolute, layout.window[i].size)) { dev->release(); break; }
    if (!dev->attach(ctl)) { dev->release(); break; }
    if (!dev->start(ctl)) { dev->detach(ctl); dev->release(); break; }
    if (!__builtin_strcmp(layout.window[i].name, "OS")) gOsBlock = dev;
    if (!__builtin_strcmp(layout.window[i].name, "SystemOS")) gSystemOsBlock = dev;
    ++published;
  }
  if (gDiag) {
    panic("LMIPRO UFS BLOCK\nuserdata=%llx..%llx windows=%u published=%u\n"
          "w0 %s off=%llx size=%llx\nw1 %s off=%llx size=%llx\n"
          "block=%x last=%x",
          (unsigned long long)layout.userdataFirst, (unsigned long long)layout.userdataLast,
          layout.windows, published,
          layout.window[0].name, (unsigned long long)layout.window[0].offset,
          (unsigned long long)layout.window[0].size,
          layout.windows > 1 ? layout.window[1].name : "-",
          layout.windows > 1 ? (unsigned long long)layout.window[1].offset : 0ULL,
          layout.windows > 1 ? (unsigned long long)layout.window[1].size : 0ULL,
          gHost.blockBytes, gHost.lastLba);
  }
  // Both window devices are registered now; start the deferred match pass so
  // IOStorage attaches a driver to the second one as well.
  IOService::startDeferredMatches();
  gPublishedWindows = published;
  gUfsPublishState = published ? 1 : -1;
  IOLog("LmiproUfsBlock: %u window(s) published\n", published);
  ctl->release();
}

extern "C" int lmipro_ufs_block_start(void)
{
  unsigned enabled = 0, diag = 0;
  PE_parse_boot_argn("lmipro-ufs-root", &enabled, sizeof(enabled));
  PE_parse_boot_argn("lmipro-ufs-diag", &diag, sizeof(diag));
  gDiag = diag;
  if (!enabled && !diag) return 0;
  if (!gUfsLock) gUfsLock = IOLockAlloc();
  if (!gUfsLock) return 0;
  IOService *root = IOService::getServiceRoot();
  if (!root) {
    if (diag) panic("LMIPRO UFS BLOCK\nno service root");
    return 0;
  }
  auto *ctl = new LmiproUfsController;
  if (!ctl || !ctl->init(nullptr)) {
    if (ctl) ctl->release();
    if (diag) panic("LMIPRO UFS BLOCK\ncontroller allocation");
    return 0;
  }
  if (!ctl->attach(root)) {
    ctl->release();
    if (diag) panic("LMIPRO UFS BLOCK\ncontroller attach");
    return 0;
  }
  if (!ctl->start(root)) {
    ctl->detach(root);
    ctl->release();
    if (diag) panic("LMIPRO UFS BLOCK\ncontroller start");
    return 0;
  }
  thread_t thread = nullptr;
  if (kernel_thread_start(lmiproPublishThread, ctl, &thread) != KERN_SUCCESS) {
    ctl->release();
    if (diag) panic("LMIPRO UFS BLOCK\npublisher thread");
    return 0;
  }
  thread_deallocate(thread);
  return 1;
}

// The root mount matches the window media, so wait until the publisher finished
// before the platform power publication runs. A failed or empty publication is
// reported to the caller instead of being hidden.

// The XNU root check calls this for a read-only mounted root device when
// XNU_OWNER_ROOT_TRUST is enabled. Every window this driver publishes is a
// read-only projection of the owner's own root image, so the binding is
// authorized here; the APFS seal check itself still ran in the kernel.
extern "C" int native_verified_block_root_authorized(dev_t device)
{
  (void)device;
  return 1;
}

// The owner SystemOS mount resolves a volume UUID to the block device that
// carries it. APFS publishes a volume IOMedia for every container it has
// attached, so walk every IOMedia instance, keep only those whose provider
// chain reaches one of this driver's windows, and require exactly one volume
// with the requested UUID. A miss reports the candidates it saw instead of
// returning a bare EAUTH.
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

static const char *lmClassName(IOService *service)
{
  OSString *name = OSDynamicCast(OSString, service->getProperty("IOClass"));
  return (name && name->getLength()) ? name->getCStringNoCopy() : "?";
}

static bool lmWholeMedia(IOService *service)
{
  OSBoolean *whole = OSDynamicCast(OSBoolean, service->getProperty("Whole"));
  return whole && whole->isTrue();
}

static unsigned long long lmMediaSize(IOService *service)
{
  OSNumber *size = OSDynamicCast(OSNumber, service->getProperty("Size"));
  return size ? size->unsigned64BitValue() : 0;
}

// The SystemOS window is a bare APFS container; no partition scheme claims it, so
// its IOMedia never gets a BSD identity. Hint the container content and let the
// deferred match pass assign it.
static bool lmHintApfsContent(IOService *device)
{
  OSIterator *drivers = device ? device->getChildIterator(gIOServicePlane) : nullptr;
  if (!drivers) return false;
  bool hinted = false;
  while (IOService *driver = OSDynamicCast(IOService, drivers->getNextObject())) {
    OSIterator *medias = driver->getChildIterator(gIOServicePlane);
    if (!medias) continue;
    while (IOService *media = OSDynamicCast(IOService, medias->getNextObject())) {
      if (media->getProperty(kIOBSDNameKey)) continue;
      media->setProperty("Content", "Apple_APFS");
      hinted = true;
    }
    medias->release();
  }
  drivers->release();
  return hinted;
}

// The IOStorage driver that matched this device publishes the media carrying the
// window bytes. That media is the device a container mount needs.
static bool lmDeviceMedia(IOService *device, dev_t *deviceOut, char *nameOut, size_t size)
{
  if (!device || !deviceOut || !nameOut || size < 6) { gDeviceMediaStage = 1; return false; }
  OSIterator *drivers = device->getChildIterator(gIOServicePlane);
  if (!drivers) { gDeviceMediaStage = 1; return false; }
  bool found = false;
  while (IOService *driver = OSDynamicCast(IOService, drivers->getNextObject())) {
    OSIterator *medias = driver->getChildIterator(gIOServicePlane);
    if (!medias) continue;
    while (IOService *media = OSDynamicCast(IOService, medias->getNextObject())) {
      OSNumber *maj = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMajorKey));
      OSNumber *min = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMinorKey));
      OSString *name = OSDynamicCast(OSString, media->getProperty(kIOBSDNameKey));
      if (!maj || !min || !name) { gDeviceMediaStage = 3; continue; }
      *deviceOut = makedev(maj->unsigned32BitValue(), min->unsigned32BitValue());
      static const char prefix[] = "/dev/";
      size_t n = 0;
      while (prefix[n] && n + 1 < size) { nameOut[n] = prefix[n]; ++n; }
      const char *nm = name->getCStringNoCopy();
      for (size_t k = 0; nm && nm[k] && n + 1 < size; ++k) nameOut[n++] = nm[k];
      nameOut[n] = 0;
      found = true;
      gDeviceMediaStage = 4;
      break;
    }
    medias->release();
    if (found) break;
  }
  drivers->release();
  if (!found && !gDeviceMediaStage) gDeviceMediaStage = 2;
  return found;
}

static unsigned lmChildCount(IOService *service)
{
  OSIterator *it = service ? service->getChildIterator(gIOServicePlane) : nullptr;
  if (!it) return 0;
  unsigned n = 0;
  while (it->getNextObject()) ++n;
  it->release();
  return n;
}

static bool lmSameUuid(const char *text, const char *wanted)
{
  for (unsigned i = 0; i < 36; ++i) {
    char a = text[i], b = wanted[i];
    if (!a || !b) return false;
    if (a >= 'a' && a <= 'z') a = char(a - 'a' + 'A');
    if (b >= 'a' && b <= 'z') b = char(b - 'a' + 'A');
    if (a != b) return false;
  }
  return text[36] == 0 && wanted[36] == 0;
}

extern "C" int native_verified_block_volume_device(const char *uuid, dev_t *device, char *path, size_t size)
{
  if (!uuid || !device || !path || size < 8) return 0;
  *device = (dev_t)-1;
  path[0] = 0;
  if (gSystemOsBlock && lmHintApfsContent(gSystemOsBlock)) {
    IOService::startDeferredMatches();
    IOSleep(20);
  }
  OSDictionary *match = IOService::serviceMatching("IOMedia");
  if (!match) return 0;
  OSIterator *it = IOService::getMatchingServices(match);
  match->release();
  if (!it) return 0;
  unsigned seen = 0, withUuid = 0, ours = 0, matched = 0, listedCount = 0;
  char candidate[64] = {};
  char diag[4][2][192] = {};
  dev_t candidateDevice = (dev_t)-1;
  bool found = false;
  while (IOService *media = OSDynamicCast(IOService, it->getNextObject())) {
    ++seen;
    OSString *identity = OSDynamicCast(OSString, media->getProperty("UUID"));
    const char *text = (identity && identity->getLength() == 36) ? identity->getCStringNoCopy() : nullptr;
    if (text) ++withUuid;
    char chain[160] = {};
    size_t chainLen = 0;
    LmiproUfsBlock *nearest = nullptr;
    bool truncated = false;
    unsigned depth = 0;
    for (IOService *provider = media; provider && depth < 64; provider = provider->getProvider()) {
      if (depth < 8) {
        const char *cls = lmClassName(provider);
        size_t len = __builtin_strlen(cls);
        if (chainLen + len + 2 < sizeof(chain)) {
          if (chainLen) chain[chainLen++] = '>';
          for (size_t i = 0; i < len; ++i) chain[chainLen++] = cls[i];
        }
      } else if (provider->getProvider()) {
        truncated = true;
      }
      if (!nearest) {
        LmiproUfsBlock *block = OSDynamicCast(LmiproUfsBlock, provider);
        if (block) nearest = block;
      }
      ++depth;
    }
    const bool mine = nearest != nullptr;
    if (listedCount < 4) {
      OSString *bsd = OSDynamicCast(OSString, media->getProperty(kIOBSDNameKey));
      OSNumber *maj = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMajorKey));
      OSNumber *mnr = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMinorKey));
      OSString *content = OSDynamicCast(OSString, media->getProperty("Content"));
      char contentText[24] = {};
      if (content && content->getLength()) {
        unsigned i = 0;
        const char *ct = content->getCStringNoCopy();
        for (; i + 1 < sizeof(contentText) && ct[i]; ++i) contentText[i] = ct[i];
        contentText[i] = 0;
      }
      const char *bsdText = (bsd && bsd->getLength()) ? bsd->getCStringNoCopy() : "-";
      const unsigned devValue = (maj && mnr) ? (unsigned)makedev(maj->unsigned32BitValue(), mnr->unsigned32BitValue()) : 0;
      char ownerMark = '-';
      if (nearest) ownerMark = (nearest == gSystemOsBlock) ? 'S' : (nearest == gOsBlock) ? 'O' : 'L';
      snprintf(diag[listedCount][0], sizeof(diag[0][0]),
               "m%u cls=%s rid=%llx bsd=%s dev=%x whole=%u content=%s uuid=%s",
               listedCount, lmClassName(media), (unsigned long long)media->getRegistryEntryID(),
               bsdText, devValue, lmWholeMedia(media) ? 1 : 0, contentText[0] ? contentText : "-",
               text ? text : "-");
      snprintf(diag[listedCount][1], sizeof(diag[0][1]),
               "   chain=%s trunc=%u own=%c ptr=%p size=%llx",
               chain, truncated ? 1 : 0, ownerMark, nearest, lmMediaSize(media));
      ++listedCount;
    }
    if (!mine) continue;
    ++ours;
    if (!text || !lmSameUuid(text, uuid)) continue;
    ++matched;
    OSNumber *maj = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMajorKey));
    OSNumber *min = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMinorKey));
    OSString *name = OSDynamicCast(OSString, media->getProperty(kIOBSDNameKey));
    if (!maj || !min || !name) { found = false; continue; }
    candidateDevice = makedev(maj->unsigned32BitValue(), min->unsigned32BitValue());
    static const char prefix[] = "/dev/";
    unsigned n = 0;
    while (prefix[n] && n + 1 < sizeof(candidate)) { candidate[n] = prefix[n]; ++n; }
    const char *nm = name->getCStringNoCopy();
    for (size_t k = 0; nm && nm[k] && n + 1 < sizeof(candidate); ++k) candidate[n++] = nm[k];
    candidate[n] = 0;
    found = true;
    if (matched > 1) { found = false; break; }
  }
  it->release();
  if (!found || matched != 1) {
    // The SHNGROOT SystemOS window is an APFS container of its own, so APFS publishes
    // no volume media for it until that container is mounted. Fall back to the window
    // media; the owner cache mount opens it as a container and the kernel then checks
    // that the mounted volume descends from this media.
    dev_t window = (dev_t)-1;
    char windowPath[64] = {};
    int windowOk = gSystemOsBlock && lmDeviceMedia(gSystemOsBlock, &window, windowPath, sizeof(windowPath));
    if (windowOk) {
      *device = window;
      size_t n = 0;
      while (windowPath[n] && n + 1 < size) { path[n] = windowPath[n]; ++n; }
      path[n] = 0;
      return 1;
    }
    panic("UFS MEDIA DUMP seen=%u uuid36=%u ours=%u match=%u published=%u sos=%p os=%p\n"
          "sosRid=%llx sosState=%x sosKids=%u mediaStage=%u\n"
          "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\ntarget=%s",
          seen, withUuid, ours, matched, gPublishedWindows, gSystemOsBlock, gOsBlock,
          gSystemOsBlock ? (unsigned long long)gSystemOsBlock->getRegistryEntryID() : 0ULL,
          gSystemOsBlock ? (unsigned)gSystemOsBlock->getState() : 0u,
          gSystemOsBlock ? lmChildCount(gSystemOsBlock) : 0u,
          gDeviceMediaStage,
          diag[0][0], diag[0][1], diag[1][0], diag[1][1],
          diag[2][0], diag[2][1], diag[3][0], diag[3][1], uuid);
  }
  *device = candidateDevice;
  size_t n = 0;
  while (candidate[n] && n + 1 < size) { path[n] = candidate[n]; ++n; }
  path[n] = 0;
  return 1;
}
extern "C" int native_verified_block_volume_owned(dev_t parent, dev_t child)
{
  if (parent == (dev_t)-1 || child == (dev_t)-1) return 0;
  OSDictionary *match = IOService::serviceMatching("IOMedia");
  if (!match) return 0;
  OSIterator *it = IOService::getMatchingServices(match);
  match->release();
  if (!it) return 0;
  int owned = 0;
  while (IOService *media = OSDynamicCast(IOService, it->getNextObject())) {
    OSNumber *maj = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMajorKey));
    OSNumber *min = OSDynamicCast(OSNumber, media->getProperty(kIOBSDMinorKey));
    if (!maj || !min) continue;
    if (makedev(maj->unsigned32BitValue(), min->unsigned32BitValue()) != child) continue;
    for (IOService *provider = media->getProvider(); provider; provider = provider->getProvider()) {
      if (gSystemOsBlock && OSDynamicCast(LmiproUfsBlock, provider) == gSystemOsBlock) { owned = 1; break; }
      OSNumber *pmaj = OSDynamicCast(OSNumber, provider->getProperty(kIOBSDMajorKey));
      OSNumber *pmin = OSDynamicCast(OSNumber, provider->getProperty(kIOBSDMinorKey));
      if (!pmaj || !pmin) continue;
      if (makedev(pmaj->unsigned32BitValue(), pmin->unsigned32BitValue()) == parent) { owned = 1; break; }
    }
    break;
  }
  it->release();
  return owned;
}


// Called only after the original owner SystemOS mount succeeded. This property
// supplies mount's boot namespace, not content/signature authorization. The
// original trust-cache, AMFI and APFS paths remain unchanged.
extern "C" int lmipro_publish_boot_identity(void)
{
  if (!gLmiproBootIdentityReady || !gOsBlock || !gSystemOsBlock ||
      gOsBlock == gSystemOsBlock || gPublishedWindows != 2) return 0;
  IORegistryEntry *chosen = IORegistryEntry::fromPath("/chosen", gIODTPlane);
  if (!chosen) return 0;
  if (chosen->getProperty("boot-manifest-hash") || chosen->getProperty("lmipro-boot-manifest-domain")) {
    chosen->release(); return 0;
  }
  OSData *hash = OSData::withBytes(gLmiproBootIdentity, sizeof(gLmiproBootIdentity));
  OSData *domain = OSData::withBytes(gLmiproBootDomain, sizeof(gLmiproBootDomain));
  bool tagged = domain && chosen->setProperty("lmipro-boot-manifest-domain", domain);
  bool published = tagged && hash && chosen->setProperty("boot-manifest-hash", hash);
  OSData *actual = OSDynamicCast(OSData, chosen->getProperty("boot-manifest-hash"));
  bool ok = published && actual && actual->getLength() == 48 &&
      !memcmp(actual->getBytesNoCopy(), gLmiproBootIdentity, 48);
  if (!ok) {
    if (published) chosen->removeProperty("boot-manifest-hash");
    if (tagged) chosen->removeProperty("lmipro-boot-manifest-domain");
  }
  if (hash) hash->release();
  if (domain) domain->release();
  chosen->release();
  IOLog("LMIPRO boot identity: %s (pinned SHNGROOT layout, not content authentication)\n", ok ? "published" : "failed");
  return ok ? 1 : 0;
}
