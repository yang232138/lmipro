// Bounded nonwaiting diagnostics only. All arguments are copied scalar values.
#pragma once
#include <stdint.h>
struct LmiproDisplayMethod {
    uint32_t selector,status;
    int pid;
    uint64_t scalar[3];
};
struct LmiproDisplaySelector {
    uint64_t entered,returned;
    uint32_t status;
};
static volatile unsigned gLmiproDisplayTraceEnabled=0;
static volatile int gLmiproDisplayTraceBusy=0;
static volatile uint64_t gLmiproDisplayTraceDropped=0;
static uint64_t gLmiproDisplayEntered=0,gLmiproDisplayReturned=0;
static unsigned gLmiproDisplayErrorValid=0;
static LmiproDisplayMethod gLmiproDisplayLastIn={},gLmiproDisplayLastOut={},gLmiproDisplayFirstError={};
static LmiproDisplaySelector gLmiproDisplaySelectors[96]={};

static void lmiproRecordDisplayMethod(unsigned kind,uint32_t selector,uint32_t status,
    int pid,uint64_t a,uint64_t b,uint64_t c)
{
    if(!gLmiproDisplayTraceEnabled || kind>1)return;
    if(!__sync_bool_compare_and_swap(&gLmiproDisplayTraceBusy,0,1)) {
        __atomic_fetch_add(&gLmiproDisplayTraceDropped,1,__ATOMIC_RELAXED);return;
    }
    LmiproDisplayMethod event={selector,status,pid,{a,b,c}};
    if(kind==0) {
        ++gLmiproDisplayEntered;gLmiproDisplayLastIn=event;
        if(selector<96)++gLmiproDisplaySelectors[selector].entered;
    } else {
        ++gLmiproDisplayReturned;gLmiproDisplayLastOut=event;
        if(status && !gLmiproDisplayErrorValid) {
            gLmiproDisplayErrorValid=1;gLmiproDisplayFirstError=event;
        }
        if(selector<96) {
            ++gLmiproDisplaySelectors[selector].returned;
            gLmiproDisplaySelectors[selector].status=status;
        }
    }
    __sync_lock_release(&gLmiproDisplayTraceBusy);
}

static unsigned lmiproFormatDisplayMethods(char *out,unsigned capacity)
{
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproDisplayTraceBusy,0,1))
        return snprintf(out,capacity,"Display dispatch snapshot busy; unavailable\n");
    const uint64_t entered=gLmiproDisplayEntered,returned=gLmiproDisplayReturned;
    const LmiproDisplayMethod in=gLmiproDisplayLastIn,back=gLmiproDisplayLastOut,err=gLmiproDisplayFirstError;
    const unsigned valid=gLmiproDisplayErrorValid;
    LmiproDisplaySelector rows[4]={};unsigned selectors[4]={},shown=0,unique=0;
    for(unsigned i=0;i<96;++i) {
        if(!gLmiproDisplaySelectors[i].entered && !gLmiproDisplaySelectors[i].returned)continue;
        ++unique;
        if(shown<4){selectors[shown]=i;rows[shown]=gLmiproDisplaySelectors[i];++shown;}
    }
    __sync_lock_release(&gLmiproDisplayTraceBusy);
    int n=snprintf(out,capacity,
      "Display calls in=%llu out=%llu dropped=%llu\n"
      "last in sel=%u pid=%d a=%llx/%llx/%llx\n"
      "last out sel=%u pid=%d status=%x\n"
      "first error valid=%u sel=%u status=%x; expected errors possible\n"
      "selectors shown=%u of %u: ",
      (unsigned long long)entered,(unsigned long long)returned,(unsigned long long)gLmiproDisplayTraceDropped,
      in.selector,in.pid,(unsigned long long)in.scalar[0],(unsigned long long)in.scalar[1],(unsigned long long)in.scalar[2],
      back.selector,back.pid,back.status,valid,err.selector,err.status,shown,unique);
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<shown && used<capacity-1;++i) {
        n=snprintf(out+used,capacity-used,"%u:%llu/%llu/%x ",selectors[i],
          (unsigned long long)rows[i].entered,(unsigned long long)rows[i].returned,rows[i].status);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    if(used<capacity-1){out[used++]='\n';out[used]=0;}
    return used;
}
