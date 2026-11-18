// Bounded nonwaiting diagnostics only. All arguments are copied scalar values.
#pragma once
#include <stdint.h>
struct LmiproDisplayMethod {
    uint32_t selector,status;
    int pid;
    uint64_t scalar[3];
    char name[17];
};
struct LmiproDisplaySelector {
    uint64_t entered,returned;
    uint32_t status;
    uint64_t output[3];
    unsigned outputCount;
    // Latest request shape and caller, copied in the same guarded update as the counts.
    uint32_t inScalars,inStructBytes,outScalars,outStructBytes;
    int pid;
};
static volatile unsigned gLmiproDisplayTraceEnabled=0;
static volatile int gLmiproDisplayTraceBusy=0;
static volatile uint64_t gLmiproDisplayTraceDropped=0;
static uint64_t gLmiproDisplayEntered=0,gLmiproDisplayReturned=0;
static unsigned gLmiproDisplayErrorValid=0;
static LmiproDisplayMethod gLmiproDisplayLastIn={},gLmiproDisplayLastOut={},gLmiproDisplayFirstError={};
static LmiproDisplaySelector gLmiproDisplaySelectors[96]={};

static unsigned lmiproFormatSwapCounts(char *out,unsigned capacity) {
    if(!__sync_bool_compare_and_swap(&gLmiproDisplayTraceBusy,0,1))
        return snprintf(out,capacity,"Swap snapshot busy\n");
    const auto begin=gLmiproDisplaySelectors[4], end=gLmiproDisplaySelectors[5];
    const uint64_t dropped=__atomic_load_n(&gLmiproDisplayTraceDropped,__ATOMIC_RELAXED);
    __sync_lock_release(&gLmiproDisplayTraceBusy);
    return snprintf(out,capacity,"Swap 4:%llu/%llu/%x 5:%llu/%llu/%x drop%llu\n",
        (unsigned long long)begin.entered,(unsigned long long)begin.returned,begin.status,
        (unsigned long long)end.entered,(unsigned long long)end.returned,end.status,
        (unsigned long long)dropped);
}

