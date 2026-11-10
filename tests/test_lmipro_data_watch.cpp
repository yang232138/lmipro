#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdexcept>
#include <initializer_list>
using wait_result_t=int;using thread_t=void *;using kern_return_t=int;
static const int KERN_SUCCESS=0;
extern "C" {
volatile int lmipro_data_stage=-2,lmipro_data_status=37,lmipro_data_readonly=-1;
volatile int lmipro_init_stage=22,lmipro_init_error=0;
char lmipro_init_path[128]="/sbin/launchd";
unsigned long cs_native4k_full_blocks=0,cs_native4k_ready_blobs=0;
unsigned long cs_validate_page_bad_hash=0,cs_validate_page_no_hash=0;
uint64_t cs_native4k_bytes=0,cs_native4k_read_ticks=0,cs_native4k_hash_ticks=0;
volatile uint32_t native_ram_block_ready=1;
volatile uint64_t native_ram_block_bytes=33554432,native_ram_block_reads=20,
    native_ram_block_writes=3,native_ram_block_failures=0;
}
static volatile int gUfsPublishState=1;
static volatile unsigned gPublishedWindows=2;
static unsigned arg=0,slept=0,created=0,released=0;
static bool supplied=false,failThread=false;
static unsigned processQueries=0;
static int tracePid=1;
static void (*entry)(void *,wait_result_t);
static char message[1024];
static int PE_parse_boot_argn(const char *name,void *out,unsigned n){assert(!strcmp(name,"lmipro-data-watch")&&n==sizeof(arg));if(!supplied)return 0;memcpy(out,&arg,n);return 1;}
static void IOSleep(unsigned ms){slept=ms;}
[[noreturn]] static void panic(const char *fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(message,sizeof message,fmt,ap);va_end(ap);throw std::runtime_error(message);}
static int kernel_thread_start(void (*f)(void *,wait_result_t),void *,thread_t *out){if(failThread)return 5;entry=f;*out=(void *)1;++created;return 0;}
static void thread_deallocate(thread_t t){assert(t==(void *)1);++released;}
#include "../package/LmiproDataWatch.h"
extern "C" int proc_selfpid(){return tracePid;}
extern "C" void proc_name(int pid,char *buffer,int size){assert(pid>=1&&pid<=64&&size==17);++processQueries;const char *name=pid==1?"launchd":pid==3?"mount":pid==17?"tzinit":"";snprintf(buffer,(size_t)size,"%s",name);}
int main(){
  lmiproStartDataWatch();assert(!created);supplied=true;lmiproStartDataWatch();assert(!created);
  for(unsigned invalid:{4U,601U}){arg=invalid;bool caught=false;try{lmiproStartDataWatch();}catch(const std::runtime_error&){caught=true;}assert(caught&&!created);}
  arg=30;failThread=true;bool caught=false;try{lmiproStartDataWatch();}catch(const std::runtime_error&){caught=true;}assert(caught&&!created);
  failThread=false;lmiproStartDataWatch();assert(created==1&&released==1&&entry);
  gLmiproDataRootPhase=2;
  for(int stage:{-2,10,11,0,1,2,3,4}){
    lmipro_data_stage=stage;bool checkpoint=false;
    try{entry(nullptr,0);}catch(const std::runtime_error&){checkpoint=true;}
    assert(slept==30000&&lmipro_data_status==37&&lmipro_data_stage==stage);
    assert(checkpoint);char expected[32];snprintf(expected,sizeof expected,"dataStage=%d",stage);assert(strstr(message,expected));assert(strstr(message,"rootphase=2 pubState=1 completeWindows=2"));assert(strstr(message,"ramReady=1 bytes=33554432"));
  }
  lmipro_data_stage=4;lmipro_data_status=0;lmipro_data_readonly=0;caught=false;
  try{entry(nullptr,0);}catch(const std::runtime_error&){caught=true;}
  assert(caught&&strstr(message,"dataStage=4 status=0 readonly=0"));
  assert(lmipro_data_stage==4&&lmipro_data_status==0&&lmipro_data_readonly==0);
  assert(strstr(message,"initStage=22 execError=0 path=/sbin/launchd"));
  assert(strstr(message,"1:launchd 3:mount 17:tzinit"));
  assert(lmipro_init_stage==22&&lmipro_init_error==0);
  char tiny[8];memset(tiny,'X',sizeof tiny);lmiproSampleProcesses(tiny,sizeof tiny);
  assert(tiny[7]==0);unsigned queries=processQueries;
  char one='X';lmiproSampleProcesses(&one,1);assert(one==0&&queries==processQueries);
  lmiproSampleProcesses(nullptr,0);assert(queries==processQueries);
  puts("PASS: default off; bounds/thread errors; all Data/init states preserved; bounded read-only process names and output truncation");
}
