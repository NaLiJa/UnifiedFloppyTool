/**
 * @file uft_jv3.h
 * @brief JV3 Container Format for TRS-80
 * 
 * EXT3-019: JV3 disk image format support
 * 
 * JV3 is a TRS-80 disk image format that stores:
 * - Variable sector sizes (128, 256, 512, 1024 bytes)
 * - FM and MFM encoded sectors
 * - Sector header information
 * - DAM types (normal, deleted, undefined)
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Format plugins
 *
 * This header declares 15 public functions; 12 are NOT implemented
 * in the source tree (only 3 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_JV3_H
#define UFT_JV3_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_JV3_HEADER_SIZE         2901    /**< Sector info header size */
#define UFT_JV3_MAX_SECTORS         2901    /**< Max sectors in header */
#define UFT_JV3_SECTORS_PER_ENTRY   3       /**< Bytes per sector entry */

/* Sector flags (byte 2 of sector header entry) */
#define UFT_JV3_DENSITY_MASK        0x80    /**< 0=FM, 1=MFM */
#define UFT_JV3_DAM_MASK            0x60    /**< Data Address Mark */
#define UFT_JV3_SIDE_MASK           0x10    /**< Side bit */
#define UFT_JV3_CRC_MASK            0x08    /**< CRC error flag */
#define UFT_JV3_SIZE_MASK           0x03    /**< Sector size code */

/* DAM values (after masking with 0x60) */
#define UFT_JV3_DAM_NORMAL_FB       0x00    /**< FB - Normal data */
#define UFT_JV3_DAM_NORMAL_FA       0x20    /**< FA - Normal data (alt) */
#define UFT_JV3_DAM_DELETED_F8      0x40    /**< F8 - Deleted data */
#define UFT_JV3_DAM_DELETED_F9      0x60    /**< F9 - Deleted data (alt) */

/* Size codes */
#define UFT_JV3_SIZE_256            0x00    /**< 256 bytes */
#define UFT_JV3_SIZE_128            0x01    /**< 128 bytes */
#define UFT_JV3_SIZE_1024           0x02    /**< 1024 bytes */
#define UFT_JV3_SIZE_512            0x03    /**< 512 bytes */

/* Special marker */
#define UFT_JV3_FREE_ENTRY          0xFF    /**< Free/unused sector entry */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief JV3 sector header entry
 */
typedef struct {
    uint8_t track;                  /**< Track number */
    uint8_t sector;                 /**< Sector number */
    uint8_t flags;                  /**< Flags byte */
} uft_jv3_sector_entry_t;

/**
 * @brief Parsed sector info
 */
typedef struct {
    uint8_t track;
    uint8_t sector;
    uint8_t side;
    uint16_t size;                  /**< Sector size in bytes */
    bool is_mfm;                    /**< MFM encoding (vs FM) */
    bool is_deleted;                /**< Deleted data mark */
    bool has_crc_error;             /**< CRC error flag */
    uint8_t dam_type;               /**< Original DAM byte */
    
    size_t data_offset;             /**< Offset to sector data */
} uft_jv3_sector_info_t;

/**
 * @brief JV3 file context
 */
typedef struct {
    const uint8_t *data;
    size_t size;
    
    /* Header info */
    uft_jv3_sector_entry_t entries[UFT_JV3_MAX_SECTORS];
    int entry_count;
    
    /* Disk geometry (detected) */
    uint8_t max_track;
    uint8_t max_sector;
    uint8_t sides;
    bool has_mfm;
    bool has_fm;
    
    /* Statistics */
    int total_sectors;
    int fm_sectors;
    int mfm_sectors;
    int deleted_sectors;
    int crc_errors;
} uft_jv3_ctx_t;

/**
 * @brief Write buffer for creating JV3 files
 */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t used;
    
    uft_jv3_sector_entry_t entries[UFT_JV3_MAX_SECTORS];
    int entry_count;
    size_t data_offset;
} uft_jv3_writer_t;

/*===========================================================================
 * Function Prototypes - Reading
 *===========================================================================*/

/**
 * @brief Check if data is JV3 format
 */
bool uft_jv3_detect(const uint8_t *data, size_t size);




/**
 * @brief Find sector by track/sector/side
 */
int uft_jv3_find_sector(const uft_jv3_ctx_t *ctx, 
                        uint8_t track, uint8_t sector, uint8_t side,
                        uft_jv3_sector_info_t *info);

/**
 * @brief Read sector data
 */
int uft_jv3_read_sector(const uft_jv3_ctx_t *ctx,
                        uint8_t track, uint8_t sector, uint8_t side,
                        uint8_t *buffer, size_t *size);


/*===========================================================================
 * Function Prototypes - Writing
 *===========================================================================*/

/**
 * @brief Create JV3 writer
 */
uft_jv3_writer_t *uft_jv3_writer_create(size_t initial_capacity);




/*===========================================================================
 * Function Prototypes - Conversion
 *===========================================================================*/



/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/




#ifdef __cplusplus
}
#endif

#endif /* UFT_JV3_H */
