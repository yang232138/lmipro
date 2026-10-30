/* This board test has no persistent store. Never report a successful disk I/O.
 * Signatures match RamBlock 25c57e0e9b68bf452dd1986914525edc9d2320bc.
 * XNU's static kernel link requires definitions even for weak optional imports.
 */
#include <stdint.h>

unsigned native_ufs_store_mode(void) { return 0; }
int native_ufs_store_open(uint64_t blocks, uint64_t *first, const char **why)
{
    (void)blocks;
    if (first) *first = 0;
    if (why) *why = "lmipro RAM-only test: persistent store unavailable";
    return 0;
}
int native_ufs_store_read(uint64_t block, uint32_t count, void *buffer)
{ (void)block; (void)count; (void)buffer; return 0; }
int native_ufs_store_write(uint64_t block, uint32_t count, const void *buffer)
{ (void)block; (void)count; (void)buffer; return 0; }
int native_ufs_store_sync(void) { return 0; }
unsigned native_ufs_store_gpt_check(void) { return 0; }
