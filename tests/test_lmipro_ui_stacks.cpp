#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// The header references this ring; the kernel defines it in sleh.c.
volatile uint64_t lmipro_user_faults[8][5] = {};
volatile unsigned lmipro_user_fault_count = 0;
#include <initializer_list>
static const char *testName="backboardd";
static uint64_t testTid=100;
static unsigned ready=1,captures=0,replyCount=3;
static int replyError=0;
static bool recurse=false,loseStore=false;
extern "C" void proc_selfname(char *out,int size){snprintf(out,(size_t)size,"%s",testName);}
static void *current_thread(){return (void *)1;}
static uint64_t thread_tid(void *){return testTid;}
static uint64_t testSeconds=100;
extern "C" uint64_t lmipro_ui_stack_seconds(void){return testSeconds;}
extern "C" unsigned lmipro_ui_stack_ready(void){return ready;}
extern "C" unsigned lmipro_ui_stack_capture(uintptr_t *,unsigned,int *,unsigned *);
#include "../package/LmiproUITrace.h"
extern "C" unsigned lmipro_ui_stack_capture(uintptr_t *frames,unsigned count,int *error,unsigned *flags){
 assert(!gLmiproUIBusy && !gLmiproUIStacksBusy);assert(count==8);++captures;
 if(recurse){recurse=false;lmipro_ui_user_event(1,-47,0,0,0,58);}
 for(unsigned i=0;i<replyCount && i<count;++i)frames[i]=0x1000+i*4;
 *error=replyError;*flags=2;
 if(loseStore)gLmiproUIStacksBusy=1;
 return replyCount;
}
int main(){
 lmipro_ui_user_event(1,-47,0,0,0,58);assert(!captures);
 lmipro_ui_trace_enable(1);lmipro_ui_user_event(1,-47,0,0,0,58);assert(!captures);
 lmipro_ui_stacks_enable(2);assert(!gLmiproUIStacksEnabled);
 lmipro_ui_stacks_enable(1);ready=0;lmipro_ui_user_event(1,-47,0,0,0,58);assert(!captures && !gLmiproUIStacksStored);
 ready=1;testName="SpringBoard";lmipro_ui_user_event(1,-47,0,0,0,27);assert(!captures);
 testName="backboardd";lmipro_ui_user_event(2,-47,0,0,0,58);assert(!captures);
 // Sample before any display open; display activity cannot gate this evidence.
 assert(!gLmiproUIStacksDisplayPid);
 lmipro_ui_stack_display_open(0);lmipro_ui_stack_display_open(-1);
 // Startup calls must not reserve even one slot before useful wait entries.
 for(int code: {74,372,463,368})for(unsigned i=0;i<12;++i){++testTid;lmipro_ui_user_event(1,code,0,0,0,58);}
 assert(!captures && !gLmiproUIStacksStored);
 recurse=true;lmipro_ui_user_event(1,-47,0,0,0,58);
 assert(!gLmiproUIStacksDisplayPid);
 assert(captures==1 && gLmiproUIStacksStored==1 && gLmiproUIStacks[0].count==3 && gLmiproUIStacks[0].state==2);
 lmipro_ui_stack_display_open(59); // Different PID must not block subsequent samples.
 lmipro_ui_user_event(1,-47,0,0,0,58);assert(captures==1);
 ++testTid;replyError=14;lmipro_ui_user_event(1,375,0,0,0,58);
 assert(gLmiproUIStacks[1].error==14 && gLmiproUIStacks[1].count==3);
 ++testTid;replyCount=9;lmipro_ui_user_event(1,-47,0,0,0,58);
 assert(gLmiproUIStacks[2].state==3 && !gLmiproUIStacks[2].count);
 ++testTid;replyCount=3;loseStore=true;lmipro_ui_user_event(1,-47,0,0,0,58);
 assert(gLmiproUIStacks[3].state==1 && gLmiproUIStacksDropped==1);
 loseStore=false;gLmiproUIStacksBusy=0;
 lmipro_ui_user_event(1,-47,0,0,0,58);assert(captures==4);
 for(unsigned i=0;i<12;++i){++testTid;lmipro_ui_user_event(1,-47,0,0,0,58);}
 assert(gLmiproUIStacksStored==8 && captures==8);
 // Display-open observation remains reported, never a sampling prerequisite.
 ++testTid;lmipro_ui_user_event(1,-47,0,0,0,60);assert(captures==8);
 lmipro_ui_stack_display_open(60);assert(gLmiproUIStacksDisplayPid==60);
 char out[1536];lmipro_ui_stack_report(out,sizeof(out));
 assert(strstr(out,"stored8 cap8 depth8 focus0"));assert(strstr(out,"open pid60; focused calls all"));assert(strstr(out,"e14 i2"));
 assert(strstr(out,"1000 1004 1008"));assert(strstr(out,"s1 n0"));
 char tiny[4]={'X','X','X','X'};lmipro_ui_stack_report(tiny,sizeof(tiny));assert(!tiny[3]);
 gLmiproUIStacksBusy=1;lmipro_ui_stack_report(out,sizeof(out));assert(strstr(out,"snapshot busy"));
 lmipro_ui_stack_report(nullptr,0);
 // Refresh a filled early slot once, after this exact process opens display.
 gLmiproUIStacksBusy=0;loseStore=false;replyCount=3;replyError=0;
 testTid=gLmiproUIStacks[0].tid;testName="backboardd";
 unsigned before=captures,stored=gLmiproUIStacksStored;
 lmipro_ui_stack_display_open(58);
 lmipro_ui_user_event(1,-47,0,0,0,58);
 assert(captures==before+1 && gLmiproUIStacksStored==stored && gLmiproUIStacks[0].postOpen==1);
 lmipro_ui_user_event(1,-47,0,0,0,58);assert(captures==before+1);
 lmipro_ui_stack_report(out,sizeof(out));assert(strstr(out,"post1"));
 // A recorded return cannot be supplied by another thread or call.
 lmipro_ui_user_event(2,375,0,99,0,58);assert(!gLmiproUIStacks[0].returned);
 ++testTid;lmipro_ui_user_event(2,-47,0,99,0,58);assert(!gLmiproUIStacks[0].returned);
 --testTid;lmipro_ui_user_event(2,-47,0,7,0,58);assert(gLmiproUIStacks[0].returned && gLmiproUIStacks[0].result==7);
 lmipro_ui_user_event(2,-47,0,99,0,58);assert(gLmiproUIStacks[0].result==7);
 testSeconds=109;lmipro_ui_user_event(1,-47,0,0,0,58);assert(captures==before+1);
 testSeconds=110;lmipro_ui_user_event(1,-47,0,0,0,58);
 assert(captures==before+2 && !gLmiproUIStacks[0].returned && gLmiproUIStacks[0].seconds==110);
 assert(gLmiproUIStacks[0].samples==3 && gLmiproUIStacksStored==8);
 lmipro_ui_stack_report(out,sizeof(out));assert(strstr(out,"focus0 auto0 ev0 capSkip") && strstr(out,"now110"));
 // Focus must be explicit and paired with the process that opened display.
 lmipro_ui_stack_focus(testTid);lmipro_ui_stack_display_open(59);
 unsigned focusedBefore=captures;
 lmipro_ui_user_event(1,305,0,0,0,58);assert(captures==focusedBefore);
 lmipro_ui_stack_display_open(58);
 lmipro_ui_user_event(1,305,0,0,0,58);
 assert(captures==focusedBefore+1 && gLmiproUIStacks[0].code==305 && !gLmiproUIStacks[0].returned);
 lmipro_ui_user_event(2,305,0,4,0x20000000,58);
 assert(gLmiproUIStacks[0].returned && gLmiproUIStacks[0].result==4 && gLmiproUIStacks[0].status==0x20000000);
 // No ten-second blind interval: retain the immediately following pending call.
 lmipro_ui_user_event(1,515,0,0,0,58);
 assert(captures==focusedBefore+2 && gLmiproUIStacks[0].code==515 && !gLmiproUIStacks[0].returned);
 lmipro_ui_user_event(2,305,0,99,0,58);assert(!gLmiproUIStacks[0].returned);
 lmipro_ui_user_event(2,515,0,0,0,58);assert(gLmiproUIStacks[0].returned && !gLmiproUIStacks[0].status);
 unsigned irqBefore=captures;
 lmipro_ui_user_event(4,0,0x12340,0,0,58);
 lmipro_ui_user_event(4,0,0x12340,0,0,58);
 assert(gLmiproUIStacks[0].irqCount==2 && gLmiproUIStacks[0].irqSame==2 && captures==irqBefore);
 lmipro_ui_user_event(4,0,0x12344,0,0,58);
 assert(gLmiproUIStacks[0].irqCount==3 && gLmiproUIStacks[0].irqSame==1 && gLmiproUIStacks[0].irqPC==0x12344);
 lmipro_ui_user_event(1,515,0,0,0,58);assert(captures==focusedBefore+3 && !gLmiproUIStacks[0].returned);
 assert(!gLmiproUIStacks[0].irqCount && !gLmiproUIStacks[0].irqPC);
 lmipro_ui_stack_report(out,sizeof(out));assert(strstr(out,"focused calls all") && strstr(out,"c515"));
 lmipro_ui_stack_focus(0);
  // Automatic focus retains every display opener (up to four), because the first
  // opener can exit immediately: t336 sampled bsdthread_ctl, a thread lifecycle
  // call, with a three-frame stack.
  lmipro_ui_stack_focus(1);assert(gLmiproUIStacksAutoFocus && !gLmiproUIStacksFocusCount);
  testTid=8001;lmipro_ui_stack_display_open(58);
  assert(gLmiproUIStacksFocusCount==1 && gLmiproUIStacksFocusTids[0]==8001);
  testTid=8002;lmipro_ui_stack_display_open(58);
  assert(gLmiproUIStacksFocusCount==2 && gLmiproUIStacksFocusTids[1]==8002);
  // Both openers are focused; an unrelated thread stays ordinary and is not sampled
  // for a non -47/375 code before the display is open.
  testTid=8003;before=captures;lmipro_ui_user_event(1,305,0,0,0,58);
  assert(captures==before && gLmiproUIStacksFocusCount==2);
  // A focused opener still gets a slot when ordinary workers already filled the cap.
  testTid=8001;unsigned ev=gLmiproUIStacksFocusEvictions;
  before=captures;lmipro_ui_user_event(1,515,0,0,0,58);
  assert(captures==before+1 && gLmiproUIStacksStored==8 && gLmiproUIStacksFocusEvictions==ev+1);
  assert(gLmiproUIStacks[7].tid==8001 && gLmiproUIStacks[7].samples==1);
  lmipro_ui_user_event(2,515,0,7,0x20000000,58);
  assert(gLmiproUIStacks[7].returned && gLmiproUIStacks[7].result==7);
  lmipro_ui_user_event(4,0,0x88880,0,0,58);
  assert(gLmiproUIStacks[7].irqCount==1 && gLmiproUIStacks[7].irqPC==0x88880);
  // Invalid observations never bind a TID.
  lmipro_ui_stack_display_open(0);assert(gLmiproUIStacksFocusCount==2);
  // A different observed PID still records its own opener.
  testTid=8100;lmipro_ui_stack_display_open(60);
  assert(gLmiproUIStacksFocusCount==3 && gLmiproUIStacksFocusTids[2]==8100);
  // An explicit focus overrides the automatic mode and keeps exactly one TID.
  lmipro_ui_stack_focus(333);testTid=9000;lmipro_ui_stack_display_open(58);
  assert(!gLmiproUIStacksAutoFocus && gLmiproUIStacksFocusCount==1 && gLmiproUIStacksFocusTids[0]==333);
  // Returning to automatic mode clears the candidates again.
  lmipro_ui_stack_focus(1);testTid=8200;lmipro_ui_stack_display_open(58);
  assert(gLmiproUIStacksFocusCount==1 && gLmiproUIStacksFocusTids[0]==8200);
  // Never overwrite another thread's in-flight reservation or wait for its lock.
  for(auto &row:gLmiproUIStacks)row.state=1;
  uint64_t drops=gLmiproUIStacksDropped;before=captures;unsigned skipped=gLmiproUIStacksCapacitySkips;
  lmipro_ui_user_event(1,515,0,0,0,58);
  assert(captures==before && gLmiproUIStacksStored==8 && gLmiproUIStacksDropped==drops+1);
  assert(gLmiproUIStacksCapacitySkips==skipped+1);
  lmipro_ui_stack_report(out,sizeof(out));assert(strstr(out,"auto1 ev1"));

}
