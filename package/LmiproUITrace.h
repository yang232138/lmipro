// Read-only activity of the real compositor and desktop. No task pointers kept.
#pragma once
#include <stdint.h>
#include "LmiproUIStacks.h"
extern "C" void proc_selfname(char *, int);
extern "C" int proc_selfpid(void);
extern "C" unsigned lmipro_ui_is_main_thread(void);
struct LmiproUIEvent {int code;uint64_t pc,a,b,tid;};
struct LmiproUIActor {
    int pid;
    uint64_t generations,entries,returns,exceptions,interrupts;
    LmiproUIEvent entry,returned,exception;
};
static volatile unsigned gLmiproUIEnabled=0;
static volatile int gLmiproUIBusy=0;
static volatile uint64_t gLmiproUIDropped=0;
static LmiproUIActor gLmiproUIActors[2]={};
struct LmiproUIMkdirError {unsigned valid;int pid,code;uint64_t error,pc,tid;};
static LmiproUIMkdirError gLmiproUIMkdirErrors[2]={};
static const char *const gLmiproUINames[2]={"SpringBoard","backboardd"};
#include "LmiproUIExit.h"
extern "C" void lmipro_ui_trace_enable(unsigned enabled) {gLmiproUIEnabled=enabled==1;}

struct LmiproRenderReceipt {
    int id, sender;
    uint64_t delivered, failed;
    unsigned firstError;
};
static LmiproRenderReceipt gLmiproRenderReceipts[4]={};
static unsigned gLmiproRenderReceiptCount=0;
static volatile unsigned gLmiproRenderReceiptBusy=0;
static volatile uint64_t gLmiproRenderReceiptDropped=0;
static uint64_t gLmiproRenderDelivered=0, gLmiproRenderFailed=0, gLmiproRenderUnlisted=0;
static uint64_t gLmiproRenderLastTid=0;
static int gLmiproRenderLastPid=0;

// Show existing samples when this boot has no backboardd render receipt.
// The two snapshots may refer to different process generations; print both PIDs.
extern "C" __attribute__((noinline)) unsigned lmipro_quiet_display_report(
    char *out, unsigned capacity)
{
    if (!out || !capacity) return 0;
    out[0] = 0;
    if (!gLmiproUIEnabled) return 0;
    if (!__sync_bool_compare_and_swap(&gLmiproRenderReceiptBusy, 0, 1))
        return snprintf(out, capacity, "BB receipt snapshot busy\n");
    const bool hasReceipt = gLmiproRenderLastPid != 0;
    __sync_lock_release(&gLmiproRenderReceiptBusy);
    if (hasReceipt) return 0;

    // -1 denotes an unavailable snapshot. 0 denotes no matching observation.
    int activityPid = -1, exitPid = -1;
    uint64_t generation = 0, exits = 0;
    LmiproUIExit last = {};
    if (__sync_bool_compare_and_swap(&gLmiproUIBusy, 0, 1)) {
        activityPid = gLmiproUIActors[1].pid;
        generation = gLmiproUIActors[1].generations;
        __sync_lock_release(&gLmiproUIBusy);
    }
    if (__sync_bool_compare_and_swap(&gLmiproUIExitBusy, 0, 1)) {
        last = gLmiproUIExitLast[1];
        exits = gLmiproUIExitCounts[1];
        exitPid = last.pid;
        __sync_lock_release(&gLmiproUIExitBusy);
    }
    return snprintf(out, capacity,
        "BB activity pid%d gen%llu exitPid%d count%llu raw%x ns%u code%llx\n",
        activityPid, (unsigned long long)generation, exitPid, (unsigned long long)exits,
        last.raw, last.ns, (unsigned long long)last.code);
}
struct LmiproRenderSend {
    int pid,lastId;
    uint32_t lastPort,firstError;
    uint64_t entered,returned,success;
};
static LmiproRenderSend gLmiproRenderSend={};

