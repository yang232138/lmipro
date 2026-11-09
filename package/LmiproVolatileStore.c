/* RamBlock optional persistence ABI for the pure-RAM lmipro build.
 * Darwin static kernel linking still requires weak_import references to resolve.
 * Mode zero leaves NativeRamBlock::attachStore disabled. These are refusal
 * functions, not a storage backend: even an unexpected direct call must fail.
 */
#include <stdint.h>

unsigned native_ufs_store_mode(void) { return 0; }

int native_ufs_store_open(uint64_t blocks, uint64_t *first, const char **why)
{
    (void)blocks;
    if (first) *first = 0;
    if (why) *why = "lmipro volatile Data: persistence disabled";
    return 0;
}

int native_ufs_store_read(uint64_t block, uint32_t count, void *dst)
{
    (void)block; (void)count; (void)dst;
    return 0;
}

int native_ufs_store_write(uint64_t block, uint32_t count, const void *src)
{
    (void)block; (void)count; (void)src;
    return 0;
}

int native_ufs_store_sync(void) { return 0; }
unsigned native_ufs_store_gpt_check(void) { return 0xff; /* not checked */ }
