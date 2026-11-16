#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../package/LmiproScanoutPolicy.h"

// The kernel defines this ring in sleh.c; host tests need their own copy.
extern "C" volatile uint64_t lmipro_user_faults[8][7] = {};
extern "C" volatile unsigned lmipro_user_fault_count = 0;
extern "C" char lmipro_user_fault_name[8][17] = {};
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
