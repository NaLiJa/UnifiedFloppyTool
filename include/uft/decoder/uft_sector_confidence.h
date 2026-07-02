/**
 * @file uft_sector_confidence.h
 * @brief Sector-Level Confidence Integration
 * 
 * P0-DC-003: Sektor-Confidence-Integration
 * 
 * Integrates confidence metrics from all sources:
 * - Hardware (flux timing, signal quality)
 * - Decoder (sync quality, CRC status)
 * - Multi-revolution (voting confidence)
 * - Protection detection (anomaly scores)
 * 
 * Provides unified confidence scoring for forensic analysis.
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Decoder pipeline
 *
 * This header declares 15 public functions; 13 are NOT implemented
 * in the source tree (only 2 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_SECTOR_CONFIDENCE_H
#define UFT_SECTOR_CONFIDENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Confidence thresholds */
#define UFT_CONF_EXCELLENT      0.95f   /**< Excellent quality */
#define UFT_CONF_GOOD           0.80f   /**< Good quality */
#define UFT_CONF_FAIR           0.60f   /**< Fair quality */
#define UFT_CONF_POOR           0.40f   /**< Poor quality */
#define UFT_CONF_BAD            0.20f   /**< Bad quality */

/** Weight factors for combined confidence */
#define UFT_CONF_W_HARDWARE     0.25f   /**< Hardware confidence weight */
#define UFT_CONF_W_DECODER      0.30f   /**< Decoder confidence weight */
#define UFT_CONF_W_MULTIREV     0.25f   /**< Multi-rev voting weight */
#define UFT_CONF_W_CRC          0.20f   /**< CRC validation weight */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Hardware-level confidence metrics
 */
typedef struct {
    float flux_timing;          /**< Flux timing stability (0-1) */
    float signal_strength;      /**< Read signal strength (0-1) */
    float head_alignment;       /**< Head alignment quality (0-1) */
    float rpm_stability;        /**< RPM stability (0-1) */
    float overall;              /**< Combined hardware confidence */
} uft_hw_confidence_t;

/**
 * @brief Decoder-level confidence metrics
 */
typedef struct {
    float sync_quality;         /**< Sync pattern quality (0-1) */
    float address_quality;      /**< Address mark quality (0-1) */
    float data_quality;         /**< Data region quality (0-1) */
    float encoding_confidence;  /**< Encoding detection confidence */
    float overall;              /**< Combined decoder confidence */
} uft_dec_confidence_t;

/**
 * @brief Multi-revolution voting confidence
 */
typedef struct {
    uint8_t revolutions;        /**< Number of revolutions analyzed */
    uint8_t agreements;         /**< Number of agreeing revolutions */
    float vote_confidence;      /**< Voting confidence (agreements/revs) */
    float variance;             /**< Bit-level variance across revs */
    float overall;              /**< Combined multi-rev confidence */
} uft_multirev_confidence_t;

/**
 * @brief CRC/Checksum validation confidence
 */
typedef struct {
    bool crc_valid;             /**< CRC validation passed */
    uint16_t calculated_crc;    /**< Calculated CRC value */
    uint16_t stored_crc;        /**< CRC stored on disk */
    float correction_confidence;/**< If corrected, correction confidence */
    uint8_t bits_corrected;     /**< Number of bits corrected (if any) */
    float overall;              /**< CRC-based confidence (1.0 if valid) */
} uft_crc_confidence_t;

/**
 * @brief Combined sector confidence
 */
typedef struct {
    /* Component confidences */
    uft_hw_confidence_t hardware;
    uft_dec_confidence_t decoder;
    uft_multirev_confidence_t multirev;
    uft_crc_confidence_t crc;
    
    /* Weighted combination */
    float combined;             /**< Overall weighted confidence */
    
    /* Quality classification */
    uint8_t quality_level;      /**< 0=unknown, 1=bad, 2=poor, 3=fair, 4=good, 5=excellent */
    const char *quality_desc;   /**< Human-readable quality description */
    
    /* Forensic flags */
    bool weak_bits_detected;    /**< Sector contains weak bits */
    bool timing_anomaly;        /**< Timing anomalies detected */
    bool protection_suspected;  /**< Copy protection suspected */
    bool multiple_candidates;   /**< Multiple decode candidates */
    
    /* Recommendations */
    bool needs_reread;          /**< Recommend re-reading */
    bool needs_manual_review;   /**< Recommend manual review */
} uft_sector_confidence_t;

