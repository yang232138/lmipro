// SPDX-License-Identifier: BSD-2-Clause-Patent
// Mode 25: read-only precheck. Mode 26: exclusive mapping and one bounded NOP.
// No SCSI, disk I/O, reset, clock or power writes. DMA retained until reboot.
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


// SPDX-License-Identifier: BSD-2-Clause-Patent
// Plan before any SMMU write. Interface register facts: ARM SMMUv2.
struct UfsSmmuPlan {
  unsigned slot = 0xffffffffU, bank = 0, pages = 0;
  uint32_t smr = 0, s2cr = 0;
  bool reuse = false;
  const char *error = nullptr;
};
// ARM IHI 0062D.c sections 1.6 and 3.7: VA64=0 still rejects inputs above
// 32 bits with SCTLR.M=0. With VA64=1 the bypass range is bounded by all
// three hardware widths, not TCR2.PASize (which only limits translation).
static inline unsigned ufsSmmuAddressBits(unsigned code)
{
  switch (code) {
    case 0: return 32; case 1: return 36; case 2: return 40;
    case 3: return 42; case 4: return 44; case 5: return 48;
    default: return 0;
  }
}
// One attempt with a given context bank (counted from the top). The caller falls back to lower banks when this one is held by another stream.
template <class Read>
static UfsSmmuPlan planUfsSmmuBank(uint16_t stream, uint64_t dma, uint64_t bytes, Read read, unsigned bankFromTop)
{
  UfsSmmuPlan p;
  uint32_t idr0 = read(0x20), idr2 = read(0x28);
  unsigned count = idr0 & 255;
  if (!(idr0 & (1U << 30)) || !(idr2 & (1U << 12))) {
    p.error = "SMMU AArch64 stage 1 unsupported"; return p;
  }
  for (unsigned shift = 0; shift <= 8; shift += 4) {
    unsigned bits = ufsSmmuAddressBits((idr2 >> shift) & 15);
    if (!bits || !bytes || dma >= (1ULL << bits) || bytes > (1ULL << bits) - dma) {
      p.error = "DMA exceeds SMMU address width"; return p;
    }
  }
  uint32_t idr1 = read(0x24);
  unsigned banks = idr1 & 255;
  p.pages = 1U << (((idr1 >> 28) & 7) + 1);
  if ((idr1 >> 31) || !count || p.pages > 128 || banks < 2 || banks > 128 ||
      (p.pages + banks) * 4096 > 0xd0000) {
    p.error = "unsupported SMMU geometry"; return p;
  }
  if (count > 128) count = 128; // Firmware's reliably accessible groups only.
  if (bankFromTop >= banks - 1) {p.error = "no free context bank"; return p;}
  p.bank = banks - 1 - bankFromTop;
  unsigned matches = 0;
  for (unsigned i = 0; i < count; ++i) {
    uint32_t smr = read(0x800 + 4 * i);
    if (!(smr & 0x80000000U)) {
      if (!p.reuse) p.slot = i;
      continue;
    }
    uint32_t s2cr = read(0xc00 + 4 * i);
    uint32_t mask = (smr >> 16) & 0x7fff;
    bool match = ((smr ^ stream) & 0xffff & ~mask) == 0;
    bool ourBank = ((s2cr >> 16) & 3) == 0 && (s2cr & 255) == p.bank;
    if (match) {
      ++matches;
      if (matches > 1 || mask || !ourBank) {
        p.slot = i; p.smr = smr; p.s2cr = s2cr;
        p.error = "conflicting UFS stream mapping"; return p;
      }
      p.reuse = true; p.slot = i;
    } else if (ourBank) {
      p.slot = i; p.smr = smr; p.s2cr = s2cr;
      p.error = "reserved context bank already used"; return p;
    }
  }
  uint32_t sctlr = read((p.pages + p.bank) * 4096);
  if ((sctlr & 1) || (p.reuse && (sctlr || read(0x1000 + 4 * p.bank) != 0x10000 ||
                                (read(0x1800 + 4 * p.bank) & ~1U)))) {
    p.error = "reserved context bank not bypass"; return p;
  }
  if (p.slot == 0xffffffffU) p.error = "no free stream-match group";
  return p;
}

