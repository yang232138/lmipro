#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../package/LmiproScanoutPolicy.h"
int main(){
  assert(lmiproCPUFormat(0x42475241,0));
  assert(!lmiproCPUFormat(0x26424741,1)&&!lmiproCPUFormat(0x42475241,1));
  assert(!lmiproCPUFormat(0x52474241,0));
  assert(lmiproKeepPresentedFrame(1,1,1,10368000,0,1,1));
  assert(!lmiproKeepPresentedFrame(0,1,1,10368000,0,1,1));
  assert(!lmiproKeepPresentedFrame(1,0,1,10368000,0,1,1));
  assert(!lmiproKeepPresentedFrame(1,1,0,10368000,0,1,1));
  assert(!lmiproKeepPresentedFrame(1,1,1,0,0,1,1));
  assert(!lmiproKeepPresentedFrame(1,1,1,10368000,1,1,1));
  assert(!lmiproKeepPresentedFrame(1,1,1,10368000,0,0,1));
  assert(!lmiproKeepPresentedFrame(1,1,1,10368000,0,1,0));
  puts("PASS: CPU format rejects unqualified compressed/planar surfaces; diagnostics remain for no copy, errors, no retirement and blank-only swaps");
}
