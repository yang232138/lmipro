#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const char *testName="SpringBoard";
static unsigned nameCopies=0;
extern "C" void proc_selfname(char *out,int size){++nameCopies;snprintf(out,(size_t)size,"%s",testName);}
static void *current_thread(){return (void *)1;}
static uint64_t thread_tid(void *){return 77;}
#include "../package/LmiproUITrace.h"
int main(){
    lmipro_ui_user_event(1,31,0x100,2,3,26);assert(!nameCopies&&!gLmiproUIActors[0].entries);
    lmipro_ui_trace_enable(2);assert(!gLmiproUIEnabled);
    lmipro_ui_trace_enable(1);
    testName="SpringBoardOther";lmipro_ui_user_event(1,31,0,0,0,26);assert(!gLmiproUIActors[0].pid);
    testName="SpringBoard";lmipro_ui_user_event(1,-47,0x100,2,3,26);
    assert(gLmiproUIActors[0].entries==1&&gLmiproUIActors[0].entry.tid==77);
    // Other threads may return without a paired entry in a last-event snapshot.
    lmiproRecordUI(0,26,2,31,0x104,4,5,88);
    assert(gLmiproUIActors[0].returned.tid==88&&gLmiproUIActors[0].entry.tid==77);
    testName="backboardd";lmipro_ui_user_event(3,0x24,0x200,0x92000007,0x300,58);
    assert(gLmiproUIActors[1].exceptions==1&&gLmiproUIActors[1].exception.b==0x300);
    lmiproRecordUI(0,26,4,0,1,2,3,77);assert(gLmiproUIActors[0].interrupts==1);
    lmiproRecordUI(0,126,1,1,0x400,2,3,99);
    assert(gLmiproUIActors[0].pid==126&&gLmiproUIActors[0].generations==2);
    assert(gLmiproUIActors[0].entries==1&&!gLmiproUIActors[0].returns&&!gLmiproUIActors[0].returned.pc);
    lmiproRecordUI(2,99,1,1,2,3,4,5);lmiproRecordUI(0,0,1,1,2,3,4,5);
    gLmiproUIBusy=1;lmiproRecordUI(0,126,1,1,2,3,4,5);assert(gLmiproUIDropped==1);
    char out[960];lmipro_ui_trace_report(out,sizeof(out));assert(strstr(out,"snapshot busy"));
    gLmiproUIBusy=0;lmipro_ui_trace_report(out,sizeof(out));
    assert(strstr(out,"SpringBoard pid=126 gen=2 in=1 out=0"));
    assert(strstr(out,"backboardd pid=58 gen=1 in=0 out=0 exc=1"));
    assert(strstr(out,"esr=92000007 far=300 tid=77"));
    char tiny[4];memset(tiny,'X',sizeof(tiny));lmipro_ui_trace_report(tiny,sizeof(tiny));assert(tiny[3]==0);
    lmipro_ui_trace_report(nullptr,0);
    puts("PASS: opt-in exact-name actors, real scalar/thread snapshots, restart separation, bounds, no-wait contention and truncation");
}