extern "C" __attribute__((noinline)) void lmipro_render_ipc_send(int id,uint32_t port,int status,unsigned phase) {
    if(!gLmiproUIEnabled || id<40000 || id>40199 || phase>1)return;
    char name[17]={};
    proc_selfname(name,sizeof(name));
    if(strcmp(name,"SpringBoard"))return;
    const int pid=proc_selfpid();
    if(!__sync_bool_compare_and_swap(&gLmiproRenderReceiptBusy,0,1)) {
        __atomic_fetch_add(&gLmiproRenderReceiptDropped,1,__ATOMIC_RELAXED);
        return;
    }
    auto &send=gLmiproRenderSend;
    if(send.pid!=pid) {
        send={};
        send.pid=pid;
    }
    send.lastId=id;
    send.lastPort=port;
    if(phase==0)++send.entered;
    else {
        ++send.returned;
        if(!status)++send.success;
        else if(!send.firstError)send.firstError=(uint32_t)status;
    }
    __sync_lock_release(&gLmiproRenderReceiptBusy);
}

// The Mach receive path has released the kmsg before calling this observer.
// A successful copyout only proves delivery to the receiver, not CA acceptance.
extern "C" __attribute__((noinline)) void lmipro_render_ipc_receive(int id,int sender,int status) {
    if(!gLmiproUIEnabled || id<40000 || id>40199)return;
    char name[17]={};
    proc_selfname(name,sizeof(name));
    if(strcmp(name,"backboardd"))return;
    const int pid=proc_selfpid();
    const uint64_t tid=thread_tid(current_thread());
    if(!__sync_bool_compare_and_swap(&gLmiproRenderReceiptBusy,0,1)) {
        __atomic_fetch_add(&gLmiproRenderReceiptDropped,1,__ATOMIC_RELAXED);
        return;
    }
    if(gLmiproRenderLastPid!=pid) {
        gLmiproRenderReceiptCount=0;
        gLmiproRenderDelivered=0;
        gLmiproRenderFailed=0;
        gLmiproRenderUnlisted=0;
    }
    if(status)++gLmiproRenderFailed;
    else ++gLmiproRenderDelivered;
    gLmiproRenderLastTid=tid;
    gLmiproRenderLastPid=pid;
    unsigned slot=0;
    while(slot<gLmiproRenderReceiptCount) {
        const auto &row=gLmiproRenderReceipts[slot];
        if(row.id==id && row.sender==sender)break;
        ++slot;
    }
    if(slot==gLmiproRenderReceiptCount && slot<4) {
        gLmiproRenderReceipts[slot]={id,sender,0,0,0};
        ++gLmiproRenderReceiptCount;
    }
    if(slot<4) {
        auto &row=gLmiproRenderReceipts[slot];
        if(status) {
            ++row.failed;
            if(!row.firstError)row.firstError=(unsigned)status;
        } else ++row.delivered;
    } else ++gLmiproRenderUnlisted;
    __sync_lock_release(&gLmiproRenderReceiptBusy);
}

extern "C" __attribute__((noinline)) unsigned lmipro_render_ipc_report(char *out,unsigned capacity) {
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproRenderReceiptBusy,0,1))
        return snprintf(out,capacity,"RenderIPC/v1 snapshot busy\n");
    LmiproRenderReceipt rows[4];
    unsigned count=gLmiproRenderReceiptCount;
    for(unsigned i=0;i<count;++i)rows[i]=gLmiproRenderReceipts[i];
    const uint64_t ok=gLmiproRenderDelivered, fail=gLmiproRenderFailed, other=gLmiproRenderUnlisted;
    const uint64_t tid=gLmiproRenderLastTid;
    const int pid=gLmiproRenderLastPid;
    const auto send=gLmiproRenderSend;
    __sync_lock_release(&gLmiproRenderReceiptBusy);
    int springboard=-1;
    if(__sync_bool_compare_and_swap(&gLmiproUIBusy,0,1)) {
        springboard=gLmiproUIActors[0].pid;
        __sync_lock_release(&gLmiproUIBusy);
    }
    int n=snprintf(out,capacity,"RenderIPC/v1 40000-40199 p%d t%llu SB%d ok%llu fail%llu other%llu drop%llu\n",
        pid,(unsigned long long)tid,springboard,(unsigned long long)ok,(unsigned long long)fail,
        (unsigned long long)other,(unsigned long long)__atomic_load_n(&gLmiproRenderReceiptDropped,__ATOMIC_RELAXED));
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    n=snprintf(out+used,capacity-used,"CA send SB%d in%llu ret%llu ok%llu first%x id%d port%x\n",
        send.pid,(unsigned long long)send.entered,(unsigned long long)send.returned,
        (unsigned long long)send.success,send.firstError,send.lastId,send.lastPort);
    if(n<0)return used;
    used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    for(unsigned i=0;i<count && used<capacity-1;++i) {
        const auto &row=rows[i];
        n=snprintf(out+used,capacity-used,"id%d from%d ok%llu fail%llu err%x\n",row.id,row.sender,
            (unsigned long long)row.delivered,(unsigned long long)row.failed,row.firstError);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    return used;
}

