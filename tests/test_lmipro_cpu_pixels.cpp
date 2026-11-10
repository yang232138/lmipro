#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "NativeLegacyRequest.h"
int main(){
  alignas(8) uint32_t input[10]={1,2,3,91,92,4,5,6,93,94};
  alignas(8) uint32_t output[12];for(auto &p:output)p=0xdeadbeef;
  // A four-byte offset forces the scalar fallback. Padding and both outside
  // guards must remain untouched when differently padded rows are copied.
  assert(native_legacy_copy_pixels(output+1,32,16,input,40,20,3,2));
  assert(output[0]==0xdeadbeef&&output[1]==1&&output[2]==2&&output[3]==3);
  assert(output[4]==0xdeadbeef&&output[5]==4&&output[6]==5&&output[7]==6);
  assert(output[8]==0xdeadbeef&&output[9]==0xdeadbeef);
  uint32_t before[12];memcpy(before,output,sizeof(output));
  assert(!native_legacy_copy_pixels(output+1,31,16,input,40,20,3,2));
  assert(!native_legacy_copy_pixels(output+1,32,16,input,39,20,3,2));
  assert(!native_legacy_copy_pixels((char *)output+1,32,16,input,40,20,3,2));
  assert(!native_legacy_copy_pixels(output+1,32,UINT64_MAX-3,input,40,20,3,2));
  assert(!memcmp(before,output,sizeof(output)));
  alignas(8) uint32_t wideSource[20],wideDest[24];
  for(unsigned i=0;i<20;++i)wideSource[i]=0xff000000U+i;
  for(auto &p:wideDest)p=0x12345678;
  assert(native_legacy_copy_pixels(wideDest,96,48,wideSource,80,40,9,2));
  for(unsigned row=0;row<2;++row){
    for(unsigned col=0;col<9;++col)assert(wideDest[row*12+col]==wideSource[row*10+col]);
    for(unsigned col=9;col<12;++col)assert(wideDest[row*12+col]==0x12345678);
  }
  puts("PASS: canonical pixel copy, padded rows, narrow/wide stores, untouched guards and no writes after invalid bounds");
}
