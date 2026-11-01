#define LMIPRO_UIC_HOST_TEST
#include "../package/LmiproUfsHandoff.cpp"
#include <cassert>
#include <vector>
#include <utility>
#include <cstdio>
struct Fake {
  uint32_t hcs=0x10f, is=0x900, ie=0x12345678, result=0, data=2;
  unsigned finish=2, count=0, delays=0;
  bool submitted=false, uicError=false;
  std::vector<std::pair<unsigned,uint32_t>> writes;
  uint32_t read32(unsigned off) {
    if (off==0x30) return hcs;
    if (off==0x24) return ie;
    if (off==0x98) return result;
    if (off==0x9c) return data;
    assert(off==0x20);
    if (submitted && ++count >= finish && finish) return uicError ? 4U : 0x400U;
    return is;
  }
  void write32(unsigned off,uint32_t v) {
    writes.push_back({off,v});
    if(off==0x24) ie=v;
    if(off==0x20) { assert(v==0x400); is &= ~v; }
    if(off==0x90) { assert(v==1); submitted=true; is=0; }
    assert(off==0x24||off==0x20||off==0x94||off==0x98||off==0x9c||off==0x90);
  }
  void delayUs(unsigned us) { assert(us==1); ++delays; }
};
int main() {
  Fake ok; auto r=lmiproLocalDmeGet(ok,0x1560,10);
  assert(r.arg1==0x15600000 && r.done==1 && r.stage==3 && r.polls==2 && r.arg3==2);
  assert(ok.writes[2]==std::make_pair(0x94U,0x15600000U) && ok.ie==0x12345678);
  assert(r.beforeIs==0x900 && ok.writes[1].second==0x400);
  Fake fail; fail.result=1; r=lmiproLocalDmeGet(fail,0x1560,10);
  assert(r.done && (r.arg2&0xff)==1 && r.stage==3); // Completed does not mean success.
  Fake timeout; timeout.finish=0; timeout.data=0; r=lmiproLocalDmeGet(timeout,0x1560,3);
  assert(!r.done && r.stage==4 && r.polls==3 && timeout.delays==3 && timeout.ie==0x12345678);
  Fake notready; notready.hcs=0; r=lmiproLocalDmeGet(notready,0x1560,3);
  assert(r.stage==1 && !r.done && notready.writes.empty());
  Fake error; error.uicError=true; r=lmiproLocalDmeGet(error,0x1560,10);
  assert(!r.done && r.stage==5 && r.is==4 && error.ie==0x12345678);
  Fake zero; r=lmiproLocalDmeGet(zero,0x1560,0);
  assert(!r.done && zero.writes.empty());
  Fake existing; existing.is=0x10000; r=lmiproLocalDmeGet(existing,0x1560,3);
  assert(r.stage==2 && r.is==0x10000 && existing.writes.empty());
  puts("PASS: encoding, write allowlist, completion vs result, bounded timeout, readiness, error, IE restore");
}
