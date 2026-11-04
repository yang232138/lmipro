#define LMIPRO_UIC_HOST_TEST
#include "../package/LmiproUfsHandoff.cpp"
#include <map>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstdio>
static void le(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(i*8));}
static void be(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>((3-i)*8));}
struct Fake {
  uint64_t dma=0xa0000000;unsigned bytes=16384,bs,submits=0;
  alignas(64) uint8_t data[16384]={};
  uint8_t header[4096]={},entries[16384]={};
  std::map<unsigned,uint32_t> h;
  bool timeout=false,sense=false,badTag=false,shortRead=false,corruptArray=false;
  Fake(unsigned sector=4096):bs(sector) {
    h[0x30]=15;h[0x34]=1;
    entries[0]=1;le(entries+32,100);le(entries+40,500);
    const char *name="userdata";for(unsigned i=0;i<8;++i)entries[56+2*i]=name[i];
    memcpy(header,"EFI PART",8);le(header+8,0x10000);le(header+12,92);le(header+24,1);le(header+32,1023);
    le(header+40,34);le(header+48,900);le(header+72,2);le(header+80,128);le(header+84,128);
    le(header+88,lmiCrc(0xffffffff,entries,16384)^0xffffffff);seal();
  }
  void seal(){le(header+16,0);le(header+16,lmiCrc(0xffffffff,header,92)^0xffffffff);}
  uint32_t hostRead(unsigned o){return h[o];}
  void hostWrite(unsigned o,uint32_t v){
    if(o==0x20){h[o]&=~v;return;}h[o]=v;if(o!=0x58)return;
    ++submits;assert(submits<=35);if(timeout)return;
    auto *w=reinterpret_cast<uint32_t*>(data);assert(w[0]==0x14000000 && (w[7]&65535)==1);
    auto *q=data+4096,*rsp=data+4608,*out=data+8192;
    unsigned n=lmiBe(q+12);assert(n<=4096 && q[1]==0x40 && !q[2]);
    auto *prd=reinterpret_cast<uint32_t*>(data+5120);assert(prd[0]==dma+8192 && prd[3]==n-1);
    if(q[16]==0x25){assert(n==8);be(out,1023);be(out+4,bs);}
    else {assert(q[16]==0x28 && q[24]==1 && n==bs);unsigned lba=lmiBe(q+18);
      if(lba==1)memcpy(out,header,n);
      else if(lba>=2 && lba<2+16384/bs){memcpy(out,entries+(lba-2)*bs,n);if(corruptArray)out[0]^=1;}
      else {assert(lba==100);memcpy(out,"SHNGROOT",8);}
    }
    w[2]=0;h[0x58]=0;rsp[0]=0x21;rsp[3]=badTag?3:2;
    if(shortRead)be(rsp+12,4);
    if(sense){rsp[7]=2;rsp[11]=16;rsp[33]=14;rsp[34]=0x70;rsp[36]=6;rsp[46]=0x29;}
  }
  void clean(){} void invalidate(){} void delay(){}
};
int main(){
  for(unsigned bs:{512U,4096U}){Fake f(bs);auto r=lmiproReadDisk(f);assert(!r.error && r.stage==14 && r.first==100 && r.last==500 && r.blockBytes==bs);assert(r.commands==3+16384/bs && r.magic0==0x474e4853);}
  Fake crc;crc.header[16]^=1;auto r=lmiproReadDisk(crc);assert(r.error && r.stage==11 && crc.submits==2);
  Fake array;array.corruptArray=true;r=lmiproReadDisk(array);assert(r.error && r.stage==12 && array.submits==6);
  Fake bounds;le(bounds.header+72,1000);bounds.seal();r=lmiproReadDisk(bounds);assert(r.error && bounds.submits==2);
  Fake cap(1024);r=lmiproReadDisk(cap);assert(r.error && cap.submits==1);
  Fake sense;sense.sense=true;r=lmiproReadDisk(sense);assert(r.error && r.scsi==2 && r.sense==0x062900 && sense.submits==1);
  Fake tag;tag.badTag=true;r=lmiproReadDisk(tag);assert(r.error && tag.submits==1);
  Fake shortRead;shortRead.shortRead=true;r=lmiproReadDisk(shortRead);assert(r.error && shortRead.submits==1);
  Fake timeout;timeout.timeout=true;r=lmiproReadDisk(timeout);assert(r.error && timeout.submits==1 && r.polls==500000);
  Fake busy;busy.h[0x58]=1;r=lmiproReadDisk(busy);assert(r.error && !busy.submits);
  puts("READ: 512/4096 capacity, full GPT CRC, userdata, sense, timeout and bounds passed");
}
