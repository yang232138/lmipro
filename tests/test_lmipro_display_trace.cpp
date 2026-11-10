#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../package/LmiproDisplayTrace.h"
int main()
{
    lmiproRecordDisplayMethod(0,6,0,58,1,2,3);
    assert(!gLmiproDisplayEntered);
    gLmiproDisplayTraceEnabled=1;
    lmiproRecordDisplayMethod(0,6,0,58,1,2,3);
    // A blocked call can have entry evidence with no return; never invent one.
    assert(gLmiproDisplayEntered==1 && !gLmiproDisplayReturned);
    lmiproRecordDisplayMethod(1,6,0xe00002d5U,58,0,0,0);
    lmiproRecordDisplayMethod(1,7,0xe00002c7U,58,0,0,0);
    assert(gLmiproDisplayErrorValid && gLmiproDisplayFirstError.selector==6);
    assert(gLmiproDisplayFirstError.status==0xe00002d5U);
    assert(gLmiproDisplayLastOut.selector==7);
    // Out-of-range selectors are recorded globally without indexing the table.
    lmiproRecordDisplayMethod(0,UINT32_MAX,0,61,4,5,6);
    assert(gLmiproDisplayEntered==2 && gLmiproDisplayLastIn.selector==UINT32_MAX);
    gLmiproDisplayTraceBusy=1;
    lmiproRecordDisplayMethod(0,8,0,58,7,8,9);
    assert(gLmiproDisplayTraceDropped==1 && gLmiproDisplayEntered==2);
    char out[512];lmiproFormatDisplayMethods(out,sizeof(out));
    assert(strstr(out,"snapshot busy"));
    gLmiproDisplayTraceBusy=0;
    lmiproFormatDisplayMethods(out,sizeof(out));
    assert(strstr(out,"in=2 out=2 dropped=1"));
    assert(strstr(out,"status=e00002d5"));
    for(unsigned i=10;i<20;++i)lmiproRecordDisplayMethod(0,i,0,58,i,0,0);
    lmiproFormatDisplayMethods(out,sizeof(out));assert(strstr(out,"shown=4 of 12"));
    char tiny[4]={};lmiproFormatDisplayMethods(tiny,sizeof(tiny));assert(tiny[3]==0);
    lmiproFormatDisplayMethods(nullptr,0);
    const char name[17]="1234567890123456";
    lmiproRecordDisplayMethod(0,28,0,94,0,0,0,name);
    assert(!strcmp(gLmiproDisplayLastIn.name,name));
    assert(gLmiproDisplayLastIn.name[16]==0);
    lmiproFormatDisplayMethods(out,sizeof(out));
    assert(strstr(out,"pid=94 name=1234567890123456"));
    puts("PASS: disabled path; unmatched entry; exact first/last error; selector bounds; nonwaiting drops; capped snapshot and termination");
}
