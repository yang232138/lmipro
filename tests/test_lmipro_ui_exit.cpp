#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static volatile unsigned gLmiproUIEnabled=0;
static const char *const gLmiproUINames[2]={"SpringBoard","backboardd"};
#include "../package/LmiproUIExit.h"
int main(){
 lmipro_ui_exit_event(58,"backboardd",0x1e00,0,0);assert(!gLmiproUIExitCounts[1]);
 gLmiproUIEnabled=1;
 lmipro_ui_exit_event(0,"backboardd",9,1,2);
 lmipro_ui_exit_event(58,nullptr,9,1,2);
 lmipro_ui_exit_event(58,"backboarddOther",9,1,2);assert(!gLmiproUIExitCounts[1]);
 char actor[17]="backboardd";
 lmipro_ui_exit_event(58,actor,0x1e00,0,0);actor[0]='X';
 assert(gLmiproUIExitCounts[1]==1 && gLmiproUIExitFirst[1].pid==58);
 lmipro_ui_exit_event(235,"backboardd",9,5,UINT64_C(0xfedcba9876543210));
 assert(gLmiproUIExitCounts[1]==2 && gLmiproUIExitFirst[1].raw==0x1e00);
 assert(gLmiproUIExitLast[1].pid==235 && gLmiproUIExitLast[1].raw==9);
 lmipro_ui_exit_event(27,"SpringBoard",0x8600,2,7);
 assert(gLmiproUIExitCounts[0]==1 && gLmiproUIExitFirst[0].pid==27);
 char out[576];unsigned n=lmipro_ui_exit_report(out,sizeof(out));assert(n<sizeof(out));
 assert(strstr(out,"backboardd first count=2 pid=58 raw=1e00 sig=0 exit=30 ns=0 code=0"));
 assert(strstr(out,"backboardd last count=2 pid=235 raw=9 sig=9 exit=0 ns=5 code=fedcba9876543210"));
 assert(strstr(out,"SpringBoard first count=1 pid=27 raw=8600 sig=0 exit=134 ns=2 code=7"));
 char tiny[4]={'X','X','X','X'};lmipro_ui_exit_report(tiny,sizeof(tiny));assert(!tiny[3]);
 gLmiproUIExitBusy=1;lmipro_ui_exit_event(28,"SpringBoard",1,2,3);
 assert(gLmiproUIExitDropped==1 && gLmiproUIExitCounts[0]==1);
 lmipro_ui_exit_report(out,sizeof(out));assert(strstr(out,"snapshot busy"));
 lmipro_ui_exit_report(nullptr,0);gLmiproUIExitBusy=0;
 puts("PASS: exact-name/default-off committed scalar exits; first/last across PIDs; raw status/reason preserved; no pointer retention; bounded nonwaiting contention/truncation");
}
