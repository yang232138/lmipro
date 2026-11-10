#pragma once
#include <stdint.h>
// The original software renderer's uncompressed, nonplanar BGRA contract.
// The generic backend also accepts a compressed format for a different board's
// GPU path; that representation has not been qualified for this CPU-only port.
static inline bool lmiproCPUFormat(uint32_t format,uint32_t planes) {
    return format==0x42475241U && planes==0;
}
static inline bool lmiproKeepPresentedFrame(unsigned enabled,unsigned owned,
    uint64_t copied,uint64_t bytes,uint64_t failed,uint64_t retired,uint64_t surfaces) {
    return enabled==1 && owned==1 && copied && bytes && !failed && retired && surfaces;
}
