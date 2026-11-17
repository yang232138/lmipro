#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../package/LmiproGraphicsTrace.h"
int main() {
    lmiproGraphicsEvent(0,0,0,6,0,58,1,80);
    lmiproGraphicsEvent(0,0,1,6,0xe00002c7,58,1,80);
    lmiproGraphicsEvent(0,0,1,7,0,58,2,90);
    lmiproGraphicsEvent(0,0,1,8,0xe00002bc,58,3,100);
    const auto &r=lmiproGraphicsRows[0];
    assert(r.entered==1 && r.returned==3 && r.success==1);
    assert(r.firstSelector==6 && r.firstStatus==0xe00002c7 && r.inBytes==80);
    assert(r.lastSelector==8 && r.lastStatus==0xe00002bc);
    lmiproGraphicsEvent(1,1,2,UINT32_MAX,0xe00002c1,27,0,0);
    assert(lmiproGraphicsRows[4].opens==1 && lmiproGraphicsRows[4].openErrors==1);
    assert(lmiproGraphicsRows[4].firstSelector==UINT32_MAX);
    assert(!lmiproGraphicsRows[3].opens && !lmiproGraphicsRows[1].returned);
    lmiproGraphicsBusy=1;
    lmiproGraphicsEvent(0,0,0,9,0,58,0,0);
    assert(lmiproGraphicsDropped==1 && r.entered==1);
    char out[900];lmiproFormatGraphics(out,sizeof(out));assert(strstr(out,"unavailable"));
    lmiproGraphicsBusy=0;
    lmiproFormatGraphics(out,sizeof(out));
    assert(strstr(out,"S0 o0/0 c0 m1/3 ok1 s0=0/0/0 last8/e00002bc"));
    assert(strstr(out,"first6/e00002c7 p58 in1/80"));
    assert(strstr(out,"G1 o1/1"));
    lmiproGraphicsEvent(0,1,0,0,0,27,1,128);
    lmiproGraphicsEvent(0,1,1,0,0,27,1,128);
    assert(lmiproGraphicsRows[1].selector0Entered==1 && lmiproGraphicsRows[1].selector0Returned==1 && lmiproGraphicsRows[1].selector0Success==1);
    uint8_t root[40]={};root[34]=1;root[35]=2;
    lmiproGraphicsReply(0,0,58,"backboardd","IOSurfaceRootUserClient",13,0,40,root);
    assert(r.rootReplies==1 && r.rootInline==1 && r.rootVersion==513 && r.lastPid==58);
    assert(!strcmp(r.lastClass,"IOSurfaceRootUserClient"));
    assert(r.lastSelector==13 && !r.lastStatus);
    // A wrong-sized or unavailable reply must never be treated as a version.
    lmiproGraphicsReply(0,0,58,"backboardd","IOSurfaceRootUserClient",13,0,39,(const uint8_t *)1);
    assert(r.rootReplies==2 && !r.rootInline && !r.rootVersion);
    lmiproGraphicsReply(0,0,58,"backboardd","IOSurfaceRootUserClient",13,0,40,nullptr);
    assert(r.rootReplies==3 && !r.rootInline);
    lmiproGraphicsReply(0,0,58,"backboardd","IOSurfaceRootUserClient",13,0xe00002c7,40,root);
    assert(r.rootReplies==3);
    lmiproGraphicsReply(0,2,81,"other process name longer than limit","IOSurfaceRootUserClientLongerThanThirtyOneBytes",0,0,1,nullptr);
    assert(lmiproGraphicsRows[2].lastName[16]==0 && lmiproGraphicsRows[2].lastClass[31]==0);
    lmiproGraphicsEvent(0,0,4,UINT32_MAX,0,58,0,0);assert(r.closes==1 && !r.opens);
    char tiny[8];memset(tiny,0x5a,sizeof(tiny));lmiproFormatGraphics(tiny+1,4);
    assert(tiny[0]==0x5a && tiny[4]==0 && tiny[5]==0x5a);
    assert(!lmiproFormatGraphics(nullptr,0));
    puts("PASS: actual replies; first error retained; processes/families separated; contention counted; bounded reports");
}
