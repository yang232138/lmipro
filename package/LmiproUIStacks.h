// Opt-in current-thread syscall-entry samples, not wait-state proof.
#pragma once
#include <stdint.h>
extern "C" unsigned lmipro_ui_stack_ready(void);
extern "C" unsigned lmipro_ui_stack_capture(uintptr_t *,unsigned,int *,unsigned *);
struct LmiproUIStack {
    int pid,code,error;uint64_t tid;unsigned state,count,info;uintptr_t pc[8];
};
static LmiproUIStack gLmiproUIStacks[8]={};
static volatile unsigned gLmiproUIStacksEnabled=0;
static int gLmiproUIStacksDisplayPid=0;
// Called only after an actual successful backboardd display-client creation.
// Keep a copied PID, never a user-client/task pointer. A restarted PID must open again.
extern "C" __attribute__((noinline)) void lmipro_ui_stack_display_open(int pid) {
    if(pid>0)__atomic_store_n(&gLmiproUIStacksDisplayPid,pid,__ATOMIC_RELEASE);
}
static volatile int gLmiproUIStacksBusy=0;
static unsigned gLmiproUIStacksStored=0;
static volatile uint64_t gLmiproUIStacksDropped=0;
extern "C" void lmipro_ui_stacks_enable(unsigned enabled) {gLmiproUIStacksEnabled=enabled==1;}
static __attribute__((noinline)) void lmiproSampleUIStack(int pid,uint64_t tid,int code) {
    if(!gLmiproUIStacksEnabled || pid<=0 || !tid || (code!=-47 && code!=375))return;
    if(__atomic_load_n(&gLmiproUIStacksDisplayPid,__ATOMIC_ACQUIRE)!=pid || !lmipro_ui_stack_ready())return;
    if(!__sync_bool_compare_and_swap(&gLmiproUIStacksBusy,0,1)) {
        __atomic_fetch_add(&gLmiproUIStacksDropped,1,__ATOMIC_RELAXED);return;
    }
    for(unsigned i=0;i<gLmiproUIStacksStored;++i)if(gLmiproUIStacks[i].pid==pid && gLmiproUIStacks[i].tid==tid) {
        __sync_lock_release(&gLmiproUIStacksBusy);return;
    }
    if(gLmiproUIStacksStored==8){__sync_lock_release(&gLmiproUIStacksBusy);return;}
    unsigned slot=gLmiproUIStacksStored++;
    gLmiproUIStacks[slot]={};gLmiproUIStacks[slot].pid=pid;gLmiproUIStacks[slot].tid=tid;
    gLmiproUIStacks[slot].code=code;gLmiproUIStacks[slot].state=1;
    __sync_lock_release(&gLmiproUIStacksBusy);
    // Faultable API runs after both observation locks have been released.
    // This reserved slot also prevents recursive samples of the same thread.
    uintptr_t frames[8]={};int error=0;unsigned info=0;
    unsigned count=lmipro_ui_stack_capture(frames,8,&error,&info);
    if(!__sync_bool_compare_and_swap(&gLmiproUIStacksBusy,0,1)) {
        __atomic_fetch_add(&gLmiproUIStacksDropped,1,__ATOMIC_RELAXED);return;
    }
    LmiproUIStack &s=gLmiproUIStacks[slot];s.error=error;s.info=info;
    if(count>8){s.state=3;s.count=0;} // Invalid API result stays explicit.
    else {s.count=count;for(unsigned i=0;i<count;++i)s.pc[i]=frames[i];s.state=2;}
    __sync_lock_release(&gLmiproUIStacksBusy);
}
extern "C" __attribute__((noinline)) unsigned lmipro_ui_stack_report(char *out,unsigned capacity) {
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproUIStacksBusy,0,1))
        return snprintf(out,capacity,"UI stacks snapshot busy; unavailable\n");
    LmiproUIStack rows[8]={};unsigned stored=gLmiproUIStacksStored;
    for(unsigned i=0;i<stored;++i)rows[i]=gLmiproUIStacks[i];
    __sync_lock_release(&gLmiproUIStacksBusy);
    int n=snprintf(out,capacity,"UI wait-entry stacks enabled=%u stored=%u cap8 depth8 delay30 dropped=%llu\n"
      " after display open pid=%d; codes=-47,375; entry only\n",
      gLmiproUIStacksEnabled,stored,(unsigned long long)gLmiproUIStacksDropped,
      __atomic_load_n(&gLmiproUIStacksDisplayPid,__ATOMIC_ACQUIRE));
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<stored && used<capacity-1;++i) {
        const LmiproUIStack &s=rows[i];
        n=snprintf(out+used,capacity-used,"pid%d tid%llu c%d state%u n%u err%d info%x:",
          s.pid,(unsigned long long)s.tid,s.code,s.state,s.count,s.error,s.info);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
        for(unsigned j=0;j<s.count && used<capacity-1;++j) {
            n=snprintf(out+used,capacity-used," %llx",(unsigned long long)s.pc[j]);if(n<0)break;
            used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
        }
        if(used<capacity-1){out[used++]='\n';out[used]=0;}
    }
    return used;
}
