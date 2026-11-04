#define LMIPRO_UIC_HOST_TEST
#include "../package/LmiproUfsHandoff.cpp"
#include <map>
#include <vector>
#include <cassert>
#include <cstdio>
struct Fake {
  uint64_t dma=0xc8000000; unsigned bytes=8192;
  alignas(64) uint8_t data[8192]={};
  std::map<unsigned,uint32_t> host,smmu;
  std::vector<unsigned> hw,sw;
  unsigned submits=0,invalidations=0,cleans=0;
  bool timeout=false,badTag=false,zeroDoorbell=false,fatal=false,stopFail=false,rejectMap=false;
  Fake() {
    host[0]=0x1587031f;host[0x30]=0x10f;host[0x34]=1;host[0x60]=1;
    smmu[0]=0x260406;smmu[0x20]=0x4c017e57;smmu[0x24]=0x6000004a;smmu[0x28]=0x5111;
    smmu[0x800]=0x800004a0; smmu[0xc00]=0;
  }
  uint32_t hostRead(unsigned o) {return host[o];}
  uint32_t smmuRead(unsigned o) {return smmu[o];}
  void smmuWrite(unsigned o,uint32_t v) { sw.push_back(o);if(!rejectMap)smmu[o]=v; }
  void hostWrite(unsigned o,uint32_t v) {
    hw.push_back(o);
    if(o==0x20) { host[o]&=~v;return; }
    if(stopFail && o==0x60 && !v) return;
    host[o]=v;
    if(o==0x58) {
      ++submits;
      assert(v==1 && cleans==1);
      assert(data[4096]==0 && data[4099]==1);
      assert(reinterpret_cast<uint32_t*>(data)[7]==0);
      if(fatal) host[0x20]=0x10000;
      if(zeroDoorbell) host[o]=0;
      if(timeout || fatal || zeroDoorbell) return;
      host[o]=0; reinterpret_cast<uint32_t*>(data)[2]=0;
      data[4608]=0x20;data[4611]=badTag?2:1;
    }
  }
  void clean() {++cleans;}
  void invalidate() {++invalidations;}
  void delay() {}
};
int main() {
  Fake ok;auto r=lmiproNop(ok);
  assert(!r.error && r.stage==8 && r.ocs==0 && r.response==0x20000001 && ok.submits==1);
  assert(r.bank[0]!=r.bank[1] && r.slot[0]!=r.slot[1]);
  assert(ok.smmu[0x800]==0x800004a0 && ok.smmu[0xc00]==0);
  Fake busy;busy.host[0x58]=2;r=lmiproNop(busy);assert(r.error && busy.hw.empty() && busy.sw.empty());
  Fake conflict;conflict.smmu[0x804]=0x800000e0;conflict.smmu[0xc04]=0;
  r=lmiproNop(conflict);assert(r.error && conflict.hw.empty() && conflict.sw.empty());
  Fake shared;shared.smmu[0xc00]=73;r=lmiproNop(shared);assert(!r.error && r.bank[0]!=73 && r.bank[1]!=73);
  Fake range;range.dma=1ULL<<40;r=lmiproNop(range);assert(r.error && range.hw.empty());
  Fake unsupported;unsupported.smmu[0]=8;r=lmiproNop(unsupported);assert(r.error && unsupported.hw.empty());
  Fake stop;stop.stopFail=true;r=lmiproNop(stop);assert(r.error && !stop.submits && stop.sw.empty());
  Fake reject;reject.rejectMap=true;r=lmiproNop(reject);assert(r.error && !reject.submits);
  Fake timeout;timeout.timeout=true;r=lmiproNop(timeout);assert(r.error && r.stage==6 && timeout.submits==1 && r.polls==100000);
  Fake zero;zero.zeroDoorbell=true;r=lmiproNop(zero);assert(r.error && r.ocs==15 && zero.submits==1);
  Fake fatal;fatal.fatal=true;r=lmiproNop(fatal);assert(r.error && r.stage==6 && fatal.submits==1);
  Fake tag;tag.badTag=true;r=lmiproNop(tag);assert(r.error && r.stage==7 && tag.submits==1);
  puts("LMIPRO NOP: ownership, bounded failure, descriptor and response tests passed");
}