static void lmiproRecordDisplayMethod(unsigned kind,uint32_t selector,uint32_t status,
    int pid,uint64_t a,uint64_t b,uint64_t c,const char *name=nullptr,unsigned outputCount=0,
    uint32_t inScalars=0,uint32_t inStructBytes=0,uint32_t outScalars=0,uint32_t outStructBytes=0)
{
    if(!gLmiproDisplayTraceEnabled || kind>1)return;
    if(!__sync_bool_compare_and_swap(&gLmiproDisplayTraceBusy,0,1)) {
        __atomic_fetch_add(&gLmiproDisplayTraceDropped,1,__ATOMIC_RELAXED);return;
    }
    LmiproDisplayMethod event={selector,status,pid,{a,b,c},{}};
    if(name)for(unsigned i=0;i<16 && name[i];++i)event.name[i]=name[i];
    if(kind==0) {
        ++gLmiproDisplayEntered;gLmiproDisplayLastIn=event;
        if(selector<96) {
            ++gLmiproDisplaySelectors[selector].entered;
            gLmiproDisplaySelectors[selector].inScalars=inScalars;
            gLmiproDisplaySelectors[selector].inStructBytes=inStructBytes;
            gLmiproDisplaySelectors[selector].pid=pid;
        }
    } else {
        ++gLmiproDisplayReturned;gLmiproDisplayLastOut=event;
        if(status && !gLmiproDisplayErrorValid) {
            gLmiproDisplayErrorValid=1;gLmiproDisplayFirstError=event;
        }
        if(selector<96) {
            ++gLmiproDisplaySelectors[selector].returned;
            gLmiproDisplaySelectors[selector].status=status;
            gLmiproDisplaySelectors[selector].outputCount=outputCount;
            gLmiproDisplaySelectors[selector].output[0]=a;
            gLmiproDisplaySelectors[selector].output[1]=b;
            gLmiproDisplaySelectors[selector].output[2]=c;
            gLmiproDisplaySelectors[selector].outScalars=outScalars;
            gLmiproDisplaySelectors[selector].outStructBytes=outStructBytes;
            gLmiproDisplaySelectors[selector].pid=pid;
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
    LmiproDisplaySelector rows[16]={};unsigned selectors[16]={},shown=0,unique=0;
    const LmiproDisplaySelector dimensions=gLmiproDisplaySelectors[8],pitch=gLmiproDisplaySelectors[28];
    const LmiproDisplaySelector sel5=gLmiproDisplaySelectors[5],sel17=gLmiproDisplaySelectors[17],sel27=gLmiproDisplaySelectors[27];
    for(unsigned i=0;i<96;++i) {
        if(!gLmiproDisplaySelectors[i].entered && !gLmiproDisplaySelectors[i].returned)continue;
        ++unique;
        if(shown<16){selectors[shown]=i;rows[shown]=gLmiproDisplaySelectors[i];++shown;}
    }
    __sync_lock_release(&gLmiproDisplayTraceBusy);
    int n=snprintf(out,capacity,
      "Display calls in=%llu out=%llu dropped=%llu\n"
      "last in sel=%u pid=%d name=%s a=%llx/%llx/%llx\n"
      "last out sel=%u pid=%d name=%s status=%x\n"
      "first error valid=%u sel=%u status=%x; expected errors possible\n"
      "scalar replies: 8 n=%u %llx/%llx; 28 n=%u %llx\n"
      "sel5 in=%u/%u out=%u/%u pid=%d st=%x ent=%llu ret=%llu\n"
      "sel17 in=%u/%u out=%u/%u pid=%d st=%x ent=%llu ret=%llu\n"
      "sel27 in=%u/%u out=%u/%u pid=%d st=%x ent=%llu ret=%llu\n"
      "selectors shown=%u of %u (cap16): ",
      (unsigned long long)entered,(unsigned long long)returned,(unsigned long long)gLmiproDisplayTraceDropped,
      in.selector,in.pid,in.name,(unsigned long long)in.scalar[0],(unsigned long long)in.scalar[1],(unsigned long long)in.scalar[2],
      back.selector,back.pid,back.name,back.status,valid,err.selector,err.status,
      dimensions.outputCount,(unsigned long long)dimensions.output[0],(unsigned long long)dimensions.output[1],
      pitch.outputCount,(unsigned long long)pitch.output[0],
      sel5.inScalars,sel5.inStructBytes,sel5.outScalars,sel5.outStructBytes,sel5.pid,sel5.status,
      (unsigned long long)sel5.entered,(unsigned long long)sel5.returned,
      sel17.inScalars,sel17.inStructBytes,sel17.outScalars,sel17.outStructBytes,sel17.pid,sel17.status,
      (unsigned long long)sel17.entered,(unsigned long long)sel17.returned,
      sel27.inScalars,sel27.inStructBytes,sel27.outScalars,sel27.outStructBytes,sel27.pid,sel27.status,
      (unsigned long long)sel27.entered,(unsigned long long)sel27.returned,
      shown,unique);
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

struct LmiproDisplayClient {
    uint64_t tid;unsigned type;int pid;char name[17];
    unsigned returned,ambiguous;uint32_t status;
};
static LmiproDisplayClient gLmiproClients[8]={};
static unsigned gLmiproClientStored=0;
static uint64_t gLmiproClientDropped=0;
static volatile int gLmiproClientBusy=0;
static void lmiproClientEvent(unsigned kind,uint64_t tid,unsigned type,int pid,const char *name,uint32_t status) {
    if(!gLmiproDisplayTraceEnabled || !tid || kind>1)return;
    if(!__sync_bool_compare_and_swap(&gLmiproClientBusy,0,1)) {
        __atomic_fetch_add(&gLmiproClientDropped,1,__ATOMIC_RELAXED);return;
    }
    int pending=-1;
    for(unsigned i=0;i<gLmiproClientStored;++i)
        if(gLmiproClients[i].tid==tid && !gLmiproClients[i].returned)pending=(int)i;
    if(kind==0) {
        if(pending>=0) {gLmiproClients[pending].ambiguous=1;__atomic_fetch_add(&gLmiproClientDropped,1,__ATOMIC_RELAXED);}
        else if(gLmiproClientStored<8) {
            LmiproDisplayClient &s=gLmiproClients[gLmiproClientStored++];
            s={};s.tid=tid;s.type=type;s.pid=pid;
            if(name)for(unsigned i=0;i<16 && name[i];++i)s.name[i]=name[i];
        } else __atomic_fetch_add(&gLmiproClientDropped,1,__ATOMIC_RELAXED);
    } else if(pending>=0 && !gLmiproClients[pending].ambiguous) {
        gLmiproClients[pending].status=status;gLmiproClients[pending].returned=1;
    } else __atomic_fetch_add(&gLmiproClientDropped,1,__ATOMIC_RELAXED);
    __sync_lock_release(&gLmiproClientBusy);
}
static unsigned lmiproFormatClients(char *out,unsigned capacity) {
    if(!out || !capacity)return 0;
    if(!__sync_bool_compare_and_swap(&gLmiproClientBusy,0,1))
        return snprintf(out,capacity,"Display opens snapshot busy; unavailable\n");
    LmiproDisplayClient rows[8]={};unsigned stored=gLmiproClientStored;
    for(unsigned i=0;i<stored;++i)rows[i]=gLmiproClients[i];
    uint64_t dropped=__atomic_load_n(&gLmiproClientDropped,__ATOMIC_RELAXED);
    __sync_lock_release(&gLmiproClientBusy);
    int n=snprintf(out,capacity,"Display opens first%u cap8 dropped=%llu:\n",stored,(unsigned long long)dropped);
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<stored && used<capacity-1;++i) {
        const LmiproDisplayClient &s=rows[i];
        n=snprintf(out+used,capacity-used,"%d:%s type%u done%u amb%u=%x ",s.pid,s.name,s.type,s.returned,s.ambiguous,s.status);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    if(used<capacity-1){out[used++]='\n';out[used]=0;}return used;
}

// Capture at most one completed backboardd call per selected initialization API.
// Reservation and publication are guarded. The faultable capture runs unlocked.
struct LmiproDisplayCaller {
    unsigned state, selector, status, count, flags;
    int pid, error;
    uint64_t tid;
    uintptr_t frames[8];
};
static LmiproDisplayCaller gLmiproDisplayCallers[3] = {};
static volatile int gLmiproDisplayCallerBusy = 0;
template <typename Capture>
static void lmiproCaptureDisplayCaller(unsigned selector, unsigned status, int pid,
    uint64_t tid, const char *name, Capture capture)
{
    if (!gLmiproDisplayTraceEnabled || !name || strcmp(name, "backboardd")) return;
    unsigned slot;
    if (selector == 57) slot = 0;
    else if (selector == 27) slot = 1;
    else if (selector == 50) slot = 2;
    else return;
    if (!__sync_bool_compare_and_swap(&gLmiproDisplayCallerBusy, 0, 1)) return;
    if (__atomic_load_n(&gLmiproDisplayCallers[slot].state, __ATOMIC_ACQUIRE)) {
        __sync_lock_release(&gLmiproDisplayCallerBusy);
        return;
    }
    __atomic_store_n(&gLmiproDisplayCallers[slot].state, 1, __ATOMIC_RELEASE);
    __sync_lock_release(&gLmiproDisplayCallerBusy);
    LmiproDisplayCaller row = {};
    row.state = 2; row.selector = selector; row.status = status;
    row.pid = pid; row.tid = tid;
    row.count = capture(row.frames, 8, &row.error, &row.flags);
    if (row.count > 8) { row.count = 0; row.state = 3; }
    // Only this reservation's owner writes its row. Publish last.
    unsigned state = row.state;
    LmiproDisplayCaller &dest = gLmiproDisplayCallers[slot];
    dest.selector=row.selector; dest.status=row.status; dest.count=row.count;
    dest.flags=row.flags; dest.pid=row.pid; dest.error=row.error; dest.tid=row.tid;
    for(unsigned i=0;i<8;++i)dest.frames[i]=row.frames[i];
    __atomic_store_n(&gLmiproDisplayCallers[slot].state, state, __ATOMIC_RELEASE);
}
static unsigned lmiproFormatDisplayCallers(char *out, unsigned capacity)
{
    if (!out || !capacity) return 0;
    int n = snprintf(out, capacity, "BBcall/v1 first returns, frames are runtime PCs\n");
    if (n < 0) return 0;
    unsigned used = (unsigned)n < capacity ? (unsigned)n : capacity - 1;
    for (unsigned i = 0; i < 3 && used < capacity - 1; ++i) {
        // Published rows are immutable after the release store.
        const unsigned state = __atomic_load_n(&gLmiproDisplayCallers[i].state, __ATOMIC_ACQUIRE);
        if (state < 2) continue;
        const LmiproDisplayCaller &r = gLmiproDisplayCallers[i];
        n = snprintf(out + used, capacity - used, "sel%u p%d t%llu st%x state%u e%d f%x n%u:",
            r.selector, r.pid, (unsigned long long)r.tid, r.status, state, r.error, r.flags, r.count);
        if (n < 0) break;
        used += (unsigned)n < capacity - used ? (unsigned)n : capacity - used - 1;
        for (unsigned j = 0; j < r.count && used < capacity - 1; ++j) {
            n = snprintf(out + used, capacity - used, " %llx", (unsigned long long)r.frames[j]);
            if (n < 0) break;
            used += (unsigned)n < capacity - used ? (unsigned)n : capacity - used - 1;
        }
        if (used < capacity - 1) { out[used++] = '\n'; out[used] = 0; }
    }
    return used;
}
