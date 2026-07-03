/**
 * @file tests/flux_gen/ufi/flux_gen.c
 * @brief Implementation of the synthetic UFI floppy-disk generator.
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

uft_ufi_gen_err_t uft_ufi_gen_disk(uint64_t seed, int bad_lba,
                                   uft_ufi_disk_t *out) {
    if (!out) return UFT_UFI_GEN_ERR_NULL;
    memset(out, 0, sizeof(*out));

    out->total_lba         = UFT_UFI_GEN_TOTAL_LBA;
    out->block_size        = UFT_UFI_GEN_BPS;
    out->cylinders         = UFT_UFI_GEN_CYLINDERS;
    out->heads             = UFT_UFI_GEN_HEADS;
    out->sectors_per_track = UFT_UFI_GEN_SPT;
    out->disk_present      = true;

    size_t bytes = (size_t)out->total_lba * out->block_size;
    out->data    = (uint8_t *)malloc(bytes);
    out->bad_lba = (uint8_t *)calloc(out->total_lba, 1);
    if (!out->data || !out->bad_lba) {
        free(out->data); free(out->bad_lba);
        return UFT_UFI_GEN_ERR_NOMEM;
    }

    uint64_t rng = seed ? seed : 0x0FF1CEULL;
    for (size_t i = 0; i < bytes; i++)
        out->data[i] = (uint8_t)(rng_next(&rng) & 0xFF);

    if (bad_lba >= 0 && (uint32_t)bad_lba < out->total_lba)
        out->bad_lba[bad_lba] = 1;

    /* Device identity (INQUIRY strings, fixed-width, space-padded — the
     * HAL copies exactly 8/16/4 bytes). */
    memcpy(out->vendor,   "UFTEMU  ", 8);  out->vendor[8]   = '\0';
    memcpy(out->product,  "USB FLOPPY DRIVE", 16); out->product[16] = '\0';
    memcpy(out->revision, "1.0 ", 4);      out->revision[4] = '\0';

    return UFT_UFI_GEN_OK;
}

void uft_ufi_gen_free(uft_ufi_disk_t *d) {
    if (!d) return;
    free(d->data);
    free(d->bad_lba);
    d->data = NULL; d->bad_lba = NULL;
}

bool uft_ufi_gen_lba_ok(const uft_ufi_disk_t *d, uint32_t lba) {
    if (!d || lba >= d->total_lba) return false;
    return d->bad_lba[lba] == 0;
}
