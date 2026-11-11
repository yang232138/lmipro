#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const char *testName="backboardd";
static uint64_t testTid=100;
static unsigned ready=1,captures=0,replyCount=3;
static int replyError=0;
static bool recurse=false,loseStore=false;
extern "C" void proc_selfname(char *out,int size){snprintf(out,(size_t)size,"%s",testName);}
static void *current_thread(){return (void *)1;}
static uint64_t thread_tid(void *){return testTid;}
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
 recurse=true;lmipro_ui_user_event(1,-47,0,0,0,58);
 assert(captures==1 && gLmiproUIStacksStored==1 && gLmiproUIStacks[0].count==3 && gLmiproUIStacks[0].state==2);
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
 char out[1536];lmipro_ui_stack_report(out,sizeof(out));
 assert(strstr(out,"stored=8 cap8 depth8 delay30"));assert(strstr(out,"err14 info2"));
 assert(strstr(out,"1000 1004 1008"));assert(strstr(out,"state1 n0"));
 char tiny[4]={'X','X','X','X'};lmipro_ui_stack_report(tiny,sizeof(tiny));assert(!tiny[3]);
 gLmiproUIStacksBusy=1;lmipro_ui_stack_report(out,sizeof(out));assert(strstr(out,"snapshot busy"));
 lmipro_ui_stack_report(nullptr,0);
 puts("PASS: opt-in/faultable gate; backboardd entry only; no observation lock during capture; per-thread cap/recursion; exact errors and incomplete samples");
}
