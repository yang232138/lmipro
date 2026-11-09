#include <assert.h>
#include <stdint.h>
#include <string.h>
unsigned native_ufs_store_mode(void);
int native_ufs_store_open(uint64_t, uint64_t *, const char **);
int native_ufs_store_read(uint64_t, uint32_t, void *);
int native_ufs_store_write(uint64_t, uint32_t, const void *);
int native_ufs_store_sync(void);
unsigned native_ufs_store_gpt_check(void);
int main(void)
{
    unsigned char buffer[4096], original[4096];
    memset(buffer, 0xa5, sizeof buffer);
    memcpy(original, buffer, sizeof buffer);
    uint64_t first = UINT64_MAX;
    const char *why = 0;
    assert(native_ufs_store_mode() == 0);
    assert(!native_ufs_store_open(UINT64_MAX, &first, &why));
    assert(first == 0 && why && strstr(why, "disabled"));
    assert(!native_ufs_store_open(0, 0, 0));
    assert(!native_ufs_store_read(0, 1, buffer));
    assert(!native_ufs_store_write(UINT64_MAX, UINT32_MAX, buffer));
    assert(!native_ufs_store_read(UINT64_MAX, UINT32_MAX, 0));
    assert(!native_ufs_store_write(0, 0, 0));
    assert(!memcmp(buffer, original, sizeof buffer));
    assert(!native_ufs_store_sync());
    assert(native_ufs_store_gpt_check() == 0xff);
    return 0;
}
