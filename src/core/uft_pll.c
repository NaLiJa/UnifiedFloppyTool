/**
 * @file uft_pll.c
 * @brief Real implementation of the standalone PLL helper from uft_pll.h.
 *
 * UFT-A07 history: prior to this commit, `uft_pll_cfg_default_mfm_dd`,
 * `uft_pll_cfg_default_mfm_hd`, and `uft_flux_to_bits_pll` lived in
 * src/core/uft_core_stubs.c — but with a LOCALLY duplicated struct
 * (`uft_pll_cfg_stub_t`) and with `cfg` typed as `const void*` instead
 * of `const uft_pll_cfg_t*`. The "stubs" file's comment claimed the
 * "real impl" was in `src/core/uft_pll.c`, but that file did not exist;
 * the stub IS the only implementation.
 *
 * Risk before A07: the duplicate struct's field layout happens to
 * match `uft_pll_cfg_t` today (5× uint32_t), so the void*-cast worked
 * by accident. Any future change to `uft_pll.h`'s struct silently
 * produced garbage at every caller that included the header — a
 * classic ABI-bomb (no compiler warning, no link error, just wrong
 * bits at runtime).
 *
 * Fix: implementation moved here, struct comes from the public header,
 * signatures match exactly, duplicate removed from the stub file.
 */

#include "uft/uft_pll.h"

#include <stdint.h>
#include <stddef.h>

uft_pll_cfg_t uft_pll_cfg_default_mfm_dd(void) {
    /* 4 µs cell = 250 kbps DD */
    uft_pll_cfg_t cfg = { 4000, 3000, 5600, 3277, 4 };
    return cfg;
}

uft_pll_cfg_t uft_pll_cfg_default_mfm_hd(void) {
    /* 2 µs cell = 500 kbps HD */
    uft_pll_cfg_t cfg = { 2000, 1500, 2800, 3277, 4 };
    return cfg;
}

size_t uft_flux_to_bits_pll(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_cfg_t *cfg,
    uint8_t *out_bits,
    size_t out_bits_capacity_bits,
    uint32_t *out_final_cell_ns,
    size_t *out_dropped_transitions)
{
    if (!timestamps_ns || count < 2 || !cfg || !out_bits)
        return 0;

    uint32_t cell = cfg->cell_ns;
    uint32_t alpha = cfg->alpha_q16;
    size_t bits = 0;
    size_t dropped = 0;

    for (size_t i = 1; i < count && bits < out_bits_capacity_bits; i++) {
        uint64_t delta = timestamps_ns[i] - timestamps_ns[i - 1];
        if (delta == 0) { dropped++; continue; }

        /* How many cells fit in this interval? */
        uint32_t n = (uint32_t)((delta + cell / 2) / cell);
        if (n == 0) n = 1;
        if (n > cfg->max_run_cells) { n = cfg->max_run_cells; dropped++; }

        /* Write n-1 zero bits + 1 one bit */
        for (uint32_t b = 0; b < n - 1 && bits < out_bits_capacity_bits; b++) {
            /* zero bit: already 0 from calloc */
            bits++;
        }
        if (bits < out_bits_capacity_bits) {
            size_t byte_idx = bits / 8;
            size_t bit_idx = 7 - (bits % 8);
            out_bits[byte_idx] |= (1u << bit_idx);
            bits++;
        }

        /* Adjust cell size (PI loop, Q16 fixed point) */
        int32_t err = (int32_t)(delta - (uint64_t)n * cell);
        int32_t adj = (int32_t)((int64_t)err * alpha >> 16);
        cell = (uint32_t)((int32_t)cell + adj);
        if (cell < cfg->cell_ns_min) cell = cfg->cell_ns_min;
        if (cell > cfg->cell_ns_max) cell = cfg->cell_ns_max;
    }

    if (out_final_cell_ns) *out_final_cell_ns = cell;
    if (out_dropped_transitions) *out_dropped_transitions = dropped;
    return bits;
}