/**
 * @brief Track-level confidence summary
 */
typedef struct {
    uint16_t track;             /**< Track number */
    uint8_t side;               /**< Side */
    
    /* Sector statistics */
    uint16_t total_sectors;     /**< Total sectors on track */
    uint16_t excellent_count;   /**< Sectors with excellent quality */
    uint16_t good_count;        /**< Sectors with good quality */
    uint16_t fair_count;        /**< Sectors with fair quality */
    uint16_t poor_count;        /**< Sectors with poor quality */
    uint16_t bad_count;         /**< Sectors with bad quality */
    
    /* Aggregate confidence */
    float min_confidence;       /**< Minimum sector confidence */
    float max_confidence;       /**< Maximum sector confidence */
    float avg_confidence;       /**< Average sector confidence */
    float std_confidence;       /**< Standard deviation */
    
    /* Quality flags */
    bool has_weak_bits;         /**< Any weak bits on track */
    bool has_crc_errors;        /**< Any CRC errors */
    bool has_anomalies;         /**< Any timing anomalies */
    bool fully_readable;        /**< All sectors readable */
} uft_track_confidence_t;

/**
 * @brief Confidence calculation configuration
 */
typedef struct {
    /* Weight factors */
    float hw_weight;            /**< Hardware confidence weight */
    float dec_weight;           /**< Decoder confidence weight */
    float multirev_weight;      /**< Multi-rev confidence weight */
    float crc_weight;           /**< CRC confidence weight */
    
    /* Thresholds */
    float excellent_threshold;  /**< Threshold for "excellent" */
    float good_threshold;       /**< Threshold for "good" */
    float fair_threshold;       /**< Threshold for "fair" */
    float poor_threshold;       /**< Threshold for "poor" */
    
    /* Options */
    bool require_crc;           /**< Require CRC for high confidence */
    bool boost_multirev;        /**< Boost confidence with multi-rev */
    bool penalize_anomalies;    /**< Reduce confidence for anomalies */
} uft_confidence_config_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/


/**
 * @brief Initialize sector confidence structure
 * @param conf Confidence structure to initialize
 */
void uft_conf_init(uft_sector_confidence_t *conf);

/*===========================================================================
 * Component Confidence Functions
 *===========================================================================*/





/*===========================================================================
 * Combined Confidence Functions
 *===========================================================================*/




/**
 * @brief Get quality description
 * @param quality_level Quality level (0-5)
 * @return Human-readable description
 */
const char *uft_conf_quality_desc(uint8_t quality_level);

/*===========================================================================
 * Track-Level Functions
 *===========================================================================*/




/*===========================================================================
 * Export Functions
 *===========================================================================*/



/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Clamp confidence to valid range
 * @param conf Confidence value
 * @return Clamped value (0-1)
 */
static inline float uft_conf_clamp(float conf)
{
    if (conf < 0.0f) return 0.0f;
    if (conf > 1.0f) return 1.0f;
    return conf;
}

/**
 * @brief Weighted average of confidences
 * @param values Confidence values
 * @param weights Weights (must sum to 1.0)
 * @param count Number of values
 * @return Weighted average
 */
static inline float uft_conf_weighted_avg(const float *values, 
                                           const float *weights, size_t count)
{
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += values[i] * weights[i];
    }
    return uft_conf_clamp(sum);
}

/**
 * @brief Check if confidence meets threshold
 * @param conf Confidence value
 * @param threshold Threshold
 * @return true if confidence >= threshold
 */
static inline bool uft_conf_meets(float conf, float threshold)
{
    return conf >= threshold;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_CONFIDENCE_H */
