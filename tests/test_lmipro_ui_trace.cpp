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
    uint64_t regs[29]={};regs[24]=1;regs[25]=1080;regs[26]=2400;
    regs[22]=2;regs[23]=1;regs[28]=1;
    testName="backboardd";lmipro_ui_fault_event(0x1234,regs);assert(!gLmiproUIFaultValid);
    testName="SpringBoard";lmipro_ui_fault_event(0x1234,regs);
    assert(gLmiproUIFaultValid&&gLmiproUIFaultPC==0x1234&&gLmiproUIFaultWords[5]==1);
    regs[24]=999;lmipro_ui_fault_event(0x5678,regs);
    assert(gLmiproUIFaultPC==0x1234&&gLmiproUIFaultWords[5]==1);
    char fault[1600];lmipro_ui_trace_report(fault,sizeof(fault));
    assert(strstr(fault,"UI first BRK valid=1 pc=1234 x24=1 x25=438 x26=960"));
    lmiproRecordUI(0,126,2,136,0x100,13,0,77);assert(!gLmiproUIMkdirErrors[0].valid);
    lmiproRecordUI(0,126,2,475,0x104,13,UINT64_C(1)<<29,77);
    assert(gLmiproUIMkdirErrors[0].valid && gLmiproUIMkdirErrors[0].error==13);
    lmiproRecordUI(0,226,1,136,0,0,0,99);
    lmiproRecordUI(0,226,2,136,0x108,28,UINT64_C(1)<<29,99);
    assert(gLmiproUIMkdirErrors[0].pid==126 && gLmiproUIMkdirErrors[0].code==475);
    lmipro_ui_trace_report(fault,sizeof(fault));assert(strstr(fault,"mkdir first SB=1 pid=126 c=475 errno=13 pc=104"));
    puts("PASS: opt-in exact-name actors, real scalar/thread snapshots, restart separation, bounds, no-wait contention and truncation");
}
