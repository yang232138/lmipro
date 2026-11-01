#define LMIPRO_UIC_HOST_TEST
#include "../package/LmiproUfsHandoff.cpp"
#include <map>
#include <vector>
#include <cassert>
#include <cstdio>
struct Fake {
  std::map<unsigned,uint32_t> regs;
  std::vector<unsigned> reads;
  uint32_t read32(unsigned off) { assert(off<=0xffffc && !(off&3)); reads.push_back(off); return regs[off]; }
  Fake() { regs[0x20]=(1U<<27)|2; regs[0x24]=(6U<<28)|4; }
};
int main() {
  Fake normal;
  normal.regs[0x800]=0x800000e0; normal.regs[0xc00]=2;
  normal.regs[0x804]=0x800004e0; normal.regs[0xc04]=0x10000;
  normal.regs[0x1008]=0x10000; normal.regs[0x82000]=1;
  normal.regs[0x82058]=0x12; normal.regs[0x82060]=0x12345678;
  auto r=lmiproSmmuAudit(normal);
  assert(r.stage==3 && r.cbBase==0x80000 && r.pgshift==12);
  assert(r.stream[0].matches==1 && r.stream[0].contextRead && r.stream[0].fsr==0x12);
  assert(r.stream[1].matches==1 && !r.stream[1].contextRead); // BYPASS is not TRANS.
  Fake broad; broad.regs[0x800]=0x840000e0; broad.regs[0xc00]=0x20000;
  r=lmiproSmmuAudit(broad);
  assert(r.stream[0].matches==1 && r.stream[1].matches==1 && !r.stream[0].contextRead);
  Fake ambiguous=normal; ambiguous.regs[0x804]=0x800000e0;
  r=lmiproSmmuAudit(ambiguous); assert(r.stream[0].matches==2 && !r.stream[0].contextRead);
  Fake invalid=normal; invalid.regs[0xc00]=4;
  r=lmiproSmmuAudit(invalid); assert(!r.stream[0].contextRead);
  Fake mismatch=normal; mismatch.regs[0x24]=4;
  r=lmiproSmmuAudit(mismatch); assert(r.stage==4 && !r.stream[0].contextRead);
  Fake large=normal; large.regs[0x24]=0xa0000004;
  large.regs[0x20000]=1; r=lmiproSmmuAudit(large);
  assert(r.pgshift==16 && r.cbBase==0x80000 && r.stream[0].contextRead);
  Fake exid; exid.regs[0]=8; exid.regs[0x20]|=1U<<8;
  exid.regs[0x800]=0xe0; exid.regs[0xc00]=0x10400;
  r=lmiproSmmuAudit(exid); assert(r.extended && r.stream[0].matches==1 && !r.stream[0].contextRead);
  Fake missing; r=lmiproSmmuAudit(missing); assert(!r.stream[0].matches);
  Fake noSms; noSms.regs[0x20]=1; r=lmiproSmmuAudit(noSms); assert(r.stage==2 && noSms.reads.size()==8);
  Fake all; all.regs[0x20]=(1U<<27)|255; r=lmiproSmmuAudit(all); assert(r.groups==255);
  puts("PASS: stream masks, ambiguity, TRANS/BYPASS/FAULT, extended IDs, 4K/64K geometry and bounded reads; no write API");
}
