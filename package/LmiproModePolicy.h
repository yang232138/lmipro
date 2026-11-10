// Atomic ownership policy for the two optional /chosen display words.
#pragma once
#include <stdint.h>
struct LmiproModePolicy {unsigned stage=0;int status=0;bool scaleAdded=false,rotationAdded=false;};
// read: 0 absent, 1 valid uint32 OSData, -1 malformed. Never overwrite a value.
template<class Read,class Write,class Erase>
static bool lmiproModeInstall(LmiproModePolicy &p,unsigned scale,unsigned rotation,
    Read read,Write write,Erase erase)
{
    if(scale!=1 || rotation!=0 || p.stage){p.status=-1;return false;}
    p.stage=1;
    const char *keys[2]={"display-scale","display-rotation"};
    unsigned values[2]={scale,rotation};bool *added[2]={&p.scaleAdded,&p.rotationAdded};
    for(unsigned i=0;i<2;++i) {
        uint32_t actual=0;int state=read(keys[i],&actual);
        if(state<0 || (state==1 && actual!=values[i])){p.status=-2;break;}
        if(state==0) {
            if(!write(keys[i],values[i])){p.status=-3;break;}
            *added[i]=true;
        }
        if(read(keys[i],&actual)!=1 || actual!=values[i]){p.status=-4;break;}
    }
    if(p.status) {
        if(p.rotationAdded)erase(keys[1]);
        if(p.scaleAdded)erase(keys[0]);
        p.rotationAdded=p.scaleAdded=false;return false;
    }
    p.stage=2;return true;
}
