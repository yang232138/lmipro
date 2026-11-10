// Board handoff observed in 1794289338194.jpg; reserved span from lmi MemoryMapLib.
#pragma once
#include <stdint.h>
static inline bool lmiproDisplayHandoff(uint64_t base,uint64_t width,uint64_t height,
    uint64_t depth,uint64_t stride,uint64_t offset,unsigned rotation,uint64_t *bytes)
{
    *bytes=0;
    if(base!=0x9c000000ULL || width!=1080 || height!=2400 || depth!=32 ||
       stride!=4320 || offset || rotation || stride>UINT64_MAX/height)return false;
    uint64_t span=stride*height;
    if(span>0x2400000ULL || base>UINT64_MAX-span)return false;
    *bytes=span;return true;
}
