/**
 * @file tests/emulators/ufi/firmware_state_machine.c
 * @brief Implementation of the mock UFI backend (uft_ufi_ops_t vtable).
 *
 * exec_cdb dispatches on cdb[0] and fills the data buffer with the SCSI
 * response a real UFI floppy would return, from the installed synthetic
 * disk. The production HAL (src/hal/ufi.c) builds the CDBs and parses the
 * responses; this backend closes the loop.
 */

#include "firmware_state_machine.h"

#include <stdlib.h>
#include <string.h>

/* ─── mock device handle ────────────────────────────────────────────── */
struct uft_ufi_device {
    const uft_ufi_disk_t *disk;
    int placeholder;
};

/* Installed disk + counters + latched sense (set by a failing command,
 * read back by REQUEST_SENSE — SCSI contemporaneous-sense semantics). */
static const uft_ufi_disk_t *g_disk = NULL;
static uint64_t g_cdb_count  = 0;
static uint64_t g_read_count = 0;
static uint8_t  g_sense_key  = UFT_UFI_SENSE_NO_SENSE;
static uint8_t  g_sense_asc  = 0;
static uint8_t  g_sense_ascq = 0;

static void latch_sense(uint8_t key, uint8_t asc, uint8_t ascq) {
    g_sense_key = key; g_sense_asc = asc; g_sense_ascq = ascq;
}

/* ─── vtable: open / close ──────────────────────────────────────────── */

static uft_rc_t mock_open(uft_ufi_device_t **dev, const char *path,
                          uft_diag_t *diag) {
    (void)path;
    if (!dev) return UFT_ERR_IO;
    uft_ufi_device_t *d = (uft_ufi_device_t *)calloc(1, sizeof(*d));
    if (!d) { uft_diag_set(diag, "ufi-mock: alloc"); return UFT_ERR_MEMORY; }
    d->disk = g_disk;
    *dev = d;
    return UFT_OK;
}

static void mock_close(uft_ufi_device_t *dev) { free(dev); }

/* ─── vtable: exec_cdb ──────────────────────────────────────────────── */

static void put_be32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);  b[3] = (uint8_t)v;
}
static uint32_t get_be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | b[3];
}

static uft_rc_t mock_exec_cdb(uft_ufi_device_t *dev,
                              const uint8_t *cdb, size_t cdb_len,
                              void *data, size_t data_len,
                              int data_dir, uint32_t timeout_ms,
                              uft_diag_t *diag) {
    (void)cdb_len; (void)data_dir; (void)timeout_ms;
    g_cdb_count++;
    if (!dev || !cdb) return UFT_ERR_IO;
    const uft_ufi_disk_t *disk = dev->disk;
    uint8_t *buf = (uint8_t *)data;

    switch (cdb[0]) {
    case UFT_UFI_TEST_UNIT_READY:   /* 0x00 */
        if (!disk || !disk->disk_present) {
            latch_sense(UFT_UFI_SENSE_NOT_READY, UFT_UFI_ASC_NO_MEDIUM, 0);
            uft_diag_set(diag, "ufi-mock: not ready (no disk)");
            return UFT_ERR_IO;
        }
        return UFT_OK;

    case UFT_UFI_INQUIRY:           /* 0x12 */
        if (!buf || data_len < 36) return UFT_ERR_IO;
        memset(buf, 0, 36);
        buf[0] = 0x00;              /* direct-access device */
        buf[1] = 0x80;              /* removable */
        buf[3] = 0x01;              /* response data format */
        buf[4] = 31;               /* additional length */
        memcpy(buf + 8,  disk ? disk->vendor   : "UFTEMU  ", 8);
        memcpy(buf + 16, disk ? disk->product  : "USB FLOPPY DRIVE", 16);
        memcpy(buf + 32, disk ? disk->revision : "1.0 ", 4);
        return UFT_OK;

    case UFT_UFI_REQUEST_SENSE: {   /* 0x03 */
        if (!buf || data_len < 18) return UFT_ERR_IO;
        memset(buf, 0, 18);
        buf[0]  = 0x70;            /* current error, valid */
        buf[2]  = g_sense_key & 0x0F;
        buf[7]  = 10;             /* additional sense length */
        buf[12] = g_sense_asc;
        buf[13] = g_sense_ascq;
        /* Sense is consumed once read (SCSI clears after REQUEST SENSE). */
        latch_sense(UFT_UFI_SENSE_NO_SENSE, 0, 0);
        return UFT_OK;
    }

    case UFT_UFI_READ_CAPACITY:     /* 0x25 */
        if (!buf || data_len < 8) return UFT_ERR_IO;
        if (!disk || !disk->disk_present) {
            latch_sense(UFT_UFI_SENSE_NOT_READY, UFT_UFI_ASC_NO_MEDIUM, 0);
            return UFT_ERR_IO;
        }
        put_be32(&buf[0], disk->total_lba - 1);   /* last LBA */
        put_be32(&buf[4], disk->block_size);
        return UFT_OK;

    case UFT_UFI_READ_10: {         /* 0x28 */
        g_read_count++;
        if (!disk || !disk->disk_present) {
            latch_sense(UFT_UFI_SENSE_NOT_READY, UFT_UFI_ASC_NO_MEDIUM, 0);
            return UFT_ERR_IO;
        }
        uint32_t lba = get_be32(&cdb[2]);
        uint16_t count = (uint16_t)((cdb[7] << 8) | cdb[8]);
        if (!buf || (size_t)count * disk->block_size > data_len)
            return UFT_ERR_IO;
        for (uint16_t i = 0; i < count; i++) {
            uint32_t cur = lba + i;
            if (cur >= disk->total_lba || disk->bad_lba[cur]) {
                /* Medium error — surface it, DO NOT fabricate the sector. */
                latch_sense(UFT_UFI_SENSE_MEDIUM_ERROR,
                            UFT_UFI_ASC_UNREC_READ, 0);
                uft_diag_set(diag, "ufi-mock: unrecovered read error");
                return UFT_ERR_IO;
            }
            memcpy(buf + (size_t)i * disk->block_size,
                   disk->data + (size_t)cur * disk->block_size,
                   disk->block_size);
        }
        return UFT_OK;
    }

    case UFT_UFI_WRITE_10:          /* 0x2A — accepted but not persisted */
        return UFT_OK;

    default:
        latch_sense(0x05 /* ILLEGAL REQUEST */, 0x20 /* invalid opcode */, 0);
        return UFT_ERR_IO;
    }
}

/* ─── installation ──────────────────────────────────────────────────── */

static const uft_ufi_ops_t g_mock_ops = {
    .open     = mock_open,
    .close    = mock_close,
    .exec_cdb = mock_exec_cdb,
};

void ufi_mock_install(const uft_ufi_disk_t *disk) {
    g_disk = disk;
    g_cdb_count = 0;
    g_read_count = 0;
    latch_sense(UFT_UFI_SENSE_NO_SENSE, 0, 0);
    uft_ufi_set_backend(disk ? &g_mock_ops : NULL);
}

uint64_t ufi_mock_cdb_count(void)  { return g_cdb_count; }
uint64_t ufi_mock_read_count(void) { return g_read_count; }
