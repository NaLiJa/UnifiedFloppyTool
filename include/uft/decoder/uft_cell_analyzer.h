/**
 * @file uft_cell_analyzer.h
 * @brief Enhanced Cell-Level Flux Analysis
 * 
 * Based on DTC's CCellAnalyzer - provides detailed cell timing
 * analysis and quality metrics.
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Decoder pipeline
 *
 * This header declares 16 public functions; 15 are NOT implemented
 * in the source tree (only 1 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_CELL_ANALYZER_H
#define UFT_CELL_ANALYZER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_error.h"

/* Map UFT_OK to UFT_SUCCESS for compatibility */
#ifndef UFT_OK
#define UFT_OK UFT_SUCCESS
#endif
#ifndef UFT_ERR_INVALID_PARAM
#define UFT_ERR_INVALID_PARAM UFT_ERR_INVALID_ARG
#endif
#ifndef UFT_ERR_MEMORY
#define UFT_ERR_MEMORY (-20)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cell search modes (like DTC -x parameter) */
#define UFT_CELL_SEARCH_OFF         0
#define UFT_CELL_SEARCH_NORMAL      1
#define UFT_CELL_SEARCH_EXTENDED    2

/* Common cell times in nanoseconds */
#define UFT_CELL_MFM_DD_NS         2000   /* 250 kbps MFM DD */
#define UFT_CELL_MFM_HD_NS         1000   /* 500 kbps MFM HD */
#define UFT_CELL_MFM_ED_NS          500   /* 1 Mbps MFM ED */
#define UFT_CELL_FM_DD_NS          4000   /* 125 kbps FM DD */
#define UFT_CELL_GCR_C64_NS        3250   /* ~307 kbps C64 zone 0 */
#define UFT_CELL_GCR_APPLE_NS      4000   /* 250 kbps Apple */

/* Default tolerances */
#define UFT_CELL_DEFAULT_TOLERANCE  0.15  /* 15% tolerance */
#define UFT_CELL_TIGHT_TOLERANCE    0.08  /* 8% tight tolerance */
#define UFT_CELL_LOOSE_TOLERANCE    0.25  /* 25% loose tolerance */

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Cell analysis options
 */
typedef struct {
    double cell_time_ns;          /**< Nominal cell time in nanoseconds */
    double tolerance;             /**< Timing tolerance (0.0-1.0) */
    double sample_rate_hz;        /**< Sample rate in Hz */
    uint8_t search_mode;          /**< Cell band search mode */
    uint32_t pll_window;          /**< PLL tracking window size */
    double pll_gain;              /**< PLL gain factor */
    bool detect_weak_bits;        /**< Enable weak bit detection */
    double weak_threshold;        /**< Weak bit threshold */
    bool auto_detect_rate;        /**< Auto-detect cell rate */
} uft_cell_options_t;

/**
 * @brief Information about a single decoded cell
 */
typedef struct {
    uint64_t position;            /**< Bit position in stream */
    double actual_time_ns;        /**< Actual cell time */
    double deviation_ns;          /**< Deviation from nominal */
    double deviation_pct;         /**< Deviation percentage */
    uint8_t value;                /**< Decoded bit value (0 or 1) */
    uint8_t confidence;           /**< Confidence 0-100 */
    bool is_weak;                 /**< Weak bit flag */
    bool is_sync;                 /**< Part of sync pattern */
} uft_cell_info_t;

/**
 * @brief Cell timing histogram bin
 */
typedef struct {
    double center_ns;             /**< Bin center in ns */
    uint32_t count;               /**< Number of cells in bin */
    double percentage;            /**< Percentage of total */
} uft_cell_histogram_bin_t;

/**
 * @brief Cell timing histogram
 */
typedef struct {
    uft_cell_histogram_bin_t *bins;
    size_t bin_count;
    double min_time_ns;
    double max_time_ns;
    double peak_time_ns;          /**< Most common cell time */
    uint32_t total_cells;
} uft_cell_histogram_t;

/**
 * @brief Cell analysis result
 */
typedef struct {
    /* Cell data */
    uft_cell_info_t *cells;       /**< Array of cell info */
    size_t cell_count;            /**< Number of cells decoded */
    
    /* Decoded bits */
    uint8_t *decoded_data;        /**< Decoded bit stream */
    size_t bit_count;             /**< Number of decoded bits */
    
    /* Statistics */
    double average_cell_time;     /**< Average cell time */
    double cell_time_stddev;      /**< Standard deviation */
    double min_cell_time;         /**< Minimum observed */
    double max_cell_time;         /**< Maximum observed */
    
    /* Quality metrics */
    uint32_t weak_bit_count;      /**< Number of weak bits */
    uint32_t error_count;         /**< Decoding errors */
    uint8_t overall_quality;      /**< Overall quality 0-100 */
    
    /* Sync detection */
    uint32_t sync_count;          /**< Sync patterns found */
    uint64_t *sync_positions;     /**< Positions of sync patterns */
    
    /* Histogram */
    uft_cell_histogram_t histogram;
    
    /* PLL state */
    double final_pll_phase;
    double final_pll_freq;
    
    /* Auto-detected rate */
    double detected_cell_time;
    double detected_bitrate;
} uft_cell_result_t;

/**
 * @brief Cell band information (for multi-rate disks)
 */
typedef struct {
    uint8_t zone;                 /**< Speed zone (C64: 0-3) */
    double cell_time_ns;          /**< Cell time for this zone */
    uint64_t start_position;      /**< Start bit position */
    uint64_t end_position;        /**< End bit position */
    uint32_t cell_count;          /**< Cells in this band */
} uft_cell_band_t;

/**
 * @brief Multi-zone analysis result
 */
typedef struct {
    uft_cell_band_t *bands;
    size_t band_count;
    bool is_multi_rate;           /**< True if multiple rates detected */
} uft_cell_band_result_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */














/* ============================================================================
 * PLL State (for advanced use)
 * ============================================================================ */

/**
 * @brief PLL state for manual stepping
 */
#ifndef UFT_PLL_STATE_T_DEFINED
#define UFT_PLL_STATE_T_DEFINED
typedef struct {
    double phase;             /**< Current phase in samples */
    double frequency;         /**< Current frequency in samples/cell */
    double nominal_freq;      /**< Nominal frequency */
    double gain;              /**< PLL gain */
    uint32_t window;          /**< Window size */
    bool locked;              /**< PLL is locked */
    uint64_t bit_position;    /**< Current bit position */
} uft_pll_state_t;
#endif /* UFT_PLL_STATE_T_DEFINED */

/**
 * @brief Initialize PLL state
 */
void uft_pll_init(
    uft_pll_state_t *pll,
    double cell_time_samples,
    double gain
);



#ifdef __cplusplus
}
#endif

#endif /* UFT_CELL_ANALYZER_H */
