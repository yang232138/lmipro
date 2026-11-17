// Read-only evidence from actual IOUserClient dispatch; no fabricated replies.
#pragma once
#include <stdint.h>
#include <stdio.h>
struct LmiproGraphicsRow {
    uint64_t entered,returned,success,opens,openErrors;
    uint32_t lastSelector,lastStatus,firstSelector,firstStatus,inScalars,inBytes;
    int firstPid;
};
static LmiproGraphicsRow lmiproGraphicsRows[6]={};
static volatile unsigned lmiproGraphicsBusy=0;
static volatile uint64_t lmiproGraphicsDropped=0;
// family 0 IOSurface, 1 GPU; process 0 backboardd, 1 SpringBoard, 2 others.
static void lmiproGraphicsEvent(unsigned family,unsigned process,unsigned phase,
    uint32_t selector,uint32_t status,int pid,uint32_t scalars,uint32_t bytes) {
    if(family>1 || process>2 || phase>3)return;
    if(!__sync_bool_compare_and_swap(&lmiproGraphicsBusy,0,1)) {
        __atomic_fetch_add(&lmiproGraphicsDropped,1,__ATOMIC_RELAXED);return;
    }
    auto &r=lmiproGraphicsRows[family*3+process];
    if(phase==0)++r.entered;
    else if(phase==1) {
        ++r.returned;if(!status)++r.success;
        r.lastSelector=selector;r.lastStatus=status;
    } else {++r.opens;if(status)++r.openErrors;}
    if(status && !r.firstStatus) {
        r.firstStatus=status;r.firstSelector=selector;r.firstPid=pid;
        r.inScalars=scalars;r.inBytes=bytes;
    }
    __sync_lock_release(&lmiproGraphicsBusy);
}
static unsigned lmiproFormatGraphics(char *out,unsigned capacity) {
    if(!out || !capacity)return 0;
    LmiproGraphicsRow rows[6];
    if(!__sync_bool_compare_and_swap(&lmiproGraphicsBusy,0,1))
        return snprintf(out,capacity,"Gfx/v1 snapshot unavailable\n");
    for(unsigned i=0;i<6;++i)rows[i]=lmiproGraphicsRows[i];
    __sync_lock_release(&lmiproGraphicsBusy);
    int n=snprintf(out,capacity,"Gfx/v1 bb/SB/other dropped=%llu; open errors use sel=ffffffff\n",
        (unsigned long long)__atomic_load_n(&lmiproGraphicsDropped,__ATOMIC_RELAXED));
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<6 && used<capacity-1;++i) {
        const auto &r=rows[i];
        n=snprintf(out+used,capacity-used,"%c%u o%llu/%llu m%llu/%llu ok%llu last%u/%x first%u/%x p%d in%u/%u\n",
          i<3?'S':'G',i%3,(unsigned long long)r.opens,(unsigned long long)r.openErrors,
          (unsigned long long)r.entered,(unsigned long long)r.returned,(unsigned long long)r.success,
          r.lastSelector,r.lastStatus,r.firstSelector,r.firstStatus,r.firstPid,r.inScalars,r.inBytes);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
    }
    return used;
}
