// Read existing signature counters only; never bypass or alter authentication.
extern "C" {
extern unsigned long cs_native4k_full_blocks, cs_native4k_ready_blobs;
extern unsigned long cs_validate_page_bad_hash, cs_validate_page_no_hash;
extern uint64_t cs_native4k_bytes, cs_native4k_read_ticks, cs_native4k_hash_ticks;
}
struct LmiproSignSample {
  uint64_t blocks, bytes, blobs, read_ticks, hash_ticks, bad, no_hash;
};
static LmiproSignSample lmiproReadSignSample()
{
  return {__atomic_load_n(&cs_native4k_full_blocks,__ATOMIC_RELAXED),
    __atomic_load_n(&cs_native4k_bytes,__ATOMIC_RELAXED),
    __atomic_load_n(&cs_native4k_ready_blobs,__ATOMIC_RELAXED),
    __atomic_load_n(&cs_native4k_read_ticks,__ATOMIC_RELAXED),
    __atomic_load_n(&cs_native4k_hash_ticks,__ATOMIC_RELAXED),
    __atomic_load_n(&cs_validate_page_bad_hash,__ATOMIC_RELAXED),
    __atomic_load_n(&cs_validate_page_no_hash,__ATOMIC_RELAXED)};
}
static __attribute__((noinline)) void lmiproWaitSignProgress(char *out, size_t capacity)
{
  LmiproSignSample start=lmiproReadSignSample();
  unsigned split=gLmiproDataWatchSeconds<30 ? gLmiproDataWatchSeconds : 30;
  IOSleep(split*1000U);
  LmiproSignSample middle=lmiproReadSignSample();
  if(gLmiproDataWatchSeconds>split)IOSleep((gLmiproDataWatchSeconds-split)*1000U);
  LmiproSignSample end=lmiproReadSignSample();
  snprintf(out,capacity,
    "CS start blocks=%llu bytes=%llu blobs=%llu\n"
    "CS at%u blocks=%llu bytes=%llu blobs=%llu\n"
    "CS at%u blocks=%llu bytes=%llu blobs=%llu\n"
    "CS readTicks=%llu hashTicks=%llu bad=%llu noHash=%llu\n",
    (unsigned long long)start.blocks,(unsigned long long)start.bytes,(unsigned long long)start.blobs,
    split,(unsigned long long)middle.blocks,(unsigned long long)middle.bytes,(unsigned long long)middle.blobs,
    gLmiproDataWatchSeconds,(unsigned long long)end.blocks,(unsigned long long)end.bytes,(unsigned long long)end.blobs,
    (unsigned long long)end.read_ticks,(unsigned long long)end.hash_ticks,
    (unsigned long long)end.bad,(unsigned long long)end.no_hash);
}
