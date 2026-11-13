#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static int currentPid=1;
extern "C" int proc_selfpid(){return currentPid;}
extern "C" void proc_selfname(char *out,int size){snprintf(out,(size_t)size,"unrelated");}
static void *current_thread(){return (void *)1;}
static uint64_t thread_tid(void *){return 42;}
extern "C" unsigned lmipro_ui_stack_ready(void){return 0;}
extern "C" uint64_t lmipro_ui_stack_seconds(void){return 0;}
extern "C" unsigned lmipro_ui_stack_capture(uintptr_t *,unsigned,int *,unsigned *){assert(false);return 0;}
#include "../package/LmiproUserTrace.h"
int main(){
  lmipro_user_trace_event(1,5,0x1234,7,8);assert(!gLmiproTrace.entries);
  gLmiproTraceEnabled=1;currentPid=2;
  lmipro_user_trace_event(1,5,0x1234,7,8);assert(!gLmiproTrace.entries);
  currentPid=1;lmipro_user_trace_event(1,5,0x1234,7,8);
  assert(gLmiproTrace.entries==1&&gLmiproTrace.entry.pc==0x1234&&gLmiproTrace.entry.a==7);
  lmipro_user_trace_event(2,-31,0x1238,9,UINT64_C(1)<<29);
  assert(gLmiproTrace.returns==1&&!gLmiproTrace.first_error_valid);
  lmipro_user_trace_event(2,5,0x1238,2,UINT64_C(1)<<29);
  lmipro_user_trace_event(2,6,0x1240,13,UINT64_C(1)<<29);
  assert(gLmiproTrace.first_error_valid&&gLmiproTrace.first_error.a==2&&gLmiproTrace.first_error.code==5);
  assert(gLmiproTrace.returned.a==13&&gLmiproTrace.returned.code==6);
  lmipro_user_trace_event(3,0x20,0x4567,0x80000000,0x4000);
  assert(gLmiproTrace.exceptions==1&&gLmiproTrace.exception.b==0x4000);
  lmipro_user_trace_event(4,0,0x8000,0x9000,0);
  assert(gLmiproTrace.interrupts==1&&gLmiproTrace.interrupt.pc==0x8000);
  lmipro_user_trace_event(5,5,0x90000000,8192,0);
  assert(gLmiproTrace.signature.code==5&&gLmiproTrace.signature.a==8192);
  gLmiproTraceBusy=1;lmipro_user_trace_event(1,8,0,0,0);
  assert(gLmiproTraceDropped==1&&gLmiproTrace.entries==1);
  char out[640];lmiproFormatUserTrace(out,sizeof out);assert(strstr(out,"snapshot unavailable"));
  gLmiproTraceBusy=0;lmiproFormatUserTrace(out,sizeof out);
  assert(strstr(out,"svcIn=1 svcOut=3 userExc=1 dropped=1"));
  assert(strstr(out,"code=5 errno=2 pc=1238"));
  assert(strstr(out,"EL0 irq=1 pc=8000 sp=9000 cpsr=0"));
  assert(strstr(out,"fileSig stage=5 detail=90000000 sigBytes=8192 base=0"));
  char tiny[8];memset(tiny,'X',sizeof tiny);lmiproFormatUserTrace(tiny,sizeof tiny);assert(tiny[7]==0);
  assert(gLmiproTrace.entry.code==5&&gLmiproTrace.returned.a==13&&!gLmiproTraceBusy);
  puts("PASS: opt-in PID filter; exact event values; first BSD error retained; nonwaiting drops; bounded snapshot");
}
