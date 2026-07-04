/**
 * @file test_d82_plugin.c
 * @brief Commodore 8250 (.D82) raw-sector format — real end-to-end tests.
 *
 * Links the REAL module (src/formats/commodore/d82.c) and drives its
 * production open/read/write/close FloppyDevice API against a synthesized
 * image.
 *
 * D82: 77 tracks, 2 heads, 256-byte sectors, 2 * 2083 sectors total,
 * sectors 0-based. LBA = head*per_side + track_offset(t) + s, where
 * per_side = 2083. The per-track table is not replicated; correctness is
 * proven via the table-independent invariants track_offset(1)==0 (head 0
 * track 1 sector s -> LBA s) and the head-1 base (head 1 track 1 sector 0
 * -> LBA per_side, derivable from the file size).
 */

#include "uft/floppy/uft_floppy_device.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── production API from src/formats/commodore/d82.c ────────────────── */
int uft_cbm_d82_open(FloppyDevice *dev, const char *path);
int uft_cbm_d82_close(FloppyDevice *dev);
int uft_cbm_d82_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_cbm_d82_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D82_BPS        256u
#define D82_PER_SIDE   2083u
#define D82_SECTORS    (2u * D82_PER_SIDE)
#define D82_SIZE       (D82_SECTORS * D82_BPS)   /* 1066496 */
#define TMP_IMG        "uft_d82_plugin_tmp.img"

static void fill_block(uint8_t *b, uint32_t lba) {
    b[0] = (uint8_t)(lba & 0xFF);
    b[1] = (uint8_t)((lba >> 8) & 0xFF);
    b[2] = (uint8_t)((lba >> 16) & 0xFF);
    b[3] = (uint8_t)((lba >> 24) & 0xFF);
    for (uint32_t i = 4; i < D82_BPS; i++)
        b[i] = (uint8_t)((lba * 131u + i * 17u) & 0xFF);
}

static uint32_t decode_lba(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int synth_image(uint32_t total_bytes) {
    FILE *fp = fopen(TMP_IMG, "wb");
    if (!fp) return 0;
    uint8_t blk[D82_BPS];
    uint32_t written = 0, lba = 0;
    while (written + D82_BPS <= total_bytes) {
        fill_block(blk, lba++);
        if (fwrite(blk, 1, D82_BPS, fp) != D82_BPS) { fclose(fp); return 0; }
        written += D82_BPS;
    }
    while (written < total_bytes) { fputc(0, fp); written++; }
    fclose(fp);
    return 1;
}

/* ── A. open + geometry ────────────────────────────────────────────── */

TEST(open_infers_two_heads) {
    ASSERT(synth_image(D82_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_OK);
    ASSERT(dev.tracks == 77);
    ASSERT(dev.heads == 2);
    ASSERT(dev.sectorSize == D82_BPS);
    uft_cbm_d82_close(&dev);
    remove(TMP_IMG);
}

/* ── B. head 0 track 1 sector s -> LBA s ───────────────────────────── */

TEST(head0_track1_maps_lba_directly) {
    ASSERT(synth_image(D82_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_OK);
    for (uint32_t s = 0; s < 5; s++) {
        uint8_t got[D82_BPS], want[D82_BPS];
        ASSERT(uft_cbm_d82_read_sector(&dev, 1, 0, s, got) == UFT_OK);
        ASSERT(decode_lba(got) == s);
        fill_block(want, s);
        ASSERT(memcmp(got, want, D82_BPS) == 0);
    }
    uft_cbm_d82_close(&dev);
    remove(TMP_IMG);
}

/* ── C. head 1 is offset by per_side (the defining 8250 behaviour) ──── */

TEST(head1_is_offset_by_per_side) {
    ASSERT(synth_image(D82_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_OK);

    uint8_t got[D82_BPS], want[D82_BPS];
    /* head 1, track 1, sector 0 -> LBA == per_side */
    ASSERT(uft_cbm_d82_read_sector(&dev, 1, 1, 0, got) == UFT_OK);
    ASSERT(decode_lba(got) == D82_PER_SIDE);
    fill_block(want, D82_PER_SIDE);
    ASSERT(memcmp(got, want, D82_BPS) == 0);

    /* head 1 sector s -> LBA per_side + s */
    ASSERT(uft_cbm_d82_read_sector(&dev, 1, 1, 3, got) == UFT_OK);
    ASSERT(decode_lba(got) == D82_PER_SIDE + 3u);

    /* the two heads at the same (track,sector) address different blocks */
    uint8_t h0[D82_BPS];
    ASSERT(uft_cbm_d82_read_sector(&dev, 1, 0, 0, h0) == UFT_OK);
    ASSERT(decode_lba(h0) == 0);
    ASSERT(memcmp(h0, got, D82_BPS) != 0);
    uft_cbm_d82_close(&dev);
    remove(TMP_IMG);
}

/* ── D. write round-trip on head 1 ─────────────────────────────────── */

TEST(write_read_roundtrip_head1) {
    ASSERT(synth_image(D82_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t wbuf[D82_BPS];
    for (uint32_t i = 0; i < D82_BPS; i++) wbuf[i] = (uint8_t)(0x33 ^ i);
    ASSERT(uft_cbm_d82_write_sector(&dev, 5, 1, 2, wbuf) == UFT_OK);
    uft_cbm_d82_close(&dev);

    memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t rbuf[D82_BPS];
    ASSERT(uft_cbm_d82_read_sector(&dev, 5, 1, 2, rbuf) == UFT_OK);
    ASSERT(memcmp(rbuf, wbuf, D82_BPS) == 0);
    uft_cbm_d82_close(&dev);
    remove(TMP_IMG);
}

/* ── E. bounds + error paths ───────────────────────────────────────── */

TEST(bounds_rejected) {
    ASSERT(synth_image(D82_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t buf[D82_BPS];
    ASSERT(uft_cbm_d82_read_sector(&dev, 0, 0, 0, buf) == UFT_EBOUNDS);   /* track < 1 */
    ASSERT(uft_cbm_d82_read_sector(&dev, 78, 0, 0, buf) == UFT_EBOUNDS);  /* track > 77 */
    ASSERT(uft_cbm_d82_read_sector(&dev, 1, 2, 0, buf) == UFT_EBOUNDS);   /* head > 1 */
    ASSERT(uft_cbm_d82_read_sector(&dev, 1, 0, 99, buf) == UFT_EBOUNDS);  /* sector >= spt */
    uft_cbm_d82_close(&dev);
    remove(TMP_IMG);
}

TEST(open_bad_size_and_null) {
    ASSERT(synth_image(54321u));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d82_open(&dev, TMP_IMG) == UFT_EINVAL);
    remove(TMP_IMG);
    ASSERT(uft_cbm_d82_open(NULL, TMP_IMG) == UFT_EINVAL);
    ASSERT(uft_cbm_d82_open(&dev, NULL) == UFT_EINVAL);
    ASSERT(uft_cbm_d82_open(&dev, "definitely_absent_d82_9b1e.img") == UFT_ENOENT);
}

int main(void) {
    printf("=== Commodore 8250 (.D82) plugin — real open/read/write tests ===\n");
    RUN(open_infers_two_heads);
    RUN(head0_track1_maps_lba_directly);
    RUN(head1_is_offset_by_per_side);
    RUN(write_read_roundtrip_head1);
    RUN(bounds_rejected);
    RUN(open_bad_size_and_null);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
