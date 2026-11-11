// Opt-in committed process exits. Kernel-owned scalar copies only.
#pragma once
struct LmiproUIExit {
    int pid;unsigned raw,ns;uint64_t code;
};
static LmiproUIExit gLmiproUIExitFirst[2]={},gLmiproUIExitLast[2]={};
static uint64_t gLmiproUIExitCounts[2]={};
static volatile int gLmiproUIExitBusy=0;
static volatile uint64_t gLmiproUIExitDropped=0;
extern "C" __attribute__((noinline)) void lmipro_ui_exit_event(int pid,const char *name,int raw,unsigned ns,uint64_t code) {
    if(!gLmiproUIEnabled || pid<=0 || !name)return;
    char copied[17]={};for(unsigned i=0;i<16 && name[i];++i)copied[i]=name[i];
    unsigned actor=2;
    for(unsigned i=0;i<2;++i)if(!strcmp(copied,gLmiproUINames[i]))actor=i;
    if(actor==2)return;
    if(!__sync_bool_compare_and_swap(&gLmiproUIExitBusy,0,1)) {
        __atomic_fetch_add(&gLmiproUIExitDropped,1,__ATOMIC_RELAXED);return;
    }
    LmiproUIExit event={pid,(unsigned)raw,ns,code};
    if(!gLmiproUIExitCounts[actor])gLmiproUIExitFirst[actor]=event;
    gLmiproUIExitLast[actor]=event;++gLmiproUIExitCounts[actor];
    __sync_lock_release(&gLmiproUIExitBusy);
}
extern "C" __attribute__((noinline)) unsigned lmipro_ui_exit_report(char *out,unsigned capacity) {
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproUIExitBusy,0,1))
        return snprintf(out,capacity,"UI exits snapshot busy; unavailable\n");
    LmiproUIExit first[2]={gLmiproUIExitFirst[0],gLmiproUIExitFirst[1]};
    LmiproUIExit last[2]={gLmiproUIExitLast[0],gLmiproUIExitLast[1]};
    uint64_t counts[2]={gLmiproUIExitCounts[0],gLmiproUIExitCounts[1]};
    __sync_lock_release(&gLmiproUIExitBusy);
    int n=snprintf(out,capacity,"UI committed exits enabled=%u dropped=%llu; raw wait status\n",
      gLmiproUIEnabled,(unsigned long long)gLmiproUIExitDropped);
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned actor=0;actor<2 && used<capacity-1;++actor) {
        for(unsigned which=0;which<2 && used<capacity-1;++which) {
            const LmiproUIExit &s=which?last[actor]:first[actor];
            n=snprintf(out+used,capacity-used,"%s %s count=%llu pid=%d raw=%x sig=%u exit=%u ns=%u code=%llx\n",
              gLmiproUINames[actor],which?"last":"first",(unsigned long long)counts[actor],
              s.pid,s.raw,s.raw&0x7f,(s.raw>>8)&0xff,s.ns,(unsigned long long)s.code);
            if(n<0)return used;
            used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
        }
    }
    return used;
}
