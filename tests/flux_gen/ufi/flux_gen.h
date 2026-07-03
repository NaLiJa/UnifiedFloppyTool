/**
 * @file tests/flux_gen/ufi/flux_gen.h
 * @brief Synthetic UFI floppy-disk generator (sector-domain).
 *
 * UFI (USB Floppy Interface, SFF-8070i) is a SCSI-like command set over
 * USB Mass Storage. It is a SECTOR device: the drive decodes the disk
 * and serves LBA sectors via READ(10) / READ_CAPACITY / INQUIRY CDBs.
 * So this "generator" produces a synthetic floppy image (LBA sectors +
 * a bad-sector map + device identity), not flux.
 *
 * Unlike the other controllers, the UFI emulator drives the REAL HAL:
 * the mock backend (tests/emulators/ufi/firmware_state_machine.c)
 * implements the uft_ufi_ops_t vtable and answers CDBs from THIS disk,
 * and the production functions in src/hal/ufi.c (uft_ufi_inquiry,
 * uft_ufi_read_capacity, uft_ufi_read_sectors, uft_ufi_request_sense)
 * are exercised end-to-end against it. That makes this an end-to-end
 * conformance test of the UFI HAL's CDB building + response parsing.
 *
 * Default geometry: 1.44 MB 3.5" HD = 80 cyl × 2 heads × 18 sectors ×
 * 512 bytes = 2880 LBA. Determinism: xorshift64* per sector.
 */
#ifndef UFT_TESTS_UFI_DISK_GEN_H
#define UFT_TESTS_UFI_DISK_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Standard 1.44 MB geometry ─────────────────────────────────────── */
#define UFT_UFI_GEN_CYLINDERS   80u
#define UFT_UFI_GEN_HEADS       2u
#define UFT_UFI_GEN_SPT         18u
#define UFT_UFI_GEN_BPS         512u
#define UFT_UFI_GEN_TOTAL_LBA   (UFT_UFI_GEN_CYLINDERS * UFT_UFI_GEN_HEADS * UFT_UFI_GEN_SPT) /* 2880 */

/* Sense keys / codes (SCSI, subset) for the medium-error path. */
#define UFT_UFI_SENSE_NO_SENSE     0x00u
#define UFT_UFI_SENSE_NOT_READY    0x02u   /* no disk */
#define UFT_UFI_SENSE_MEDIUM_ERROR 0x03u   /* unrecovered read error */
#define UFT_UFI_ASC_NO_MEDIUM      0x3Au
#define UFT_UFI_ASC_UNREC_READ     0x11u

/* ─── Errors ────────────────────────────────────────────────────────── */
typedef enum {
    UFT_UFI_GEN_OK        = 0,
    UFT_UFI_GEN_ERR_NULL  = -1,
    UFT_UFI_GEN_ERR_NOMEM = -2,
    UFT_UFI_GEN_ERR_SPEC  = -3,
} uft_ufi_gen_err_t;

/* ─── Synthetic disk ────────────────────────────────────────────────── */
typedef struct {
    uint32_t total_lba;
    uint16_t block_size;
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors_per_track;

    uint8_t *data;            /* total_lba × block_size bytes */
    uint8_t *bad_lba;         /* total_lba bytes: 1 = medium-error sector */

    char     vendor[9];       /* INQUIRY vendor  (8 chars + NUL) */
    char     product[17];     /* INQUIRY product (16 chars + NUL) */
    char     revision[5];     /* INQUIRY revision (4 chars + NUL) */

    bool     disk_present;
} uft_ufi_disk_t;

/* ─── Public API ────────────────────────────────────────────────────── */

/** Build a synthetic 1.44 MB floppy with deterministic sector data.
 *  `bad_lba` (or -1 for none) marks one sector as a medium error. */
uft_ufi_gen_err_t uft_ufi_gen_disk(uint64_t seed, int bad_lba,
                                   uft_ufi_disk_t *out);

void uft_ufi_gen_free(uft_ufi_disk_t *d);

/** True if `lba` is within range and NOT marked bad. */
bool uft_ufi_gen_lba_ok(const uft_ufi_disk_t *d, uint32_t lba);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_UFI_DISK_GEN_H */
