// Read-only activity of the real compositor and desktop. No task pointers kept.
#pragma once
#include <stdint.h>
extern "C" void proc_selfname(char *, int);
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
static const char *const gLmiproUINames[2]={"SpringBoard","backboardd"};
extern "C" void lmipro_ui_trace_enable(unsigned enabled) {gLmiproUIEnabled=enabled==1;}

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
        lmiproRecordUI(i,pid,kind,code,pc,a,b,thread_tid(current_thread()));return;
    }
}
static unsigned gLmiproUIFaultValid=0;
static uint64_t gLmiproUIFaultPC=0,gLmiproUIFaultWords[10]={};
extern "C" __attribute__((noinline)) void lmipro_ui_fault_event(uint64_t pc,const uint64_t *regs) {
    if(!gLmiproUIEnabled || !regs)return;
    char name[17]={};proc_selfname(name,sizeof(name));name[16]=0;
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
          "UI first BRK valid=%u pc=%llx x24=%llx x25=%llx x26=%llx\n"
          " x22=%llx x23=%llx x28=%llx x19=%llx\n",faultValid,
          (unsigned long long)faultPC,(unsigned long long)words[5],(unsigned long long)words[6],
          (unsigned long long)words[7],(unsigned long long)words[3],(unsigned long long)words[4],
          (unsigned long long)words[9],(unsigned long long)words[0]);
        if(n>0)used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    return used;
}
