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
    lmiproFormatDisplayMethods(out,sizeof(out));assert(strstr(out,"shown=12 of 12"));
    char tiny[4]={};lmiproFormatDisplayMethods(tiny,sizeof(tiny));assert(tiny[3]==0);
    lmiproFormatDisplayMethods(nullptr,0);
    const char name[17]="1234567890123456";
    lmiproRecordDisplayMethod(0,28,0,94,0,0,0,name);
    assert(!strcmp(gLmiproDisplayLastIn.name,name));
    assert(gLmiproDisplayLastIn.name[16]==0);
    lmiproFormatDisplayMethods(out,sizeof(out));
    assert(strstr(out,"pid=94 name=1234567890123456"));
    lmiproRecordDisplayMethod(1,8,0,58,1080,2400,0,"backboardd",2);
    char full[1400];lmiproFormatDisplayMethods(full,sizeof(full));
    assert(strstr(full,"8 n=2 438/960"));
    assert(strstr(full,"28:1/0/0"));
    lmiproClientEvent(0,100,2,58,"backboardd",0);
    lmiproClientEvent(0,200,0,27,"SpringBoard",0);
    lmiproClientEvent(1,200,0,0,nullptr,0);
    lmiproClientEvent(1,100,0,0,nullptr,0xe00002c1U);
    assert(gLmiproClients[0].type==2 && gLmiproClients[0].status==0xe00002c1U && gLmiproClients[0].returned);
    assert(gLmiproClients[1].pid==27 && !gLmiproClients[1].status && gLmiproClients[1].returned);
    lmiproClientEvent(0,300,2,58,"backboardd",0);
    lmiproClientEvent(0,300,3,58,"backboardd",0);
    lmiproClientEvent(1,300,0,0,nullptr,0);
    assert(gLmiproClients[2].ambiguous && !gLmiproClients[2].returned);
    lmiproFormatClients(full,sizeof(full));assert(strstr(full,"type2 done1 amb0=e00002c1"));
    lmiproFormatClients(tiny,sizeof(tiny));assert(!tiny[3]);
    gLmiproClientBusy=1;lmiproClientEvent(0,400,0,1,nullptr,0);
    lmiproFormatClients(full,sizeof(full));assert(strstr(full,"snapshot busy"));gLmiproClientBusy=0;
    for(unsigned i=0;i<12;++i)lmiproClientEvent(0,500+i,0,1,nullptr,0);
    assert(gLmiproClientStored==8 && gLmiproClientDropped);
    for(unsigned i=40;i<58;++i)lmiproRecordDisplayMethod(0,i,0,58,0,0,0);
    lmiproFormatDisplayMethods(full,sizeof(full));assert(strstr(full,"shown=16 of"));
    puts("PASS: disabled path; unmatched entry; exact first/last error; selector bounds; nonwaiting drops; capped snapshot and termination");
}
