/**
 * @file test_d80_plugin.c
 * @brief Commodore 8050 (.D80) raw-sector format — real end-to-end tests.
 *
 * Links the REAL module (src/formats/commodore/d80.c) and drives its
 * production open/read/write/close FloppyDevice API against a synthesized
 * image — tests the production code, not a mirror.
 *
 * D80: 77 tracks, 1 logical head, 256-byte sectors, 2083 sectors total
 * (variable per-track GCR zones), sectors 0-based. LBA = track_offset(t)+s.
 *
 * The per-track sector table is NOT replicated here; correctness is proven
 * via the table-independent invariant track_offset(1)==0, so on track 1
 * sector s maps exactly to LBA s. Each 256-byte block on disk encodes its
 * own LBA, so a read's returned bytes reveal which block the module fetched.
 */

#include "uft/floppy/uft_floppy_device.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── production API from src/formats/commodore/d80.c ────────────────── */
int uft_cbm_d80_open(FloppyDevice *dev, const char *path);
int uft_cbm_d80_close(FloppyDevice *dev);
int uft_cbm_d80_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_cbm_d80_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D80_BPS        256u
#define D80_SECTORS    2083u          /* sum of the 8050 zone table */
#define D80_SIZE       (D80_SECTORS * D80_BPS)   /* 533248 */
#define TMP_IMG        "uft_d80_plugin_tmp.img"

/* Each 256-byte block encodes its LBA in bytes 0..3 (LE32); the rest is a
 * deterministic mix so a read is verified for both position AND content. */
static void fill_block(uint8_t *b, uint32_t lba) {
    b[0] = (uint8_t)(lba & 0xFF);
    b[1] = (uint8_t)((lba >> 8) & 0xFF);
    b[2] = (uint8_t)((lba >> 16) & 0xFF);
    b[3] = (uint8_t)((lba >> 24) & 0xFF);
    for (uint32_t i = 4; i < D80_BPS; i++)
        b[i] = (uint8_t)((lba * 131u + i * 17u) & 0xFF);
}

static uint32_t decode_lba(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int synth_image(uint32_t total_bytes) {
    FILE *fp = fopen(TMP_IMG, "wb");
    if (!fp) return 0;
    uint8_t blk[D80_BPS];
    uint32_t written = 0, lba = 0;
    while (written + D80_BPS <= total_bytes) {
        fill_block(blk, lba++);
        if (fwrite(blk, 1, D80_BPS, fp) != D80_BPS) { fclose(fp); return 0; }
        written += D80_BPS;
    }
    while (written < total_bytes) { fputc(0, fp); written++; }
    fclose(fp);
    return 1;
}

/* ── A. open + geometry ────────────────────────────────────────────── */

TEST(open_infers_geometry) {
    ASSERT(synth_image(D80_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_OK);
    ASSERT(dev.tracks == 77);
    ASSERT(dev.heads == 1);
    ASSERT(dev.sectorSize == D80_BPS);
    ASSERT(dev.flux_supported == false);
    uft_cbm_d80_close(&dev);
    remove(TMP_IMG);
}

/* ── B. read maps track 1 sector s -> LBA s, byte-exact ─────────────── */

TEST(read_track1_maps_lba_directly) {
    ASSERT(synth_image(D80_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_OK);

    for (uint32_t s = 0; s < 5; s++) {
        uint8_t got[D80_BPS], want[D80_BPS];
        ASSERT(uft_cbm_d80_read_sector(&dev, 1, 0, s, got) == UFT_OK);
        ASSERT(decode_lba(got) == s);          /* track_offset(1)==0 */
        fill_block(want, s);
        ASSERT(memcmp(got, want, D80_BPS) == 0);
    }
    uft_cbm_d80_close(&dev);
    remove(TMP_IMG);
}

/* ── C. later tracks read monotonically increasing, valid blocks ────── */

TEST(read_later_tracks_are_valid_blocks) {
    ASSERT(synth_image(D80_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_OK);

    uint8_t got[D80_BPS], want[D80_BPS];
    uint32_t prev = 0;
    /* track 2 sector 0 must be past track 1 (LBA >= spt[0]) and its block
     * must be self-consistent; each subsequent track strictly increases. */
    for (uint32_t t = 2; t <= 77; t += 15) {
        ASSERT(uft_cbm_d80_read_sector(&dev, t, 0, 0, got) == UFT_OK);
        uint32_t lba = decode_lba(got);
        ASSERT(lba > prev);
        ASSERT(lba < D80_SECTORS);
        fill_block(want, lba);
        ASSERT(memcmp(got, want, D80_BPS) == 0);   /* real block, not garbage */
        prev = lba;
    }
    uft_cbm_d80_close(&dev);
    remove(TMP_IMG);
}

/* ── D. write round-trip ───────────────────────────────────────────── */

TEST(write_read_roundtrip) {
    ASSERT(synth_image(D80_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_OK);

    uint8_t wbuf[D80_BPS];
    for (uint32_t i = 0; i < D80_BPS; i++) wbuf[i] = (uint8_t)(0x5A ^ i);
    ASSERT(uft_cbm_d80_write_sector(&dev, 1, 0, 3, wbuf) == UFT_OK);
    uft_cbm_d80_close(&dev);

    memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t rbuf[D80_BPS];
    ASSERT(uft_cbm_d80_read_sector(&dev, 1, 0, 3, rbuf) == UFT_OK);
    ASSERT(memcmp(rbuf, wbuf, D80_BPS) == 0);
    uft_cbm_d80_close(&dev);
    remove(TMP_IMG);
}

/* ── E. bounds + error paths ───────────────────────────────────────── */

TEST(bounds_rejected) {
    ASSERT(synth_image(D80_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t buf[D80_BPS];
    ASSERT(uft_cbm_d80_read_sector(&dev, 0, 0, 0, buf) == UFT_EBOUNDS);   /* track < 1 */
    ASSERT(uft_cbm_d80_read_sector(&dev, 78, 0, 0, buf) == UFT_EBOUNDS);  /* track > 77 */
    ASSERT(uft_cbm_d80_read_sector(&dev, 1, 1, 0, buf) == UFT_EBOUNDS);   /* head != 0 */
    ASSERT(uft_cbm_d80_read_sector(&dev, 1, 0, 99, buf) == UFT_EBOUNDS);  /* sector >= spt */
    uft_cbm_d80_close(&dev);
    remove(TMP_IMG);
}

TEST(open_bad_size_and_null) {
    ASSERT(synth_image(12345u));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d80_open(&dev, TMP_IMG) == UFT_EINVAL);
    remove(TMP_IMG);
    ASSERT(uft_cbm_d80_open(NULL, TMP_IMG) == UFT_EINVAL);
    ASSERT(uft_cbm_d80_open(&dev, NULL) == UFT_EINVAL);
    ASSERT(uft_cbm_d80_open(&dev, "definitely_absent_d80_7c3f.img") == UFT_ENOENT);
}

int main(void) {
    printf("=== Commodore 8050 (.D80) plugin — real open/read/write tests ===\n");
    RUN(open_infers_geometry);
    RUN(read_track1_maps_lba_directly);
    RUN(read_later_tracks_are_valid_blocks);
    RUN(write_read_roundtrip);
    RUN(bounds_rejected);
    RUN(open_bad_size_and_null);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
