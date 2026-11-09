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
volatile uint32_t native_ram_block_ready=1;
volatile uint64_t native_ram_block_bytes=33554432,native_ram_block_reads=20,
    native_ram_block_writes=3,native_ram_block_failures=0;
}
static volatile int gUfsPublishState=1;
static volatile unsigned gPublishedWindows=2;
static unsigned arg=0,slept=0,created=0,released=0;
static bool supplied=false,failThread=false;
static void (*entry)(void *,wait_result_t);
static char message[1024];
struct Stopped {};
static int PE_parse_boot_argn(const char *name,void *out,unsigned n){assert(!strcmp(name,"lmipro-data-watch")&&n==sizeof(arg));if(!supplied)return 0;memcpy(out,&arg,n);return 1;}
static void IOSleep(unsigned ms){slept=ms;}
[[noreturn]] static void panic(const char *fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(message,sizeof message,fmt,ap);va_end(ap);throw std::runtime_error(message);}
static int kernel_thread_start(void (*f)(void *,wait_result_t),void *,thread_t *out){if(failThread)return 5;entry=f;*out=(void *)1;++created;return 0;}
static void thread_deallocate(thread_t t){assert(t==(void *)1);++released;}
[[noreturn]] static void thread_terminate_self(){throw Stopped{};}
#include "../package/LmiproDataWatch.h"
int main(){
  lmiproStartDataWatch();assert(!created);supplied=true;lmiproStartDataWatch();assert(!created);
  for(unsigned invalid:{4U,121U}){arg=invalid;bool caught=false;try{lmiproStartDataWatch();}catch(const std::runtime_error&){caught=true;}assert(caught&&!created);}
  arg=30;failThread=true;bool caught=false;try{lmiproStartDataWatch();}catch(const std::runtime_error&){caught=true;}assert(caught&&!created);
  failThread=false;lmiproStartDataWatch();assert(created==1&&released==1&&entry);
  gLmiproDataRootPhase=2;
  for(int stage:{-2,10,11,0,1,2,3,4}){
    lmipro_data_stage=stage;bool stopped=false,checkpoint=false;
    try{entry(nullptr,0);}catch(const Stopped&){stopped=true;}catch(const std::runtime_error&){checkpoint=true;}
    assert(slept==30000&&lmipro_data_status==37&&lmipro_data_stage==stage);
    if(stage==4)assert(stopped&&!checkpoint);else{assert(checkpoint&&!stopped);char expected[32];snprintf(expected,sizeof expected,"dataStage=%d",stage);assert(strstr(message,expected));assert(strstr(message,"rootphase=2 pubState=1 completeWindows=2"));assert(strstr(message,"ramReady=1 bytes=33554432"));}
  }
  puts("PASS: default off; bounds/thread errors; real stages and errno preserved; Data success terminates without panic");
}
