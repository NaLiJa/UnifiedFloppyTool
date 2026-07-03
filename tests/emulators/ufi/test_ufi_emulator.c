/**
 * @file tests/emulators/ufi/test_ufi_emulator.c
 * @brief Firmware-realistic tests for the UFI emulator (9/9 — the finale).
 *
 * UFI's C HAL (src/hal/ufi.c) exposes an injectable backend vtable, so
 * this emulator drives the PRODUCTION HAL functions end-to-end against a
 * mock backend that answers SCSI CDBs from a synthetic disk. Every HAL
 * call — uft_ufi_inquiry, uft_ufi_test_unit_ready, uft_ufi_read_capacity,
 * uft_ufi_read_sectors, uft_ufi_request_sense — is a live conformance
 * test of the HAL's CDB construction + response parsing.
 *
 * Six groups:
 *   A. backend install / not-set guard
 *   B. INQUIRY identity round-trip
 *   C. TEST_UNIT_READY + READ_CAPACITY
 *   D. READ(10) sector-data round-trip (production HAL)
 *   E. forensic: medium-error sense, no fabricated data
 *   F. no-disk sense, write path, determinism
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"
#include "uft/hal/ufi.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0;
#define ASSERT(cond) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
} while (0)
#define TEST(name) printf("  [TEST] %-58s ... ", name)
#define DONE()     printf("OK\n")

static const char *PATH = "mock:0";

/* ─── A. backend install ────────────────────────────────────────────── */

static void group_a(void) {
    printf("\n-- A. backend install / not-set guard --\n");
    uft_diag_t diag;

    TEST("no_backend_returns_not_implemented");
    ufi_mock_install(NULL);   /* clears backend */
    ASSERT(uft_ufi_test_unit_ready(PATH, &diag) == UFT_ERR_NOT_IMPLEMENTED);
    DONE();

    TEST("install_registers_backend");
    uft_ufi_disk_t disk;
    ASSERT(uft_ufi_gen_disk(1, -1, &disk) == UFT_UFI_GEN_OK);
    ufi_mock_install(&disk);
    ASSERT(uft_ufi_test_unit_ready(PATH, &diag) == UFT_OK);
    ufi_mock_install(NULL);
    uft_ufi_gen_free(&disk);
    DONE();
}

/* ─── B. INQUIRY ────────────────────────────────────────────────────── */

static void group_b(void) {
    printf("\n-- B. INQUIRY identity (production HAL parse) --\n");
    uft_diag_t diag;
    uft_ufi_disk_t disk;
    uft_ufi_gen_disk(2, -1, &disk);
    ufi_mock_install(&disk);

    TEST("inquiry_parses_vendor_product_rev");
    char vendor[9], product[17], rev[5];
    ASSERT(uft_ufi_inquiry(PATH, vendor, product, rev, &diag) == UFT_OK);
    ASSERT(strncmp(vendor, "UFTEMU", 6) == 0);
    ASSERT(strncmp(product, "USB FLOPPY DRIVE", 16) == 0);
    ASSERT(strncmp(rev, "1.0", 3) == 0);
    DONE();

    ufi_mock_install(NULL);
    uft_ufi_gen_free(&disk);
}

/* ─── C. capacity ───────────────────────────────────────────────────── */

static void group_c(void) {
    printf("\n-- C. TEST_UNIT_READY + READ_CAPACITY --\n");
    uft_diag_t diag;
    uft_ufi_disk_t disk;
    uft_ufi_gen_disk(3, -1, &disk);
    ufi_mock_install(&disk);

    TEST("test_unit_ready_ok_with_disk");
    ASSERT(uft_ufi_test_unit_ready(PATH, &diag) == UFT_OK);
    DONE();

    TEST("read_capacity_is_2880_lba_512_bytes");
    uint32_t total_lba = 0, block = 0;
    ASSERT(uft_ufi_read_capacity(PATH, &total_lba, &block, &diag) == UFT_OK);
    ASSERT(total_lba == UFT_UFI_GEN_TOTAL_LBA);   /* 2880 */
    ASSERT(block == UFT_UFI_GEN_BPS);             /* 512 */
    DONE();

    ufi_mock_install(NULL);
    uft_ufi_gen_free(&disk);
}

/* ─── D. READ(10) round-trip ────────────────────────────────────────── */

static void group_d(void) {
    printf("\n-- D. READ(10) sector-data round-trip --\n");
    uft_diag_t diag;
    uft_ufi_disk_t disk;
    uft_ufi_gen_disk(0xD, -1, &disk);
    ufi_mock_install(&disk);

    TEST("read_one_sector_matches_disk");
    uint8_t buf[512];
    ASSERT(uft_ufi_read_sectors(PATH, 100, 1, buf, sizeof(buf), &diag) == UFT_OK);
    ASSERT(memcmp(buf, disk.data + 100u * 512u, 512) == 0);
    DONE();

    TEST("read_multi_sector_matches_disk");
    static uint8_t big[4 * 512];
    ASSERT(uft_ufi_read_sectors(PATH, 200, 4, big, sizeof(big), &diag) == UFT_OK);
    ASSERT(memcmp(big, disk.data + 200u * 512u, 4u * 512u) == 0);
    DONE();

    TEST("hal_actually_issued_read_cdbs");
    ASSERT(ufi_mock_read_count() >= 2);
    DONE();

    ufi_mock_install(NULL);
    uft_ufi_gen_free(&disk);
}

