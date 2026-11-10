#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
extern "C" {
unsigned long cs_native4k_full_blocks=0,cs_native4k_ready_blobs=0;
unsigned long cs_validate_page_bad_hash=0,cs_validate_page_no_hash=0;
uint64_t cs_native4k_bytes=0,cs_native4k_read_ticks=0,cs_native4k_hash_ticks=0;
}
static unsigned gLmiproDataWatchSeconds=600;
static bool advance=true;
static std::vector<unsigned> sleeps;
static void IOSleep(unsigned ms){sleeps.push_back(ms);if(advance){cs_native4k_full_blocks+=64;cs_native4k_bytes+=1048576;cs_native4k_ready_blobs++;cs_native4k_read_ticks+=100;cs_native4k_hash_ticks+=50;}}
#include "../package/LmiproSignProgress.h"
int main(){
  char out[448];lmiproWaitSignProgress(out,sizeof out);
  assert((sleeps==std::vector<unsigned>{30000,570000}));
  assert(strstr(out,"CS start blocks=0 bytes=0 blobs=0"));
  assert(strstr(out,"CS at30 blocks=64 bytes=1048576 blobs=1"));
  assert(strstr(out,"CS at600 blocks=128 bytes=2097152 blobs=2"));
  assert(strstr(out,"readTicks=200 hashTicks=100 bad=0 noHash=0"));
  advance=false;sleeps.clear();gLmiproDataWatchSeconds=5;
  cs_validate_page_bad_hash=7;cs_validate_page_no_hash=8;
  LmiproSignSample before=lmiproReadSignSample();
  lmiproWaitSignProgress(out,sizeof out);
  assert((sleeps==std::vector<unsigned>{5000}));
  assert(strstr(out,"CS at5")&&strstr(out,"bad=7 noHash=8"));
  LmiproSignSample after=lmiproReadSignSample();assert(!memcmp(&before,&after,sizeof before));
  char tiny[8];memset(tiny,'X',sizeof tiny);lmiproWaitSignProgress(tiny,sizeof tiny);assert(tiny[7]==0);
  puts("PASS: bounded two-point wait; real counter progress/unchanged cases; error counters preserved; safe formatting");
}
