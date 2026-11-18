#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const char *testName="SpringBoard";
static unsigned nameCopies=0;
static int testPid=58;
extern "C" int proc_selfpid(void){return testPid;}
static unsigned testMain=0,stackReady=0;
static uint64_t stackTime=45;
extern "C" unsigned lmipro_ui_is_main_thread(void){return testMain;}
extern "C" void proc_selfname(char *out,int size){++nameCopies;snprintf(out,(size_t)size,"%s",testName);}
static void *current_thread(){return (void *)1;}
static uint64_t thread_tid(void *){return 77;}
extern "C" unsigned lmipro_ui_stack_ready(void){return stackReady;}
extern "C" uint64_t lmipro_ui_stack_seconds(void){return stackTime;}
extern "C" unsigned lmipro_ui_stack_capture(uintptr_t *,unsigned,int *,unsigned *);
#include "../package/LmiproUITrace.h"
extern "C" unsigned lmipro_ui_stack_capture(uintptr_t *pc,unsigned cap,int *error,unsigned *flags){
    assert(stackReady && cap==8 && !gLmiproSpringMainBusy);
    pc[0]=0x12340;pc[1]=0x31220;*error=0;*flags=1;
    return 2;
}

// The kernel defines this ring in sleh.c; host tests need their own copy.
extern "C" volatile uint64_t lmipro_user_faults[8][7] = {};
extern "C" volatile unsigned lmipro_user_fault_count = 0;
extern "C" char lmipro_user_fault_name[8][17] = {};
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
    char wdt[128]="original";testName="watchdogd";
    uint64_t wdtRegs[31]={};wdtRegs[30]=0x112233;
    lmipro_ui_fault_event(0x10000ab54,wdtRegs);
    lmipro_watchdog_exit_report(113,"watchdogd",wdt,sizeof(wdt));
    assert(strstr(wdt,"pc=10000ab54 lr=112233"));
    strcpy(wdt,"original");lmipro_watchdog_exit_report(27,"SpringBoard",wdt,sizeof(wdt));
    assert(!strcmp(wdt,"original"));
    lmipro_render_ipc_receive(40000,27,0); // SpringBoard isn't a receiver here.
    assert(!gLmiproRenderDelivered);
    testName="backboardd";
    lmipro_render_ipc_receive(39999,27,0);
    lmipro_render_ipc_receive(40200,27,0);
    assert(!gLmiproRenderDelivered);
    lmipro_render_ipc_receive(40000,27,0);
    lmipro_render_ipc_receive(40000,27,0x10004004);
    lmipro_render_ipc_receive(40000,27,0x1000400c);
    lmipro_render_ipc_receive(40000,29,0);
    for(int id=40001;id<=40003;++id)lmipro_render_ipc_receive(id,27,0);
    char receipts[420];
    lmipro_render_ipc_report(receipts,sizeof(receipts));
    assert(strstr(receipts,"p58 t77 SB226 ok5 fail2 other1 drop0"));
    assert(strstr(receipts,"id40000 from27 ok1 fail2 err10004004"));
    assert(strstr(receipts,"id40000 from29 ok1 fail0 err0"));
    gLmiproRenderReceiptBusy=1;
    lmipro_render_ipc_receive(40000,27,0);
    lmipro_render_ipc_report(receipts,sizeof(receipts));
    assert(strstr(receipts,"snapshot busy") && gLmiproRenderReceiptDropped==1);
    gLmiproRenderReceiptBusy=0;
    testPid=158;
    lmipro_render_ipc_receive(40199,27,0);
    lmipro_render_ipc_report(receipts,sizeof(receipts));
    assert(strstr(receipts,"p158 t77 SB226 ok1 fail0 other0 drop1"));
    assert(!strstr(receipts,"id40000") && strstr(receipts,"id40199 from27"));
    lmipro_render_ipc_send(40002,0x123,0,0); // Non-SpringBoard excluded.
    assert(!gLmiproRenderSend.entered);
    testName="SpringBoard";
    lmipro_render_ipc_send(40002,0x123,0,0);
    lmipro_render_ipc_send(40002,0x123,0x10000003,1);
    lmipro_render_ipc_send(40003,0x234,0,0);
    lmipro_render_ipc_send(40003,0x234,0,1);
    lmipro_render_ipc_report(receipts,sizeof(receipts));
    assert(strstr(receipts,"CA send SB158 in2 ret2 ok1 first10000003 id40003 port234"));
    lmipro_ui_stacks_enable(1);
    stackReady=1;
    lmipro_ui_user_event(1,-47,0x100,0,0,158);
    assert(!gLmiproSpringMain.state);
    testMain=1;
    lmipro_ui_user_event(1,-47,0x100,0,0,158);
    assert(gLmiproSpringMain.count==2 && gLmiproSpringMain.seconds==45);
    lmipro_ui_user_event(2,-47,0x100,0x10004003,0,158);
    lmipro_spring_main_report(receipts,sizeof(receipts));
    assert(strstr(receipts,"SB main p158 t77 at45 c-47 s2 n2 e0 R1/10004003"));
    assert(strstr(receipts,"12340 31220"));
    stackTime=59;
    lmipro_ui_user_event(1,375,0x100,0,0,158);
    assert(gLmiproSpringMain.code==-47);
    stackTime=60;
    lmipro_ui_user_event(1,375,0x100,0,0,158);
    assert(gLmiproSpringMain.code==375 && !gLmiproSpringMain.returned);
}
