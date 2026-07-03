/**
 * @file tests/flux_gen/kryoflux/flux_gen.h
 * @brief Synthetic KryoFlux RAW stream generator.
 *
 * Unlike the other emulators, KryoFlux is not driven by a USB/serial
 * wire protocol — the UFT HAL runs the DTC command-line tool, which
 * writes a KryoFlux RAW *stream* file that UFT then parses with
 * uft_kf_decode() (src/flux/uft_kryoflux_stream.c). So the "flux
 * generator" here emits exactly that container format, and the emulator
 * test feeds the output straight into the PRODUCTION parser and asserts
 * on the decoded flux + the returned uft_kf_status_t. That closes the
 * loop: a defect class here maps to a specific parser status code.
 *
 * STREAM FORMAT (public KryoFlux protocol, matches the decoder):
 *   0x00..0x07  Flux2  — 2-byte value (op<<8)|next
 *   0x08 Nop1 · 0x09 Nop2 · 0x0A Nop3
 *   0x0B        Ovl16  — add 0x10000 to the next flux value
 *   0x0C        Flux3  — 3-byte value (b1<<8)|b2
 *   0x0D        OOB    — type + LE16 size + payload (StreamInfo / Index /
 *                        StreamEnd / KFInfo / EOF); OOB bytes do NOT
 *                        advance the cell-stream position counter
 *   0x0E..0xFF  Flux1  — single-byte value
 *
 * Sample clock: 24.027428 MHz (~41.6 ns/tick), cross-checked against the
 * HAL SSOT uft_kf_get_sample_clock() in the test.
 *
 * Determinism: xorshift64* RNG, caller-supplied seed. The stream is
 * serialised byte-by-byte (no host-endian leak). Same seed => identical
 * bytes on every platform.
 *
 * Forensic-medium-safety: refuses to emit cells whose duration is
 * outside [MIN_NS, MAX_NS] or weak-bit jitter above 10% (a write-back of
 * such a stream would push transitions out of spec).
 */
#ifndef UFT_TESTS_KF_FLUX_GEN_H
#define UFT_TESTS_KF_FLUX_GEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Constants ─────────────────────────────────────────────────────── */

/** KryoFlux sample clock (Hz). MUST equal the HAL's UFT_KF_SAMPLE_CLOCK;
 *  the test asserts uft_kf_get_sample_clock() == this. */
#define UFT_KF_GEN_SAMPLE_HZ    24027428.5714285

/** DD MFM nominal cell = 4 µs. At 24.027 MHz that is ~96 ticks. */
#define UFT_KF_GEN_CELL_DD_NS   4000u

/** Forensic-safety window for a single flux interval. */
#define UFT_KF_GEN_MIN_NS       1000u        /* 1 µs  */
#define UFT_KF_GEN_MAX_NS       200000u      /* 200 µs — long-gap cap */
#define UFT_KF_GEN_MAX_JITTER   10u          /* percent */

/** Track / revolution bounds. */
#define UFT_KF_GEN_MAX_TRACK    83
#define UFT_KF_GEN_MIN_REVS     1
#define UFT_KF_GEN_MAX_REVS     5

/* ─── Error codes ───────────────────────────────────────────────────── */
typedef enum {
    UFT_KF_GEN_OK              = 0,
    UFT_KF_GEN_ERR_NULL        = -1,
    UFT_KF_GEN_ERR_OUT_OF_SPEC = -2,
    UFT_KF_GEN_ERR_NOMEM       = -3,
} uft_kf_gen_err_t;

/* ─── Defect classes (bitmask) ──────────────────────────────────────────
 *
 * The first group is flux-domain; the second maps to a specific
 * uft_kf_status_t the production parser will return, which the test
 * asserts. Only ONE of the container-level defects should be set per
 * call (they alter the terminating OOB blocks). */
typedef enum {
    UFT_KF_DEFECT_NONE          = 0,
    /* flux-domain */
    UFT_KF_DEFECT_WEAK_BITS     = 1u << 0,  /* per-cell jitter (<=10%)      */
    UFT_KF_DEFECT_LONG_CELL     = 1u << 1,  /* one damaged over-long gap    */
    /* container-level → parser status */
    UFT_KF_DEFECT_MISSING_END   = 1u << 2,  /* omit StreamEnd+EOF → MISSING_END */
    UFT_KF_DEFECT_NO_INDEX_BLK  = 1u << 3,  /* omit Index OOB → index_count 0   */
    UFT_KF_DEFECT_DEV_NO_INDEX  = 1u << 4,  /* StreamEnd result=NO_INDEX → DEV_INDEX */
    UFT_KF_DEFECT_DEV_BUFFER    = 1u << 5,  /* StreamEnd result=BUFFERING → DEV_BUFFER */
    UFT_KF_DEFECT_TRUNCATED     = 1u << 6,  /* cut mid-flux → MISSING_DATA  */
} uft_kf_defect_flags_t;

/* ─── Params ────────────────────────────────────────────────────────── */
typedef struct {
    uint64_t seed;
    int      track;            /* 0..83 */
    int      side;             /* 0..1  */
    int      revolutions;      /* 1..5  */
    uint32_t cell_ns;          /* nominal cell (0 => DD default) */
    uint32_t defects;          /* uft_kf_defect_flags_t bitmask */
    uint8_t  weak_jitter_pct;  /* 1..10 when WEAK_BITS set */
} uft_kf_gen_params_t;

/* ─── Output ────────────────────────────────────────────────────────── */
typedef struct {
    uint8_t  *bytes;              /* the RAW stream, malloc'd */
    size_t    bytes_len;
    uint32_t  flux_count;         /* flux transitions ENCODED (pre-truncation) */
    uint32_t  index_count;        /* Index OOB blocks emitted */
    uint32_t  revolution_ticks;   /* nominal index-to-index, KF ticks */
    double    rpm;                /* spindle speed the track was mastered at */
} uft_kf_gen_stream_t;

/* ─── Public API ────────────────────────────────────────────────────── */

/** Generate one KryoFlux RAW stream for an MFM-DD track: `revolutions`
 *  worth of nominal cells, an StreamInfo header, per-revolution Index
 *  OOB blocks, and (unless a container defect suppresses it) a
 *  StreamEnd + EOF. Allocates out->bytes; free with uft_kf_gen_free. */
uft_kf_gen_err_t uft_kf_gen_stream(const uft_kf_gen_params_t *params,
                                   uft_kf_gen_stream_t *out);

/** Free the buffer from uft_kf_gen_stream. Safe on NULL / empty. */
void uft_kf_gen_free(uft_kf_gen_stream_t *s);

/** Count cell intervals outside the medium-safe [MIN_NS, MAX_NS] window.
 *  0 == safe to (hypothetically) feed to a write path. Decodes the
 *  stream itself, so it also validates the generator's own output. */
size_t uft_kf_gen_count_unsafe(const uft_kf_gen_stream_t *s);

/** Encode a single flux value into the RAW opcode stream at *pos (buffer
 *  cap `cap`). Advances *pos. Exposed for white-box tests of the
 *  Ovl16/Flux1/Flux2/Flux3 boundaries. Returns bytes written or 0 on
 *  overflow. */
size_t uft_kf_gen_encode_flux(uint8_t *buf, size_t cap, size_t *pos,
                              uint32_t value);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_KF_FLUX_GEN_H */
