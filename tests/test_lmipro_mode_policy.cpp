#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../package/LmiproModePolicy.h"
int main(){
  for(unsigned scenario=0;scenario<6;++scenario){
    int state[2]={0,0};unsigned value[2]={0,0},writes=0,erases=0;
    if(scenario==1){state[0]=state[1]=1;value[0]=1;}
    if(scenario==2){state[0]=1;value[0]=2;}
    if(scenario==3)state[1]=-1;
    auto index=[](const char *k){return strcmp(k,"display-scale")?1:0;};
    auto read=[&](const char *k,uint32_t *out){int i=index(k);*out=value[i];return state[i];};
    auto write=[&](const char *k,unsigned v){int i=index(k);++writes;if(scenario==4&&i==1)return false;state[i]=1;value[i]=(scenario==5&&i==1)?3:v;return true;};
    auto erase=[&](const char *k){state[index(k)]=0;++erases;};
    LmiproModePolicy p;bool ok=lmiproModeInstall(p,1,0,read,write,erase);
    assert(ok==(scenario<2));
    if(scenario==0)assert(p.stage==2&&p.scaleAdded&&p.rotationAdded&&value[0]==1&&value[1]==0);
    if(scenario==1)assert(!writes&&!erases&&!p.scaleAdded&&!p.rotationAdded);
    if(scenario==2)assert(!writes&&!erases&&value[0]==2&&p.status==-2);
    if(scenario>=3)assert(!p.scaleAdded&&!p.rotationAdded&&state[0]==0&&erases>=1);
  }
  LmiproModePolicy p;unsigned queries=0;
  auto read=[&](const char *,uint32_t *){++queries;return 0;};
  auto write=[](const char *,unsigned){return true;};auto erase=[](const char *){};
  assert(!lmiproModeInstall(p,2,0,read,write,erase)&&!queries);
  puts("PASS: actual unit-scale policy, existing-value preservation, malformed property refusal, partial-write rollback and readback verification");
}