static LmiproUIStack gLmiproSpringMain={};
static volatile unsigned gLmiproSpringMainBusy=0;
static volatile unsigned gLmiproSpringMainInFlight=0;
static volatile uint64_t gLmiproSpringMainDropped=0;

// Sample the fork-created main thread using the pinned XNU thread tag.
// A stack is a syscall-entry observation, even when no return was recorded.
static __attribute__((noinline)) void lmiproSpringMainEvent(unsigned kind,int pid,uint64_t tid,
    int code,uint64_t result) {
    if(!gLmiproUIStacksEnabled || (kind!=1 && kind!=2) || !lmipro_ui_is_main_thread())return;
    if(!__sync_bool_compare_and_swap(&gLmiproSpringMainBusy,0,1)) {
        __atomic_fetch_add(&gLmiproSpringMainDropped,1,__ATOMIC_RELAXED);
        return;
    }
    auto &row=gLmiproSpringMain;
    if(kind==2) {
        if(row.pid==pid && row.tid==tid && row.code==code && row.state==2 && !row.returned) {
            row.result=result;
            row.returned=1;
        }
        __sync_lock_release(&gLmiproSpringMainBusy);
        return;
    }
    const uint64_t now=lmipro_ui_stack_seconds();
    if(__atomic_load_n(&gLmiproSpringMainInFlight,__ATOMIC_ACQUIRE) ||
       (row.pid==pid && now>=row.seconds && now-row.seconds<15) ||
       !lmipro_ui_stack_ready()) {
        __sync_lock_release(&gLmiproSpringMainBusy);
        return;
    }
    row={};
    row.pid=pid;
    row.tid=tid;
    row.code=code;
    row.seconds=now;
    row.state=1;
    __atomic_store_n(&gLmiproSpringMainInFlight,1,__ATOMIC_RELEASE);
    __sync_lock_release(&gLmiproSpringMainBusy);
    uintptr_t frames[8]={};
    int error=0;
    unsigned flags=0;
    const unsigned count=lmipro_ui_stack_capture(frames,8,&error,&flags);
    if(!__sync_bool_compare_and_swap(&gLmiproSpringMainBusy,0,1)) {
        __atomic_fetch_add(&gLmiproSpringMainDropped,1,__ATOMIC_RELAXED);
        __atomic_store_n(&gLmiproSpringMainInFlight,0,__ATOMIC_RELEASE);
        return;
    }
    row.count=count<=8?count:0;
    row.error=error;
    row.info=flags;
    for(unsigned i=0;i<row.count;++i)row.pc[i]=frames[i];
    row.state=count<=8?2:3;
    __atomic_store_n(&gLmiproSpringMainInFlight,0,__ATOMIC_RELEASE);
    __sync_lock_release(&gLmiproSpringMainBusy);
}

extern "C" __attribute__((noinline)) unsigned lmipro_spring_main_report(char *out,unsigned capacity) {
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproSpringMainBusy,0,1))
        return snprintf(out,capacity,"SB main snapshot busy\n");
    auto row=gLmiproSpringMain;
    if(row.state==1 && !__atomic_load_n(&gLmiproSpringMainInFlight,__ATOMIC_ACQUIRE))row.state=3;
    __sync_lock_release(&gLmiproSpringMainBusy);
    int n=snprintf(out,capacity,"SB main p%d t%llu at%llu c%d s%u n%u e%d R%u/%llx drop%llu",
        row.pid,(unsigned long long)row.tid,(unsigned long long)row.seconds,row.code,row.state,row.count,
        row.error,row.returned,(unsigned long long)row.result,
        (unsigned long long)__atomic_load_n(&gLmiproSpringMainDropped,__ATOMIC_RELAXED));
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<row.count && used<capacity-1;++i) {
        n=snprintf(out+used,capacity-used," %llx",(unsigned long long)row.pc[i]);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    if(used<capacity-1){out[used++]='\n';out[used]=0;}
    return used;
}

