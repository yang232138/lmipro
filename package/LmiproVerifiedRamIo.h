/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef LMIPRO_VERIFIED_RAM_IO_H
#define LMIPRO_VERIFIED_RAM_IO_H
#include "verified_root.h"
#ifdef __cplusplus
extern "C" {
#endif
enum lr_status { LR_OK, LR_ARGUMENT, LR_SOURCE, LR_INTEGRITY, LR_OUTPUT };
struct lr_io {
    void *context;
    int (*load)(void *, uint64_t, void *, size_t);
    size_t (*emit)(void *, uint64_t, const void *, size_t);
};
/* Authenticate a full signed block BEFORE publishing even a partial read.
 * Caller serializes its immutable root backing and owns the 4096-byte scratch.
 * On failure only the reported, already verified prefix may have been copied.
 */
int lmipro_verified_ram_read(const struct vr_view *, const struct vr_crypto *,
    uint64_t, size_t, const struct lr_io *, uint8_t[VR_BLOCK_BYTES], uint64_t *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