// The UFS stream's context bank: the top bank first. A stream mapping left behind by an earlier run (or by another OS that used the SMMU before a
// warm reset) can still point another stream at that bank; rather than refuse to boot, the next banks down are tried, each held to the same checks
// (no other valid stream maps it, translation off). Any other refusal (a conflicting mapping of our own stream, no free group, unsupported geometry)
// is final and is reported as the first attempt saw it.
template <class Read>
static UfsSmmuPlan planUfsSmmu(uint16_t stream, uint64_t dma, uint64_t bytes, Read read)
{
  enum {kBankAttempts = 8};
  UfsSmmuPlan first = planUfsSmmuBank(stream, dma, bytes, read, 0);
  if (!first.error) return first;
  // "conflicting" is retried too: our own stream may already sit on a lower bank from an earlier run that had to fall back.
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

// The caller must first quiesce the UFS queue. The planner refuses other
// streams sharing this bank and enabled translation; no live context is
// disabled here. Write callbacks must complete an MMIO barrier before return.
template <class Read, class Write>
static const char *configureUfsSmmu(const UfsSmmuPlan &p, uint16_t stream, Read read, Write write)
{
  if (p.error) return p.error;
  unsigned cb = (p.pages + p.bank) * 4096;
  if (!p.reuse) write(cb, 0);
  if (read(cb) != 0) return "SMMU bypass disable readback";
  // Set VA64 before exposing a newly allocated stream. Repair an exclusive
  // existing 32-bit bypass too; do not silently reuse the faulty state.
  if (read(0x1800 + 4 * p.bank) != 1) write(0x1800 + 4 * p.bank, 1);
  if (!p.reuse) write(0x1000 + 4 * p.bank, 0x10000);
  if (read(0x1800 + 4 * p.bank) != 1 || read(0x1000 + 4 * p.bank) != 0x10000 || read(cb) != 0)
    return "SMMU VA64 bypass readback";
  if (!p.reuse) {
    write(0xc00 + 4 * p.slot, p.bank);
    if (read(0xc00 + 4 * p.slot) != p.bank) return "SMMU stream context readback";
    write(0x800 + 4 * p.slot, 0x80000000U | stream);
  }
  if (read(0x800 + 4 * p.slot) != (0x80000000U | stream) ||
      read(0xc00 + 4 * p.slot) != p.bank)
    return "SMMU stream mapping readback";
  return nullptr;
}

// SPDX-License-Identifier: BSD-2-Clause-Patent
// Single-slot completion policy. Port supplies ordered MMIO, invalidate-only
// descriptor reads, a monotonic deadline and bounded pacing. No resubmission.
enum class UfsTransferEnd { Completed, Fatal, Timeout };
struct UfsTransferPoll {
  uint32_t polls = 0, doorbellSeen = 0, interruptSeen = 0, ocs = 15;
};
static constexpr uint32_t kUfsFatalStatus = 0x71880U;

template <class Port>
static UfsTransferEnd pollUfsTransfer(Port &port, UfsTransferPoll &state)
{
  for (;;) {
    ++state.polls;
    uint32_t doorbell = port.doorbell();
    state.doorbellSeen |= doorbell;
    state.interruptSeen |= port.interrupt();
    if (state.interruptSeen & kUfsFatalStatus) return UfsTransferEnd::Fatal;
    if (!(doorbell & 1)) {
      state.ocs = port.completedOcs();
      // A zero doorbell alone does not prove that this command was accepted.
      // Also allow a completion so fast that software never observes DB=1.
      if (state.ocs != 15) return UfsTransferEnd::Completed;
    }
    if (port.expired()) return UfsTransferEnd::Timeout;
    port.delay();
  }
}

// Single NOP, no SCSI/UIC command. Adapted from pinned ShengUfs transport.
struct LmiproNopResult {
  const char *error = nullptr;
  unsigned stage = 0, slot[2] = {}, bank[2] = {}, polls = 0;
  uint32_t ocs = 15, response = 0, db = 0, status = 0;
};
template<class Ops>
static LmiproNopResult lmiproNop(Ops &io) {
  LmiproNopResult r;
  auto fail = [&](const char *why) { r.error=why; return r; };
  auto hr = [&](unsigned off) { return io.hostRead(off); };
  auto sr = [&](unsigned off) { return io.smmuRead(off); };
  auto idle = [&]() {
    return (hr(0x34)&1) && (hr(0x30)&15)==15 &&
           !hr(0x58) && !hr(0x78) && !(hr(0x20)&kUfsFatalStatus);
  };
  r.stage=1;
  if (!idle()) return fail("controller not idle/ready");
  if (!io.dma || (io.dma&4095) || io.bytes<16384 || io.dma>UINT64_MAX-16384 ||
      (!(hr(0)&0x01000000) && io.dma+16384>0x100000000ULL)) return fail("DMA range");
  if ((sr(0)&9) || !(sr(0x20)&(1U<<27))) return fail("SMMU mode");
  r.stage=2;
  UfsSmmuPlan p[2];
  p[0]=planUfsSmmu(0xe0,io.dma,16384,sr);
  if (p[0].error) return fail(p[0].error);
  if (p[0].reuse) return fail("existing e0 mapping not owned");
  // Plan both exclusive banks before changing any register. The second plan
  // sees the first reservation and must select a different bank and slot.
  auto reservedRead = [&](unsigned off) {
    if (off==0x800+4*p[0].slot) return 0x800000e0U;
    if (off==0xc00+4*p[0].slot) return uint32_t(p[0].bank);
    return sr(off);
  };
  p[1]=planUfsSmmu(0x4e0,io.dma,16384,reservedRead);
  if (p[1].error) return fail(p[1].error);
  if (p[1].reuse || p[1].slot==p[0].slot || p[1].bank==p[0].bank) return fail("mapping ownership");
  for(unsigned i=0;i<2;++i) { r.slot[i]=p[i].slot; r.bank[i]=p[i].bank; }
  io.delay();
  if (!idle()) return fail("controller changed before takeover");
  r.stage=3;
  io.hostWrite(0x24,0);
  io.hostWrite(0x60,0);
  unsigned loops=10000;
  while ((hr(0x60)&1) && loops--) io.delay();
  if ((hr(0x60)&1) || hr(0x58) || hr(0x78)) return fail("queue stop");
  r.stage=4;
  for(unsigned i=0;i<2;++i) {
    const uint16_t sid=i ? 0x4e0 : 0xe0;
    auto now=planUfsSmmu(sid,io.dma,16384,sr);
    if(now.error || now.reuse || now.slot!=p[i].slot || now.bank!=p[i].bank)
      return fail("mapping changed before claim");
    const char *why=configureUfsSmmu(now,sid,sr,
        [&](unsigned off,uint32_t value){io.smmuWrite(off,value);});
    if(why) return fail(why);
  }
  r.stage=5;
  // Dedicated allocation only: never dereference or overwrite firmware UTRLBA.
  for(unsigned i=0;i<16384;++i) io.data[i]=0;
  auto *utrd=reinterpret_cast<uint32_t *>(io.data);
  const uint64_t ucd=io.dma+4096;
  utrd[0]=0x10000000; utrd[2]=15;
  utrd[4]=uint32_t(ucd); utrd[5]=uint32_t(ucd>>32);
  utrd[6]=128U|(128U<<16); // 512 byte response at command + 512.
  io.data[4096+3]=1; // NOP OUT, task tag 1; no PRDT and no data transfer.
  io.clean();
  io.hostWrite(0x50,uint32_t(io.dma));
  io.hostWrite(0x54,uint32_t(io.dma>>32));
  if(hr(0x50)!=uint32_t(io.dma) || hr(0x54)!=uint32_t(io.dma>>32)) return fail("list readback");
  if (!idle()) return fail("controller changed before start");
  io.hostWrite(0x20,1); // Completion only; never clear fatal/UIC fault evidence.
  io.hostWrite(0x60,1);
  loops=10000;
  while (!(hr(0x60)&1) && loops--) io.delay();
  if(!(hr(0x60)&1) || !idle()) return fail("queue start");
  r.stage=6;
  io.hostWrite(0x58,1); // Exactly one submission.
  for (r.polls=0;r.polls<100000;++r.polls) {
    r.db=hr(0x58); r.status=hr(0x20);
    if(r.status&kUfsFatalStatus) return fail("fatal status; DMA quarantined");
    if(!(r.db&1)) {
      io.invalidate(); r.ocs=utrd[2]&255;
      if(r.ocs!=15) break;
    }
    io.delay();
  }
  if(r.polls==100000) return fail("timeout; DMA quarantined");
  r.stage=7;
  if(r.ocs) return fail("descriptor completion error");
  const uint8_t *rsp=io.data+4096+512;
  r.response=(uint32_t(rsp[0])<<24)|(uint32_t(rsp[1])<<16)|(uint32_t(rsp[2])<<8)|rsp[3];
  if(rsp[0]!=0x20 || rsp[2]!=0 || rsp[3]!=1 || rsp[6] || rsp[7] || rsp[8] || rsp[10] || rsp[11])
    return fail("invalid NOP IN response");
  r.stage=8;
  io.hostWrite(0x60,0);
  loops=10000;
  while((hr(0x60)&1) && loops--) io.delay();
  if(hr(0x60)&1) return fail("completed; queue stop failed");
  return r;
}


struct LmiproReadResult {
  const char *error=nullptr;
  unsigned stage=0,commands=0,polls=0;
  uint32_t ocs=15,scsi=0xffff,sense=0,blockBytes=0,lastLba=0,headerCrc=0,arrayCrc=0;
  uint64_t first=0,last=0;
  uint32_t magic0=0,magic1=0;
};
static inline uint32_t lmiBe(const uint8_t *p) {return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
static inline uint32_t lmiLe(const uint8_t *p) {return uint32_t(p[3])<<24|uint32_t(p[2])<<16|uint32_t(p[1])<<8|p[0];}
static inline uint64_t lmiLe64(const uint8_t *p) {return uint64_t(lmiLe(p))|(uint64_t(lmiLe(p+4))<<32);}
static inline uint32_t lmiCrc(uint32_t crc,const uint8_t *p,unsigned n) {
  for(unsigned i=0;i<n;++i) {crc^=p[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320U&(0U-(crc&1)));}return crc;
}
// Called only after a validated NOP and stopped, exclusively owned queue.
// Fixed allowlist: READ CAPACITY(10) and READ(10), LUN 0, device-to-host only.
template<class Ops>
static LmiproReadResult lmiproReadDisk(Ops &io) {
  LmiproReadResult r;
  auto fail=[&](const char *s){r.error=s;return r;};
  auto command=[&](uint8_t opcode,uint32_t lba,unsigned bytes)->const char * {
    if((opcode!=0x25 && opcode!=0x28) || !bytes || bytes>4096 || (bytes&3) || r.commands>=35)
      return "read command bounds";
    if(io.bytes<16384 || io.hostRead(0x58) || io.hostRead(0x78) || (io.hostRead(0x20)&kUfsFatalStatus))
      return "read queue not idle";
    for(unsigned i=0;i<16384;++i)io.data[i]=0;
    auto *w=reinterpret_cast<uint32_t *>(io.data);
    uint64_t ucd=io.dma+4096,data=io.dma+8192;
    w[0]=0x14000000;w[2]=15;w[4]=uint32_t(ucd);w[5]=uint32_t(ucd>>32);
    w[6]=128U|(128U<<16);w[7]=1U|(256U<<16);
    auto *prd=reinterpret_cast<uint32_t *>(io.data+5120);
    prd[0]=uint32_t(data);prd[1]=uint32_t(data>>32);prd[3]=bytes-1;
    uint8_t *q=io.data+4096;q[0]=1;q[1]=0x40;q[3]=2;
    q[12]=uint8_t(bytes>>24);q[13]=uint8_t(bytes>>16);q[14]=uint8_t(bytes>>8);q[15]=uint8_t(bytes);
    q[16]=opcode;
    if(opcode==0x28) {q[18]=uint8_t(lba>>24);q[19]=uint8_t(lba>>16);q[20]=uint8_t(lba>>8);q[21]=uint8_t(lba);q[24]=1;}
    io.clean();io.hostWrite(0x20,1);io.hostWrite(0x60,1);
    if(!(io.hostRead(0x60)&1) || (io.hostRead(0x30)&15)!=15) return "read queue start";
    ++r.commands;io.hostWrite(0x58,1);
    for(r.polls=0;r.polls<500000;++r.polls) {
      if(io.hostRead(0x20)&kUfsFatalStatus)return "read fatal; DMA retained";
      if(!io.hostRead(0x58)) {io.invalidate();r.ocs=w[2]&255;if(r.ocs!=15)break;}
      io.delay();
    }
    if(r.polls==500000)return "read timeout; DMA retained";
    if(r.ocs)return "read OCS error";
    const uint8_t *rsp=io.data+4608;
    if(rsp[0]!=0x21 || rsp[2] || rsp[3]!=2 || rsp[8])return "read response header";
    unsigned len=unsigned(rsp[10])*256+rsp[11];
    if(len>480)return "read response length";
    r.scsi=unsigned(rsp[6])*256+rsp[7];r.sense=0;
    if(len) {
      unsigned n=unsigned(rsp[32])*256+rsp[33];if(len<2 || n>len-2)return "sense bounds";
      const uint8_t *s=rsp+34;
      if(n>=14 && ((s[0]&127)==0x70 || (s[0]&127)==0x71))r.sense=(uint32_t(s[2]&15)<<16)|(uint32_t(s[12])<<8)|s[13];
      if(n>=4 && ((s[0]&127)==0x72 || (s[0]&127)==0x73))r.sense=(uint32_t(s[1]&15)<<16)|(uint32_t(s[2])<<8)|s[3];
    }
    if(r.scsi)return "SCSI status; no retry";
    if((rsp[1]&0x60) || lmiBe(rsp+12))return "short/overrun read";
    return nullptr;
  };
  r.stage=10;
  const char *why=command(0x25,0,8);if(why)return fail(why);
  uint8_t *b=io.data+8192;
  r.lastLba=lmiBe(b);r.blockBytes=lmiBe(b+4);
  if(r.lastLba==0xffffffffU || r.lastLba<100 || (r.blockBytes!=512 && r.blockBytes!=4096))return fail("capacity geometry unsupported");
  r.stage=11;why=command(0x28,1,r.blockBytes);if(why)return fail(why);
  if(lmiLe(b)!=0x20494645 || lmiLe(b+4)!=0x54524150 || lmiLe(b+8)!=0x10000 || lmiLe(b+20) || lmiLe64(b+24)!=1)
    return fail("GPT header signature/revision");
  unsigned hlen=lmiLe(b+12);r.headerCrc=lmiLe(b+16);
  if(hlen<92 || hlen>r.blockBytes)return fail("GPT header size");
  b[16]=b[17]=b[18]=b[19]=0;
  if((lmiCrc(0xffffffffU,b,hlen)^0xffffffffU)!=r.headerCrc)return fail("GPT header CRC");
  uint64_t backup=lmiLe64(b+32),usableFirst=lmiLe64(b+40),usableLast=lmiLe64(b+48),entries=lmiLe64(b+72);
  unsigned count=lmiLe(b+80),entryBytes=lmiLe(b+84);r.arrayCrc=lmiLe(b+88);
  if(!count || count>128 || entryBytes!=128)return fail("GPT entry bounds");
  unsigned blocks=(count*128+r.blockBytes-1)/r.blockBytes;
  if(entries<2 || entries>64 || entries+blocks>usableFirst || usableFirst>usableLast || usableLast>=backup || backup!=r.lastLba || backup-usableLast<=blocks)
    return fail("GPT disk bounds");
  r.stage=12;unsigned matches=0;uint32_t crc=0xffffffffU;
  for(unsigned block=0;block<blocks;++block) {
    why=command(0x28,uint32_t(entries+block),r.blockBytes);if(why)return fail(why);
    unsigned remaining=count*128-block*r.blockBytes,n=remaining<r.blockBytes?remaining:r.blockBytes;
    crc=lmiCrc(crc,b,n);
    for(unsigned off=0;off<n;off+=128) {
      const uint8_t *e=b+off;unsigned used=0;for(unsigned i=0;i<16;++i)used|=e[i];if(!used)continue;
      const char name[]="userdata";bool match=true;
      for(unsigned i=0;i<9;++i)if(e[56+2*i]!=uint8_t(name[i]) || e[57+2*i])match=false;
      if(match) {++matches;r.first=lmiLe64(e+32);r.last=lmiLe64(e+40);}
    }
  }
  if((crc^0xffffffffU)!=r.arrayCrc)return fail("GPT array CRC");
  if(matches!=1 || r.first<usableFirst || r.last<r.first || r.last>usableLast)return fail("userdata missing/ambiguous/bounds");
  r.stage=13;why=command(0x28,uint32_t(r.first),r.blockBytes);if(why)return fail(why);
  // stage 14: the backup GPT header and its entry array must describe the same
  // disk as the primary header, verified with their own CRCs.
  r.stage=14;
  why=command(0x28,uint32_t(r.lastLba),r.blockBytes);if(why)return fail(why);
  if(lmiLe(b)!=0x20494645 || lmiLe(b+4)!=0x54524150 || lmiLe(b+8)!=0x10000)
  return fail("backup GPT signature/revision");
  if(lmiLe64(b+24)!=r.lastLba)return fail("backup GPT myLba");
  if(lmiLe64(b+32)!=1)return fail("backup GPT alternate");
  if(lmiLe64(b+72)!=entries || lmiLe(b+80)!=count || lmiLe(b+84)!=128 || lmiLe(b+88)!=r.arrayCrc)
  return fail("backup GPT geometry/crc field");
  if(lmiLe64(b+40)!=r.usableFirst || lmiLe64(b+48)!=r.usableLast)return fail("backup GPT usable range");
  {unsigned bhlen=lmiLe(b+12);
  if(bhlen<92 || bhlen>r.blockBytes)return fail("backup GPT header size");
  b[16]=b[17]=b[18]=b[19]=0;
  if((lmiCrc(0xffffffffU,b,bhlen)^0xffffffffU)!=lmiLe(b+16))return fail("backup GPT header CRC");}
  r.stage=15;
  {uint32_t bcrc=0xffffffffU;
  for(unsigned bblk=0;bblk<blocks;++bblk) {
  why=command(0x28,uint32_t(r.lastLba-blocks+bblk),r.blockBytes);if(why)return fail(why);
  unsigned bremaining=count*128-bblk*r.blockBytes,bn=bremaining<r.blockBytes?bremaining:r.blockBytes;
  bcrc=lmiCrc(bcrc,b,bn);}
  if((bcrc^0xffffffffU)!=r.arrayCrc)return fail("backup GPT array CRC");}
  r.stage=16;
  r.magic0=lmiLe(b);r.magic1=lmiLe(b+4);
  io.hostWrite(0x60,0);unsigned loops=10000;
  while((io.hostRead(0x60)&1) && loops--)io.delay();
  if(io.hostRead(0x60)&1)return fail("read complete; queue stop failed");
  r.stage=17;return r;
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
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <kern/task.h>
struct LmiproNopPort {
  uintptr_t hc,smmu;
  uint64_t dma;
  IOByteCount bytes;
  uint8_t *data;
  static uint32_t read(uintptr_t base,unsigned off) {
    uint32_t value=*reinterpret_cast<const volatile uint32_t *>(base+off);
    __asm__ volatile("dsb sy" ::: "memory"); return value;
  }
  static void write(uintptr_t base,unsigned off,uint32_t value) {
    __asm__ volatile("dsb sy" ::: "memory");
    *reinterpret_cast<volatile uint32_t *>(base+off)=value;
    __asm__ volatile("dsb sy" ::: "memory");
  }
  uint32_t hostRead(unsigned off) { return read(hc,off); }
  uint32_t smmuRead(unsigned off) {
    if ((off&3) || off>=0xd0000) panic("LMIPRO NOP: SMMU read bounds");
    return read(smmu,off);
  }
  void hostWrite(unsigned off,uint32_t value) {
    if(off!=0x20 && off!=0x24 && off!=0x50 && off!=0x54 && off!=0x58 && off!=0x60)
      panic("LMIPRO NOP: host write bounds");
    write(hc,off,value);
  }
  void smmuWrite(unsigned off,uint32_t value) { write(smmu,off,value); }
  void delay() { IODelay(2); }
  void clean() {
    for(uintptr_t p=reinterpret_cast<uintptr_t>(data);p<reinterpret_cast<uintptr_t>(data)+16384;p+=64)
      __asm__ volatile("dc civac, %0" :: "r"(p) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
  }
  void invalidate() {
    __asm__ volatile("dsb sy" ::: "memory");
    for(uintptr_t p=reinterpret_cast<uintptr_t>(data);p<reinterpret_cast<uintptr_t>(data)+16384;p+=64)
      __asm__ volatile("dc ivac, %0" :: "r"(p) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
  }
};
static void lmiproNopProbe(bool disk=false) {
  LmiproHostState h=lmiproHostState();
  if(h.mapped!=2) panic("LMIPRO UFS NOP v1: power/clock gate host=%u",h.mapped);
  IODeviceMemory *mem=IODeviceMemory::withRange(0x01d84000ULL,0x1000);
  if(!mem) panic("LMIPRO UFS NOP v1: host descriptor");
  OSSharedPtr<IOMemoryMap> hc(mem->map(kIOMapAnywhere|kIOMapInhibitCache),OSNoRetain); mem->release();
  mem=IODeviceMemory::withRange(0x15000000ULL,0xd0000);
  if(!mem) panic("LMIPRO UFS NOP v1: SMMU descriptor");
  OSSharedPtr<IOMemoryMap> smmu(mem->map(kIOMapAnywhere|kIOMapInhibitCache),OSNoRetain); mem->release();
  if(!hc || !smmu) panic("LMIPRO UFS NOP v1: MMIO map");
  // Retain allocation until reboot, including all timeout/fault paths. DMA may
  // still own it; there is no cancellation, retry, free or firmware-list reuse.
  auto *buffer=IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
    static_cast<IOOptionBits>(kIODirectionInOut)|static_cast<IOOptionBits>(kIOMemoryPhysicallyContiguous)|
    static_cast<IOOptionBits>(kIOMemoryMapperNone),16384,4096);
  if(!buffer) panic("LMIPRO UFS NOP v1: DMA allocation");
  IOByteCount count=0;
  uint64_t pa=buffer->getPhysicalSegment(0,&count,kIOMemoryMapperNone);
  auto *va=static_cast<uint8_t *>(buffer->getBytesNoCopy());
  if(!va || (reinterpret_cast<uintptr_t>(va)&63)) panic("LMIPRO UFS NOP v1: DMA CPU alignment");
  LmiproNopPort io={hc->getVirtualAddress(),smmu->getVirtualAddress(),pa,count,va};
  auto r=lmiproNop(io);
  if(disk && !r.error) {
    auto d=lmiproReadDisk(io);
    panic("LMIPRO UFS READ v1\n%s stage=%u cmds=%u\n"
          "ocs=%x scsi=%x sense=%06x\nblock=%u lastLba=%x\n"
          "GPT crc=%08x/%08x\nuserdata=%llx..%llx\nmagic=%08x %08x\n"
          "hcs=%08x db=%08x is=%08x gfsr=%08x\nNo disk writes; no root publication",
          d.error?d.error:"Capacity/GPT/userdata block read",d.stage,d.commands,d.ocs,d.scsi,d.sense,
          d.blockBytes,d.lastLba,d.headerCrc,d.arrayCrc,(unsigned long long)d.first,(unsigned long long)d.last,
          d.magic0,d.magic1,io.hostRead(0x30),io.hostRead(0x58),io.hostRead(0x20),io.smmuRead(0x48));
  }
  panic("LMIPRO UFS NOP v1\n%s stage=%u\n"
        "dma=%llx bytes=%llu\ne0 slot=%u bank=%u; 4e0 slot=%u bank=%u\n"
        "polls=%u ocs=%x response=%08x\nhcs=%08x hce=%08x db=%08x is=%08x\n"
        "gfsr=%08x syn=%08x/%08x/%08x\n"
        "No disk read/write; no root publication",
        r.error?r.error:"NOP IN validated",r.stage,(unsigned long long)pa,(unsigned long long)count,
        r.slot[0],r.bank[0],r.slot[1],r.bank[1],r.polls,r.ocs,r.response,
        io.hostRead(0x30),io.hostRead(0x34),io.hostRead(0x58),io.hostRead(0x20),
        io.smmuRead(0x48),io.smmuRead(0x50),io.smmuRead(0x54),io.smmuRead(0x58));
}

extern "C" void lmipro_ufs_handoff_probe(void)
{
  unsigned mode=0;
  if (!PE_parse_boot_argn("lmipro-ufs-handoff",&mode,sizeof(mode))) return;
  if (mode==27) { lmiproNopProbe(true); return; }
  if (mode==26) { lmiproNopProbe(); return; }
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