static void lmiproRecordUI(unsigned index,int pid,unsigned kind,int code,
    uint64_t pc,uint64_t a,uint64_t b,uint64_t tid)
{
    if(!gLmiproUIEnabled || index>=2 || pid<=0 || kind<1 || kind>4)return;
    if(!__sync_bool_compare_and_swap(&gLmiproUIBusy,0,1)) {
        __atomic_fetch_add(&gLmiproUIDropped,1,__ATOMIC_RELAXED);return;
    }
    LmiproUIActor &s=gLmiproUIActors[index];
    // A restarted daemon is a new observation generation. Do not combine its
    // old entry/return PCs with the new process and imply a pending syscall.
    if(s.pid!=pid) {
        uint64_t generation=s.generations+1;s={};s.pid=pid;s.generations=generation;
    }
    if(kind==2 && (code==136 || code==475) && (b & (UINT64_C(1)<<29)) &&
       !gLmiproUIMkdirErrors[index].valid)
        gLmiproUIMkdirErrors[index]={1,pid,code,a,pc,tid};
    LmiproUIEvent event={code,pc,a,b,tid};
    if(kind==1){++s.entries;s.entry=event;}
    else if(kind==2){++s.returns;s.returned=event;}
    else if(kind==3){++s.exceptions;s.exception=event;}
    else ++s.interrupts;
    __sync_lock_release(&gLmiproUIBusy);
}
extern "C" __attribute__((noinline)) void lmipro_ui_user_event(unsigned kind,int code,
    uint64_t pc,uint64_t a,uint64_t b,int pid)
{
    if(!gLmiproUIEnabled || kind<1 || kind>4)return;
    // Pinned proc_selfname only copies current_proc()->p_name. Unlike proc_name,
    // it performs no proc_find, allocation or reference/transition wait.
    char name[17]={};proc_selfname(name,sizeof(name));name[16]=0;
    for(unsigned i=0;i<2;++i)if(!strcmp(name,gLmiproUINames[i])) {
        uint64_t tid=thread_tid(current_thread());
        lmiproRecordUI(i,pid,kind,code,pc,a,b,tid);
        if(i==0)lmiproSpringMainEvent(kind,pid,tid,code,a);
        if(i==1 && kind==1)lmiproSampleUIStack(pid,tid,code);
        if(i==1 && kind==2)lmiproReturnUIStack(pid,tid,code,a,b);
        if(i==1 && kind==4)lmiproInterruptUIStack(pid,tid,pc);
        return;
    }
}
static uint64_t gLmiproWatchdogFaultPC=0,gLmiproWatchdogFaultLR=0;
static unsigned gLmiproWatchdogFaultValid=0;
extern "C" void lmipro_watchdog_exit_report(int pid,const char *name,char *out,unsigned capacity) {
    if(!gLmiproUIEnabled || !name || strcmp(name,"watchdogd") || !out || !capacity)return;
    if(!__sync_bool_compare_and_swap(&gLmiproUIBusy,0,1))return;
    uint64_t pc=gLmiproWatchdogFaultPC,lr=gLmiproWatchdogFaultLR;
    unsigned valid=gLmiproWatchdogFaultValid;
    __sync_lock_release(&gLmiproUIBusy);
    snprintf(out,capacity,"watchdogd[%d] exited BRK v=%u pc=%llx lr=%llx",
             pid,valid,(unsigned long long)pc,(unsigned long long)lr);
}
static unsigned gLmiproUIFaultValid=0;
static uint64_t gLmiproUIFaultPC=0,gLmiproUIFaultWords[10]={};
extern "C" __attribute__((noinline)) void lmipro_ui_fault_event(uint64_t pc,const uint64_t *regs) {
    if(!gLmiproUIEnabled || !regs)return;
    char name[17]={};proc_selfname(name,sizeof(name));name[16]=0;
    if(!strcmp(name,"watchdogd")) {
        if(!__sync_bool_compare_and_swap(&gLmiproUIBusy,0,1))return;
        gLmiproWatchdogFaultPC=pc;
        gLmiproWatchdogFaultLR=regs[30];
        gLmiproWatchdogFaultValid=1;
        __sync_lock_release(&gLmiproUIBusy);
        return;
    }
    if(strcmp(name,"SpringBoard"))return;
    if(!__sync_bool_compare_and_swap(&gLmiproUIBusy,0,1)) {
        __atomic_fetch_add(&gLmiproUIDropped,1,__ATOMIC_RELAXED);return;
    }
    if(!gLmiproUIFaultValid) {
        gLmiproUIFaultPC=pc;
        for(unsigned i=0;i<10;++i)gLmiproUIFaultWords[i]=regs[19+i];
        gLmiproUIFaultValid=1;
    }
    __sync_lock_release(&gLmiproUIBusy);
}
extern "C" __attribute__((noinline)) unsigned lmipro_ui_trace_report(char *out,unsigned capacity)
{
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproUIBusy,0,1))
        return snprintf(out,capacity,"UI activity snapshot busy; unavailable\n");
    LmiproUIActor snapshot[2]={gLmiproUIActors[0],gLmiproUIActors[1]};
    const LmiproUIMkdirError mkdirErrors[2]={gLmiproUIMkdirErrors[0],gLmiproUIMkdirErrors[1]};
    const unsigned faultValid=gLmiproUIFaultValid;
    const uint64_t faultPC=gLmiproUIFaultPC;
    uint64_t words[10]={};memcpy(words,gLmiproUIFaultWords,sizeof(words));
    __sync_lock_release(&gLmiproUIBusy);
    int n=snprintf(out,capacity,"UI activity enabled=%u dropped=%llu; last events across threads\n",
        gLmiproUIEnabled,(unsigned long long)gLmiproUIDropped);
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<2 && used<capacity-1;++i) {
        const LmiproUIActor &s=snapshot[i];
        n=snprintf(out+used,capacity-used,
          "%s pid=%d gen=%llu in=%llu out=%llu exc=%llu irq=%llu\n"
          " in c=%d pc=%llx a=%llx/%llx tid=%llu\n"
          " out c=%d pc=%llx r=%llx psr=%llx tid=%llu\n"
          " exc ec=%x pc=%llx esr=%llx far=%llx tid=%llu\n",
          gLmiproUINames[i],s.pid,(unsigned long long)s.generations,
          (unsigned long long)s.entries,(unsigned long long)s.returns,
          (unsigned long long)s.exceptions,(unsigned long long)s.interrupts,
          s.entry.code,(unsigned long long)s.entry.pc,(unsigned long long)s.entry.a,
          (unsigned long long)s.entry.b,(unsigned long long)s.entry.tid,
          s.returned.code,(unsigned long long)s.returned.pc,(unsigned long long)s.returned.a,
          (unsigned long long)s.returned.b,(unsigned long long)s.returned.tid,
          (unsigned)s.exception.code,(unsigned long long)s.exception.pc,
          (unsigned long long)s.exception.a,(unsigned long long)s.exception.b,
          (unsigned long long)s.exception.tid);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    if(used<capacity-1) {
        n=snprintf(out+used,capacity-used,
          "mkdir first SB=%u pid=%d c=%d errno=%llu pc=%llx\n"
          "mkdir first BBD=%u pid=%d c=%d errno=%llu pc=%llx\n"
          "UI first BRK valid=%u pc=%llx x24=%llx x25=%llx x26=%llx\n"
          " x22=%llx x23=%llx x28=%llx x19=%llx\n",
          mkdirErrors[0].valid,mkdirErrors[0].pid,mkdirErrors[0].code,(unsigned long long)mkdirErrors[0].error,(unsigned long long)mkdirErrors[0].pc,
          mkdirErrors[1].valid,mkdirErrors[1].pid,mkdirErrors[1].code,(unsigned long long)mkdirErrors[1].error,(unsigned long long)mkdirErrors[1].pc,faultValid,
          (unsigned long long)faultPC,(unsigned long long)words[5],(unsigned long long)words[6],
          (unsigned long long)words[7],(unsigned long long)words[3],(unsigned long long)words[4],
          (unsigned long long)words[9],(unsigned long long)words[0]);
        if(n>0)used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    return used;
}
