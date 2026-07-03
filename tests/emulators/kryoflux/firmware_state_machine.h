/**
 * @file tests/emulators/kryoflux/firmware_state_machine.h
 * @brief DTC-subprocess + device model for the KryoFlux controller.
 *
 * KryoFlux is not driven by a wire protocol — the UFT HAL
 * (src/hal/uft_kryoflux_dtc.c) runs the DTC command-line tool as a
 * subprocess, which drives the board and writes a KryoFlux RAW stream
 * file. So this "firmware state machine" models the DTC + device pair:
 * a capture request (track, side, revolutions) either produces a RAW
 * stream (fed from the synthetic generator) or fails the way DTC would,
 * with a DTC-style exit code.
 *
 * SPEC_STATUS: REVERSE-ENGINEERED. Modelled on the DTC argv contract
 * built in src/hal/uft_kryoflux_dtc.c (-f/-i/-e/-s/-g0/-d/-k2/-t/-p)
 * and the KryoFlux RAW stream protocol the HAL parses. Every place the
 * real DTC/board behaviour is inferred rather than known is recorded in
 * DIVERGENCES.md.
 *
 * Forensic invariants:
 *   - The model NEVER fabricates flux — a successful capture returns the
 *     bytes the synthetic generator built, verbatim.
 *   - Write requests are ALWAYS refused (KryoFlux is read-only in UFT;
 *     the HAL's write path is a documented honest-stub). The refusal is
 *     visible via a distinct exit code, never a silent no-op.
 *   - A failed capture returns a non-zero exit code AND an empty stream —
 *     the caller can never mistake a failure for a short read.
 */
#ifndef UFT_TESTS_KF_DTC_STATE_MACHINE_H
#define UFT_TESTS_KF_DTC_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── DTC exit codes (modelled; see DIVERGENCES.md K-1) ─────────────── */
typedef enum {
    KF_DTC_EXIT_OK          = 0,   /* capture succeeded, stream written  */
    KF_DTC_EXIT_NO_DEVICE   = 1,   /* no KryoFlux board found            */
    KF_DTC_EXIT_NO_DISK     = 2,   /* drive empty / not ready            */
    KF_DTC_EXIT_NO_INDEX    = 3,   /* no index pulse (bad drive / disk)  */
    KF_DTC_EXIT_BAD_ARGS    = 4,   /* malformed command line             */
    KF_DTC_EXIT_WRITE_DENY  = 5,   /* write requested — refused (RO)     */
    KF_DTC_EXIT_IO          = 6,   /* transfer / buffering failure       */
} kf_dtc_exit_t;

typedef enum {
    KF_DTC_STATE_DISCONNECTED = 0, /* no board                           */
    KF_DTC_STATE_READY        = 1, /* board detected, idle               */
    KF_DTC_STATE_CAPTURING    = 2, /* mid-capture (transient)            */
    KF_DTC_STATE_ERROR        = 3, /* last op failed                     */
} kf_dtc_state_t;

/* ─── capture request (mirrors the HAL argv knobs) ──────────────────── */
typedef struct {
    int  track;          /* 0..83 (-i/-e) */
    int  side;           /* 0/1   (-s)    */
    int  revolutions;    /* 1..5          */
    bool double_step;    /* -k2           */
    bool is_write;       /* write request — always denied */
} kf_dtc_request_t;

/* ─── device model ──────────────────────────────────────────────────── */
typedef struct {
    kf_dtc_state_t state;

    /* Physical truth (configurable per test). */
    bool   device_present;
    bool   disk_present;
    bool   index_present;
    int    device_count;      /* uft_kf_detect_devices result */
    int    max_track;         /* stepper reach (default 83) */

    /* Fault injection. */
    int    fail_next_n;       /* next N captures return retryable IO error */
    uint64_t stream_seed;     /* seed handed to the synthetic generator */
    uint32_t inject_defects;  /* uft_kf_defect_flags_t for produced streams */

    /* Last-capture bookkeeping. */
    kf_dtc_exit_t last_exit;
    int          retries_used;
    uint64_t     captures_ok;
    uint64_t     captures_failed;
} kf_dtc_t;

/* ─── lifecycle ─────────────────────────────────────────────────────── */

/** Reset to DISCONNECTED with sane defaults (no device, no disk). */
void kf_dtc_reset(kf_dtc_t *dev);

/** Bench-ready device: board present, disk in, index OK, one device,
 *  84-track reach, clean streams. */
void kf_dtc_power_on_defaults(kf_dtc_t *dev);

void kf_dtc_set_device_present(kf_dtc_t *dev, bool present);
void kf_dtc_set_disk_present(kf_dtc_t *dev, bool present);
void kf_dtc_set_index_present(kf_dtc_t *dev, bool present);
void kf_dtc_set_max_track(kf_dtc_t *dev, int max_track);
/** Next `n` capture calls return a retryable IO failure. */
void kf_dtc_set_fail_next(kf_dtc_t *dev, int n);
void kf_dtc_set_stream_seed(kf_dtc_t *dev, uint64_t seed);
/** Defect bitmask (uft_kf_defect_flags_t) injected into produced streams. */
void kf_dtc_set_inject_defects(kf_dtc_t *dev, uint32_t defects);

/* ─── operations ────────────────────────────────────────────────────── */

/** Model `dtc -i`: returns device count and moves to READY if any. */
int kf_dtc_detect(kf_dtc_t *dev);

/** Model one `dtc` capture invocation for a single (track, side). On
 *  success, allocates *out_stream / *out_len with the synthetic RAW
 *  stream (caller frees via free()) and returns KF_DTC_EXIT_OK. On
 *  failure returns a non-zero exit code and leaves *out_stream = NULL.
 *  `max_retries` models the HAL's -t retry count: a transient IO error
 *  is retried up to that many times before giving up. */
kf_dtc_exit_t kf_dtc_capture(kf_dtc_t *dev, const kf_dtc_request_t *req,
                             int max_retries,
                             uint8_t **out_stream, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_KF_DTC_STATE_MACHINE_H */
