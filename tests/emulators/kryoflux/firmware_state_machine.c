/**
 * @file tests/emulators/kryoflux/firmware_state_machine.c
 * @brief Implementation of the DTC-subprocess + device model.
 *
 * See firmware_state_machine.h. Capture success produces a synthetic
 * KryoFlux RAW stream via the generator in tests/flux_gen/kryoflux; the
 * test then decodes it with the PRODUCTION parser uft_kf_decode() and
 * asserts on both the flux and the returned status.
 */

#include "firmware_state_machine.h"
#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

/* ─── lifecycle ─────────────────────────────────────────────────────── */

void kf_dtc_reset(kf_dtc_t *dev) {
    if (!dev) return;
    memset(dev, 0, sizeof(*dev));
    dev->state       = KF_DTC_STATE_DISCONNECTED;
    dev->max_track   = UFT_KF_GEN_MAX_TRACK;
    dev->stream_seed = 0xC0FFEEULL;
    dev->last_exit   = KF_DTC_EXIT_NO_DEVICE;
}

void kf_dtc_power_on_defaults(kf_dtc_t *dev) {
    if (!dev) return;
    kf_dtc_reset(dev);
    dev->device_present = true;
    dev->disk_present   = true;
    dev->index_present  = true;
    dev->device_count   = 1;
    dev->state          = KF_DTC_STATE_READY;
}

void kf_dtc_set_device_present(kf_dtc_t *dev, bool p) {
    if (dev) { dev->device_present = p; if (!p) dev->state = KF_DTC_STATE_DISCONNECTED; }
}
void kf_dtc_set_disk_present(kf_dtc_t *dev, bool p) { if (dev) dev->disk_present = p; }
void kf_dtc_set_index_present(kf_dtc_t *dev, bool p) { if (dev) dev->index_present = p; }
void kf_dtc_set_max_track(kf_dtc_t *dev, int t) { if (dev) dev->max_track = t; }
void kf_dtc_set_fail_next(kf_dtc_t *dev, int n) { if (dev) dev->fail_next_n = n; }
void kf_dtc_set_stream_seed(kf_dtc_t *dev, uint64_t s) { if (dev) dev->stream_seed = s; }
void kf_dtc_set_inject_defects(kf_dtc_t *dev, uint32_t d) { if (dev) dev->inject_defects = d; }

/* ─── detect ────────────────────────────────────────────────────────── */

int kf_dtc_detect(kf_dtc_t *dev) {
    if (!dev) return -1;
    if (!dev->device_present) {
        dev->device_count = 0;
        dev->state = KF_DTC_STATE_DISCONNECTED;
        dev->last_exit = KF_DTC_EXIT_NO_DEVICE;
        return 0;
    }
    dev->device_count = dev->device_count > 0 ? dev->device_count : 1;
    dev->state = KF_DTC_STATE_READY;
    return dev->device_count;
}

/* ─── capture ───────────────────────────────────────────────────────── */

static kf_dtc_exit_t fail(kf_dtc_t *dev, kf_dtc_exit_t code) {
    dev->state = KF_DTC_STATE_ERROR;
    dev->last_exit = code;
    dev->captures_failed++;
    return code;
}

kf_dtc_exit_t kf_dtc_capture(kf_dtc_t *dev, const kf_dtc_request_t *req,
                             int max_retries,
                             uint8_t **out_stream, size_t *out_len) {
    if (out_stream) *out_stream = NULL;
    if (out_len) *out_len = 0;
    if (!dev || !req) return KF_DTC_EXIT_BAD_ARGS;

    dev->retries_used = 0;

    /* Write is always refused — KryoFlux is read-only in UFT. */
    if (req->is_write)
        return fail(dev, KF_DTC_EXIT_WRITE_DENY);

    /* Argument validation (mirrors the HAL's range checks). */
    if (req->track < 0 || req->track > dev->max_track ||
        req->side < 0 || req->side > 1 ||
        req->revolutions < UFT_KF_GEN_MIN_REVS ||
        req->revolutions > UFT_KF_GEN_MAX_REVS)
        return fail(dev, KF_DTC_EXIT_BAD_ARGS);

    if (!dev->device_present) return fail(dev, KF_DTC_EXIT_NO_DEVICE);
    if (!dev->disk_present)   return fail(dev, KF_DTC_EXIT_NO_DISK);
    if (!dev->index_present)  return fail(dev, KF_DTC_EXIT_NO_INDEX);

    dev->state = KF_DTC_STATE_CAPTURING;

    /* Transient IO failure with retry, modelling DTC -t. Each retry
     * "uses up" one injected failure. */
    while (dev->fail_next_n > 0) {
        if (dev->retries_used >= max_retries) {
            /* Exhausted retries — give up with an IO error. */
            return fail(dev, KF_DTC_EXIT_IO);
        }
        dev->retries_used++;
        dev->fail_next_n--;
    }

    /* Produce the RAW stream via the synthetic generator. The track's
     * seed folds in track/side so different tracks differ deterministically. */
    uft_kf_gen_params_t gp = {
        .seed = dev->stream_seed ^ ((uint64_t)req->track << 8) ^ (uint64_t)req->side,
        .track = req->track,
        .side = req->side,
        .revolutions = req->revolutions,
        .cell_ns = 0,   /* DD default */
        .defects = dev->inject_defects,
        .weak_jitter_pct = (dev->inject_defects & UFT_KF_DEFECT_WEAK_BITS) ? 8 : 0,
    };
    uft_kf_gen_stream_t s;
    if (uft_kf_gen_stream(&gp, &s) != UFT_KF_GEN_OK)
        return fail(dev, KF_DTC_EXIT_IO);

    if (out_stream && out_len) {
        *out_stream = s.bytes;   /* transfer ownership to caller */
        *out_len = s.bytes_len;
        s.bytes = NULL;          /* prevent double-free */
    }
    uft_kf_gen_free(&s);

    dev->state = KF_DTC_STATE_READY;
    dev->last_exit = KF_DTC_EXIT_OK;
    dev->captures_ok++;
    return KF_DTC_EXIT_OK;
}
