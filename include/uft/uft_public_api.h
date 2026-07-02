/**
 * @file uft_public_api.h
 * @brief Unified Public API for UFT
 * 
 * P1-H05: Single-include header for all public UFT functionality
 * 
 * USAGE:
 *   #include <uft/uft_public_api.h>
 * 
 * This header provides stable, versioned APIs that:
 * - Are guaranteed to be backward compatible within major versions
 * - Have well-defined ownership semantics
 * - Use consistent error handling
 * - Are suitable for GUI, CLI, and library consumers
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Root-level API
 *
 * This header declares 32 public functions; 29 are NOT implemented
 * in the source tree (only 3 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_PUBLIC_API_H
#define UFT_PUBLIC_API_H

/* API Version */
#define UFT_API_VERSION_MAJOR 1
#define UFT_API_VERSION_MINOR 0
#define UFT_API_VERSION_PATCH 0
#define UFT_API_VERSION ((UFT_API_VERSION_MAJOR << 16) | \
                         (UFT_API_VERSION_MINOR << 8) | \
                         UFT_API_VERSION_PATCH)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SECTION 1: CORE TYPES
 * ============================================================================ */

/* Error codes - canonical source */
#include "uft/core/uft_unified_types.h"

/* Forward declarations for opaque types
 * Note: uft_disk_image_t, uft_track_t, uft_sector_t are defined in uft_unified_types.h
 */
#ifndef UFT_CONTEXT_T_DEFINED
#define UFT_CONTEXT_T_DEFINED
typedef struct uft_context uft_context_t;
#endif

/* ============================================================================
 * SECTION 2: INITIALIZATION
 * ============================================================================ */





/* ============================================================================
 * SECTION 3: FORMAT DETECTION & INFO
 * ============================================================================ */

/**
 * @brief Detect format of a file
 * @param path File path
 * @param out_format Output: detected format ID
 * @param out_confidence Output: confidence 0-100 (optional)
 * @return UFT_OK on success
 */
uft_error_t uft_detect_format(const char *path,
                              uft_format_id_t *out_format,
                              uint8_t *out_confidence);



/**
 * @brief Get format capabilities
 */
typedef struct {
    uft_format_id_t format;
    const char *name;
    const char *extension;
    
    bool can_read;
    bool can_write;
    bool can_repair;
    
    bool supports_timing;
    bool supports_weak_bits;
    bool supports_long_tracks;
    bool supports_error_map;
    bool supports_multi_rev;
    
} uft_format_info_t;



/* ============================================================================
 * SECTION 4: DISK IMAGE I/O
 * ============================================================================ */

/**
 * @brief Read options
 */
typedef struct {
    bool analyze;              /**< Run analysis after read */
    bool detect_protection;    /**< Detect copy protection */
    bool preserve_errors;      /**< Keep error information */
    uint8_t max_retries;       /**< Retry count for hardware */
} uft_read_options_t;




/**
 * @brief Write options
 */
typedef struct {
    uft_format_id_t format;    /**< Output format (0 = same as input) */
    bool verify;               /**< Verify after write */
    bool preserve_errors;      /**< Preserve error flags */
    bool compress;             /**< Use compression if available */
} uft_write_options_t;




/* ============================================================================
 * SECTION 5: DISK IMAGE ACCESS
 * ============================================================================ */

/**
 * @brief Get disk geometry
 */
#ifndef UFT_GEOMETRY_T_DEFINED
#define UFT_GEOMETRY_T_DEFINED
typedef struct {
    uint16_t tracks;
    uint8_t heads;
    uint8_t sectors_per_track;  /* 0 = variable */
    uint16_t bytes_per_sector;  /* 0 = variable */
    uft_format_id_t format;
} uft_geometry_t;
#endif /* UFT_GEOMETRY_T_DEFINED */





/* ============================================================================
 * SECTION 6: ANALYSIS & DIAGNOSTICS
 * ============================================================================ */

/**
 * @brief Disk analysis result
 */
typedef struct {
    bool success;
    
    /* Geometry */
    uft_geometry_t geometry;
    
    /* Quality metrics */
    uint16_t total_sectors;
    uint16_t valid_sectors;
    uint16_t error_sectors;
    float quality_percent;
    
    /* Errors */
    uint16_t crc_errors;
    uint16_t missing_sectors;
    uint16_t weak_bit_sectors;
    
    /* Protection */
    bool has_protection;
    uft_protection_t protection_type;
    uint8_t protection_confidence;
    
    /* Filesystem */
    bool has_filesystem;
    char filesystem_type[32];
    char volume_name[64];
    
} uft_analysis_t;


/**
 * @brief Get diagnostic info for track
 */
typedef struct {
    uint16_t track;
    uint8_t head;
    
    uint8_t sectors_found;
    uint8_t sectors_valid;
    uint8_t encoding;
    uint8_t quality;
    
    bool has_errors;
    bool has_weak_bits;
    bool has_protection;
    
    const char *diagnosis;  /* Human-readable diagnosis */
    
} uft_track_diag_t;


/* ============================================================================
 * SECTION 7: CONVERSION
 * ============================================================================ */



/* ============================================================================
 * SECTION 8: COPY & RECOVERY
 * ============================================================================ */

/**
 * @brief Copy options
 */
typedef struct {
    bool preserve_protection;  /**< Preserve copy protection */
    bool preserve_timing;      /**< Preserve timing data */
    bool preserve_weak_bits;   /**< Preserve weak bits */
    bool use_multi_rev;        /**< Use multi-revision reads */
    uint8_t max_retries;       /**< Maximum retries */
    uint8_t min_confidence;    /**< Minimum confidence threshold */
} uft_copy_options_t;



/**
 * @brief Recovery options
 */
typedef struct {
    enum {
        UFT_RECOVERY_SAFE,       /**< Safe: only use verified data */
        UFT_RECOVERY_AGGRESSIVE, /**< Aggressive: interpolate missing */
        UFT_RECOVERY_FORENSIC    /**< Forensic: preserve all with metadata */
    } mode;
    
    uint8_t max_retries;
    bool use_multi_rev;
    float confidence_threshold;
    
} uft_recovery_options_t;



/* ============================================================================
 * SECTION 9: PROGRESS & CALLBACKS
 * ============================================================================ */

/**
 * @brief Progress callback
 * @param current Current step
 * @param total Total steps
 * @param message Status message
 * @param user_data User data
 * @return true to continue, false to cancel
 */
typedef bool (*uft_progress_fn)(int current, int total,
                                const char *message,
                                void *user_data);

/**
 * @brief Error callback
 */
typedef void (*uft_error_fn)(uft_error_t error,
                             const char *message,
                             void *user_data);



/* ============================================================================
 * SECTION 10: MEMORY MANAGEMENT
 * ============================================================================ */

/**
 * @brief Free disk image
 * @param disk Disk to free (NULL safe)
 */
void uft_disk_free(uft_disk_image_t *disk);



#ifdef __cplusplus
}
#endif

#endif /* UFT_PUBLIC_API_H */
