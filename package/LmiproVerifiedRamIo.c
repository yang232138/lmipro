/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "LmiproVerifiedRamIo.h"

int lmipro_verified_ram_read(const struct vr_view *view, const struct vr_crypto *crypto,
    uint64_t offset, size_t length, const struct lr_io *io, uint8_t scratch[VR_BLOCK_BYTES],
    uint64_t *actual, uint64_t *verified_blocks)
{
    if (actual) *actual = 0;
    if (verified_blocks) *verified_blocks = 0;
    if (!actual || !verified_blocks || !view || !view->hashes || !crypto || !scratch ||
        !io || !io->load || !io->emit || !length || offset >= view->image_bytes ||
        length > view->image_bytes - offset) return LR_ARGUMENT;
    while (*actual < length) {
        uint64_t position = offset + *actual;
        uint64_t base = position - position % VR_BLOCK_BYTES;
        uint64_t remaining = view->image_bytes - base;
        size_t block_length = remaining < VR_BLOCK_BYTES ? (size_t)remaining : VR_BLOCK_BYTES;
        size_t skip = (size_t)(position - base);
        size_t amount = block_length - skip;
        if (amount > length - *actual) amount = (size_t)(length - *actual);
        if (!io->load(io->context, base, scratch, block_length)) return LR_SOURCE;
        if (vr_verify_block(view, base / VR_BLOCK_BYTES, scratch, block_length, crypto) != VR_OK)
            return LR_INTEGRITY;
        ++*verified_blocks;
        size_t copied = io->emit(io->context, *actual, scratch + skip, amount);
        if (copied > amount) return LR_OUTPUT;
        *actual += copied;
        if (copied != amount) return LR_OUTPUT;
    }
    return LR_OK;
}
