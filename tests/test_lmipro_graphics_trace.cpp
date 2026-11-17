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
    assert(strstr(out,"S0 o0/0 m1/3 ok1 s0=0/0/0 last8/e00002bc first6/e00002c7 p58 in1/80"));
    assert(strstr(out,"G1 o1/1"));
    lmiproGraphicsEvent(0,1,0,0,0,27,1,128);
    lmiproGraphicsEvent(0,1,1,0,0,27,1,128);
    assert(lmiproGraphicsRows[1].selector0Entered==1 && lmiproGraphicsRows[1].selector0Returned==1 && lmiproGraphicsRows[1].selector0Success==1);
    char tiny[8];memset(tiny,0x5a,sizeof(tiny));lmiproFormatGraphics(tiny+1,4);
    assert(tiny[0]==0x5a && tiny[4]==0 && tiny[5]==0x5a);
    assert(!lmiproFormatGraphics(nullptr,0));
    puts("PASS: actual replies; first error retained; processes/families separated; contention counted; bounded reports");
}