/* ─── E. forensic: medium error ─────────────────────────────────────── */

static void group_e(void) {
    printf("\n-- E. medium-error sense, no fabricated data --\n");
    uft_diag_t diag;
    uft_ufi_disk_t disk;
    /* Sector 500 is a medium error. */
    uft_ufi_gen_disk(0xE, 500, &disk);
    ufi_mock_install(&disk);

    TEST("read_bad_sector_returns_error_not_ok");
    uint8_t buf[512];
    memset(buf, 0xCC, sizeof(buf));
    uft_rc_t rc = uft_ufi_read_sectors(PATH, 500, 1, buf, sizeof(buf), &diag);
    ASSERT(rc != UFT_OK);   /* HAL surfaced the error — no fake success */
    DONE();

    TEST("request_sense_reports_medium_error");
    uint8_t key = 0, asc = 0, ascq = 0;
    ASSERT(uft_ufi_request_sense(PATH, &key, &asc, &ascq, &diag) == UFT_OK);
    ASSERT(key == UFT_UFI_SENSE_MEDIUM_ERROR);   /* 0x03 */
    ASSERT(asc == UFT_UFI_ASC_UNREC_READ);       /* 0x11 */
    DONE();

    TEST("good_sector_still_reads_around_the_bad_one");
    ASSERT(uft_ufi_read_sectors(PATH, 499, 1, buf, sizeof(buf), &diag) == UFT_OK);
    ASSERT(memcmp(buf, disk.data + 499u * 512u, 512) == 0);
    ASSERT(uft_ufi_read_sectors(PATH, 501, 1, buf, sizeof(buf), &diag) == UFT_OK);
    DONE();

    TEST("bad_sector_error_is_repeatable_not_cleared");
    ASSERT(uft_ufi_read_sectors(PATH, 500, 1, buf, sizeof(buf), &diag) != UFT_OK);
    DONE();

    ufi_mock_install(NULL);
    uft_ufi_gen_free(&disk);
}

/* ─── F. no-disk, write, determinism ────────────────────────────────── */

static void group_f(void) {
    printf("\n-- F. no-disk sense, write, determinism --\n");
    uft_diag_t diag;
    uft_ufi_disk_t disk;
    uft_ufi_gen_disk(0xF, -1, &disk);

    TEST("no_disk_test_unit_ready_fails");
    disk.disk_present = false;
    ufi_mock_install(&disk);
    ASSERT(uft_ufi_test_unit_ready(PATH, &diag) != UFT_OK);
    DONE();

    TEST("no_disk_sense_is_not_ready");
    uint8_t key = 0, asc = 0, ascq = 0;
    ASSERT(uft_ufi_request_sense(PATH, &key, &asc, &ascq, &diag) == UFT_OK);
    ASSERT(key == UFT_UFI_SENSE_NOT_READY);   /* 0x02 */
    ASSERT(asc == UFT_UFI_ASC_NO_MEDIUM);     /* 0x3A */
    DONE();

    TEST("write10_accepted_by_device");
    disk.disk_present = true;
    ufi_mock_install(&disk);
    uint8_t wbuf[512];
    memset(wbuf, 0x55, sizeof(wbuf));
    ASSERT(uft_ufi_write_sectors(PATH, 10, 1, wbuf, sizeof(wbuf), &diag) == UFT_OK);
    DONE();

    TEST("disk_generation_is_deterministic");
    uft_ufi_disk_t d1, d2;
    uft_ufi_gen_disk(0x1234, -1, &d1);
    uft_ufi_gen_disk(0x1234, -1, &d2);
    ASSERT(d1.total_lba == d2.total_lba);
    ASSERT(memcmp(d1.data, d2.data,
                  (size_t)d1.total_lba * d1.block_size) == 0);
    uft_ufi_gen_free(&d1);
    uft_ufi_gen_free(&d2);
    DONE();

    TEST("lba_ok_helper_flags_bad_sector");
    uft_ufi_disk_t db;
    uft_ufi_gen_disk(1, 42, &db);
    ASSERT(uft_ufi_gen_lba_ok(&db, 41) == true);
    ASSERT(uft_ufi_gen_lba_ok(&db, 42) == false);
    ASSERT(uft_ufi_gen_lba_ok(&db, 999999) == false);
    uft_ufi_gen_free(&db);
    DONE();

    ufi_mock_install(NULL);
    uft_ufi_gen_free(&disk);
}

int main(void) {
    printf("=== UFI firmware-realistic emulator tests (9/9 finale) ===\n");
    group_a();
    group_b();
    group_c();
    group_d();
    group_e();
    group_f();
    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
