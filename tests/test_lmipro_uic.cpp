#define LMIPRO_UIC_HOST_TEST
#include "../package/LmiproUfsHandoff.cpp"
#include <map>
#include <vector>
#include <cassert>
#include <cstdio>
struct Fake {
  std::map<unsigned,uint32_t> regs;
  std::vector<unsigned> reads;
  unsigned changes=0;
  bool mutate=false, global=false;
  uint32_t read32(unsigned off) {
    assert(!(off&3));
    // RO2 only uses GR0 globals and the architected first 128 routing entries.
    assert(off<=0x58 || (off>=0x800 && off<=0x9fc) || (off>=0xc00 && off<=0xdfc));
    reads.push_back(off);
    if (mutate && off==0x800 && ++changes==2) return regs[off]^1;
    if (global && off==0 && ++changes==2) return regs[off]^1;
    return regs[off];
  }
  Fake() { regs[0x20]=(1U<<27)|4; regs[0x24]=(6U<<28)|74; }
};
int main() {
  Fake empty; auto r=lmiproSmmuAudit(empty);
  assert(r.stage==3 && r.scanned==4 && !r.valid && !r.nonzero && !r.sampleCount);
  assert(r.hash==r.hash2 && !r.changed && !r.globalChanged && !r.stream[0].matches);
  Fake unrelated; unrelated.regs[0x800]=0x80000021; unrelated.regs[0xc00]=3;
  r=lmiproSmmuAudit(unrelated);
  assert(r.valid==1 && r.nonzero==1 && r.types[0]==1 && !r.stream[0].matches);
  assert(r.sampleCount==1 && r.sampleIndex[0]==0 && r.sampleSmr[0]==0x80000021);
  Fake mix;
  mix.regs[0x800]=0x800000e0; mix.regs[0xc00]=2;
  mix.regs[0x804]=0x800004e0; mix.regs[0xc04]=0x10000;
  mix.regs[0x808]=0x840000e0; mix.regs[0xc08]=0x20000;
  mix.regs[0x80c]=0x80000020; mix.regs[0xc0c]=0x30000;
  r=lmiproSmmuAudit(mix);
  assert(r.valid==4 && r.nonzero==4 && r.sampleCount==4);
  for(unsigned t:r.types) assert(t==1);
  assert(r.stream[0].matches==2 && r.stream[1].matches==2);
  assert(!r.stream[0].contextRead && !r.stream[1].contextRead);
  Fake invalid; invalid.regs[0x800]=0xe0; invalid.regs[0xc00]=0x20000;
  r=lmiproSmmuAudit(invalid); assert(!r.valid && r.nonzero==1 && !r.stream[0].matches);
  Fake exid; exid.regs[0]=8; exid.regs[0x20]|=1U<<8;
  exid.regs[0x800]=0xe0; exid.regs[0xc00]=0x10400;
  r=lmiproSmmuAudit(exid); assert(r.extended && r.valid==1 && r.types[1]==1 && r.stream[0].matches==1);
  Fake unsupported=exid; unsupported.reads.clear(); unsupported.regs[0x20]&=~(1U<<8);
  r=lmiproSmmuAudit(unsupported); assert(r.stage==2 && !r.scanned && unsupported.reads.size()==8);
  Fake allones; allones.regs[0x800]=0xffffffffU; allones.regs[0xc00]=0xffffffffU;
  r=lmiproSmmuAudit(allones); assert(r.allones==1);
  Fake large; large.regs[0x20]=(1U<<27)|255;
  r=lmiproSmmuAudit(large); assert(r.groups==255 && r.scanned==128 && large.reads.size()==8+512+5);
  Fake changing=mix; changing.mutate=true;
  r=lmiproSmmuAudit(changing); assert(r.changed==1 && r.hash!=r.hash2);
  Fake globals=mix; globals.global=true;
  r=lmiproSmmuAudit(globals); assert(r.globalChanged==1 && !r.changed);
  Fake noSms; noSms.regs[0x20]=1;
  r=lmiproSmmuAudit(noSms); assert(r.stage==2 && noSms.reads.size()==8);
  Fake absent; absent.regs[0x20]=0;
  r=lmiproSmmuAudit(absent); assert(r.stage==1 && !r.scanned);
  puts("PASS: empty/nonmatching/mixed/invalid/EXID/all-ones tables, exact snapshot changes, global changes and 128-entry read-only bound");
}
