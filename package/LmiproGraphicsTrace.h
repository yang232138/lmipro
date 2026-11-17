// Read-only evidence from actual IOUserClient dispatch; no fabricated replies.
#pragma once
#include <stdint.h>
struct LmiproGraphicsRow {
    uint64_t entered,returned,success,opens,openErrors,selector0Entered,selector0Returned,selector0Success;
    uint32_t lastSelector,lastStatus,firstSelector,firstStatus,inScalars,inBytes;
    int firstPid;
    int lastPid;
    char lastName[17],lastClass[32];
    uint64_t closes;
    unsigned rootReplies,rootBytes,rootVersion,rootInline;
};
static LmiproGraphicsRow lmiproGraphicsRows[6]={};
static volatile unsigned lmiproGraphicsBusy=0;
static volatile uint64_t lmiproGraphicsDropped=0;
// family 0 IOSurface, 1 GPU; process 0 backboardd, 1 SpringBoard, 2 others.
static void lmiproGraphicsEvent(unsigned family,unsigned process,unsigned phase,
    uint32_t selector,uint32_t status,int pid,uint32_t scalars,uint32_t bytes) {
    if(family>1 || process>2 || phase>4)return;
    if(!__sync_bool_compare_and_swap(&lmiproGraphicsBusy,0,1)) {
        __atomic_fetch_add(&lmiproGraphicsDropped,1,__ATOMIC_RELAXED);return;
    }
    auto &r=lmiproGraphicsRows[family*3+process];
    if(phase==0) {++r.entered;if(!family && !selector)++r.selector0Entered;}
    else if(phase==1) {
        ++r.returned;if(!status)++r.success;
        if(!family && !selector){++r.selector0Returned;if(!status)++r.selector0Success;}
        r.lastSelector=selector;r.lastStatus=status;
    } else if(phase==4)++r.closes;
    else {++r.opens;if(status)++r.openErrors;}
    if(status && !r.firstStatus) {
        r.firstStatus=status;r.firstSelector=selector;r.firstPid=pid;
        r.inScalars=scalars;r.inBytes=bytes;
    }
    __sync_lock_release(&lmiproGraphicsBusy);
}
// Copy only bounded identity strings and an already validated in-band reply.
// The caller excludes descriptors/user buffers; malformed replies stay explicit.
static void lmiproGraphicsReply(unsigned family,unsigned process,int pid,const char *name,
    const char *className,unsigned selector,unsigned status,unsigned bytes,
    const uint8_t *inlineRoot) {
    if(family>1 || process>2)return;
    if(!__sync_bool_compare_and_swap(&lmiproGraphicsBusy,0,1)) {
        __atomic_fetch_add(&lmiproGraphicsDropped,1,__ATOMIC_RELAXED);return;
    }
    auto &r=lmiproGraphicsRows[family*3+process];r.lastPid=pid;
    unsigned i=0;for(;i<16 && name && name[i];++i)r.lastName[i]=name[i];r.lastName[i]=0;
    i=0;for(;i<31 && className && className[i];++i)r.lastClass[i]=className[i];r.lastClass[i]=0;
    if(!family && selector==13 && !status) {
        ++r.rootReplies;r.rootBytes=bytes;r.rootInline=inlineRoot && bytes==40;
        r.rootVersion=r.rootInline?unsigned(inlineRoot[34])|(unsigned(inlineRoot[35])<<8):0;
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
    int n=snprintf(out,capacity,"Gfx/v2 S=surface G=GPU 0=bb 1=SB 2=other drop=%llu\n",
        (unsigned long long)__atomic_load_n(&lmiproGraphicsDropped,__ATOMIC_RELAXED));
    if(n<0)return 0;
    unsigned used=(unsigned)n<capacity?(unsigned)n:capacity-1;
    for(unsigned i=0;i<6 && used<capacity-1;++i) {
        const auto &r=rows[i];
        n=snprintf(out+used,capacity-used,"%c%u o%llu/%llu c%llu m%llu/%llu ok%llu s0=%llu/%llu/%llu last%u/%x\n",
          i<3?'S':'G',i%3,(unsigned long long)r.opens,(unsigned long long)r.openErrors,
          (unsigned long long)r.closes,
          (unsigned long long)r.entered,(unsigned long long)r.returned,(unsigned long long)r.success,
          (unsigned long long)r.selector0Entered,(unsigned long long)r.selector0Returned,(unsigned long long)r.selector0Success,
          r.lastSelector,r.lastStatus);
        if(n<0)break;
        used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
        if((r.returned || r.firstStatus) && used<capacity-1) {
            n=snprintf(out+used,capacity-used," p%d %s %s first%u/%x p%d in%u/%u root%u bytes%u inline%u ver%u\n",
              r.lastPid,r.lastName,r.lastClass,r.firstSelector,r.firstStatus,r.firstPid,r.inScalars,r.inBytes,
              r.rootReplies,r.rootBytes,r.rootInline,r.rootVersion);
            if(n<0)break;
            used+=(unsigned)n<capacity-used?(unsigned)n:capacity-used-1;
        }
    }
    return used;
}
