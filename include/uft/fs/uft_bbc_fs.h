/**
 * @file uft_bbc_fs.h
 * @brief BBC Micro DFS/ADFS Filesystem Support
 * 
 * Supports:
 * - Acorn DFS (Disc Filing System) - 40/80 track, single/double sided
 * - Acorn ADFS (Advanced Disc Filing System) - various formats
 * - Opus DDOS/EDOS variants
 * - Watford DDFS
 * 
 * DFS Format (Catalog in tracks 0/1):
 * - Sector 0: 8 filename entries (8 chars each)
 * - Sector 1: Directory info, disk title, file metadata
 * - File entries: load/exec addr, length, start sector
 * 
 * ADFS Format:
 * - Hierarchical directory structure
 * - Fragment map for allocation
 * - Multiple directory formats (Old/New/Big/+)
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Filesystem layer
 *
 * This header declares 22 public functions; 20 are NOT implemented
 * in the source tree (only 2 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_BBC_FS_H
#define UFT_BBC_FS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * DFS Constants
 *===========================================================================*/

/** DFS sector size */
#define UFT_DFS_SECTOR_SIZE     256

/** Maximum files in DFS catalog */
#define UFT_DFS_MAX_FILES       31

/** DFS filename length */
#define UFT_DFS_NAME_LEN        7

/** DFS directory character position */
#define UFT_DFS_DIR_CHAR        7

/*===========================================================================
 * DFS Boot Options
 *===========================================================================*/

#ifndef UFT_DFS_BOOT_T_DEFINED
#define UFT_DFS_BOOT_T_DEFINED
typedef enum {
    UFT_DFS_BOOT_NONE = 0,      /**< No boot action */
    UFT_DFS_BOOT_LOAD = 1,      /**< *LOAD !BOOT */
    UFT_DFS_BOOT_RUN = 2,       /**< *RUN !BOOT */
    UFT_DFS_BOOT_EXEC = 3       /**< *EXEC !BOOT */
} uft_dfs_boot_t;
#endif /* UFT_DFS_BOOT_T_DEFINED */

/*===========================================================================
 * DFS File Entry
 *===========================================================================*/

/**
 * @brief DFS file entry
 */
typedef struct {
    char        name[UFT_DFS_NAME_LEN + 1];  /**< Filename (null-terminated) */
    char        dir;                          /**< Directory character ($, !, etc) */
    uint32_t    load_addr;                    /**< Load address */
    uint32_t    exec_addr;                    /**< Execution address */
    uint32_t    length;                       /**< File length in bytes */
    uint16_t    start_sector;                 /**< Start sector on disk */
    bool        locked;                       /**< File locked flag */
} uft_dfs_file_t;

/*===========================================================================
 * DFS Disk Info
 *===========================================================================*/

/**
 * @brief DFS disk information
 */
typedef struct {
    char        title[13];          /**< Disk title (12 chars max) */
    uint8_t     sequence;           /**< Sequence number */
    uint8_t     boot_option;        /**< Boot option (0-3) */
    uint16_t    num_sectors;        /**< Total sectors on disk */
    uint8_t     num_files;          /**< Number of files in catalog */
    bool        double_sided;       /**< Double-sided disk flag */
    uint8_t     tracks;             /**< Number of tracks (40 or 80) */
    
    uft_dfs_file_t files[UFT_DFS_MAX_FILES];
} uft_dfs_info_t;

/*===========================================================================
 * ADFS Constants
 *===========================================================================*/

/** ADFS sector size (standard) */
#define UFT_ADFS_SECTOR_SIZE    256

/** ADFS big sector size */
#define UFT_ADFS_BIG_SECTOR     1024

/** ADFS directory entry size */
#define UFT_ADFS_DIRENTRY_SIZE  26

/** ADFS filename max length */
#define UFT_ADFS_NAME_LEN       10

/*===========================================================================
 * ADFS Directory Types
 *===========================================================================*/

typedef enum {
    UFT_ADFS_DIR_OLD = 0,       /**< Old directory format */
    UFT_ADFS_DIR_NEW,           /**< New directory format */
    UFT_ADFS_DIR_BIG,           /**< Big directory format */
    UFT_ADFS_DIR_PLUS           /**< ADFS+ directory format */
} uft_adfs_dir_type_t;

/*===========================================================================
 * ADFS File Attributes
 *===========================================================================*/

typedef enum {
    UFT_ADFS_ATTR_R = 0x01,     /**< Owner read */
    UFT_ADFS_ATTR_W = 0x02,     /**< Owner write */
    UFT_ADFS_ATTR_L = 0x04,     /**< Locked */
    UFT_ADFS_ATTR_D = 0x08,     /**< Directory */
    UFT_ADFS_ATTR_E = 0x10,     /**< Execute only */
    UFT_ADFS_ATTR_r = 0x20,     /**< Public read */
    UFT_ADFS_ATTR_w = 0x40,     /**< Public write */
    UFT_ADFS_ATTR_e = 0x80      /**< Public execute */
} uft_adfs_attr_t;

/*===========================================================================
 * ADFS File Entry
 *===========================================================================*/

/**
 * @brief ADFS file/directory entry
 */
typedef struct {
    char        name[UFT_ADFS_NAME_LEN + 1];  /**< Filename */
    uint32_t    load_addr;                     /**< Load address */
    uint32_t    exec_addr;                     /**< Execution address */
    uint32_t    length;                        /**< File length */
    uint32_t    sector;                        /**< Start sector/fragment */
    uint8_t     attributes;                    /**< File attributes */
    bool        is_directory;                  /**< True if directory */
} uft_adfs_entry_t;

/*===========================================================================
 * ADFS Disk Info
 *===========================================================================*/

/**
 * @brief ADFS disk information
 */
typedef struct {
    char        name[11];           /**< Disk name */
    uint8_t     dir_type;           /**< Directory format type */
    uint32_t    total_sectors;      /**< Total sectors */
    uint32_t    free_sectors;       /**< Free sectors */
    uint16_t    sector_size;        /**< Bytes per sector */
    uint8_t     log2_sector;        /**< Log2 of sector size */
    uint8_t     zones;              /**< Number of allocation zones */
    uint16_t    zone_bits;          /**< Bits per zone */
    uint32_t    root_dir;           /**< Root directory address */
    uint32_t    boot_option;        /**< Boot option */
} uft_adfs_info_t;

/*===========================================================================
 * Format Variants
 *===========================================================================*/

/**
 * @brief BBC disk format variant
 */
typedef enum {
    /* DFS variants */
    UFT_BBC_DFS_40T_SS = 0,     /**< 40 track, single sided (100KB) */
    UFT_BBC_DFS_80T_SS,         /**< 80 track, single sided (200KB) */
    UFT_BBC_DFS_40T_DS,         /**< 40 track, double sided (200KB) */
    UFT_BBC_DFS_80T_DS,         /**< 80 track, double sided (400KB) */
    
    /* ADFS variants */
    UFT_BBC_ADFS_S,             /**< ADFS S format (160KB) */
    UFT_BBC_ADFS_M,             /**< ADFS M format (320KB) */
    UFT_BBC_ADFS_L,             /**< ADFS L format (640KB) */
    UFT_BBC_ADFS_D,             /**< ADFS D format (800KB) */
    UFT_BBC_ADFS_E,             /**< ADFS E format (800KB) */
    UFT_BBC_ADFS_F,             /**< ADFS F format (1600KB) */
    UFT_BBC_ADFS_G,             /**< ADFS G format (3200KB HD) */
    
    /* Opus variants */
    UFT_BBC_DDOS_40T,           /**< Opus DDOS 40 track (180KB) */
    UFT_BBC_DDOS_80T,           /**< Opus DDOS 80 track (360KB) */
    UFT_BBC_EDOS,               /**< Opus EDOS (various) */
    
    /* Watford */
    UFT_BBC_WATFORD_DDFS,       /**< Watford DDFS (double density) */
    
    UFT_BBC_FORMAT_COUNT
} uft_bbc_format_t;

/*===========================================================================
 * API Functions - Detection
 *===========================================================================*/


/**
 * @brief Get format name
 * @param format Format ID
 * @return Format name string
 */
const char *uft_bbc_format_name(uft_bbc_format_t format);



/*===========================================================================
 * API Functions - DFS
 *===========================================================================*/


/**
 * @brief Find file in DFS catalog
 * @param info Disk information
 * @param name Filename to find
 * @param dir Directory character ('$' for default)
 * @return Pointer to file entry or NULL
 */
const uft_dfs_file_t *uft_dfs_find_file(const uft_dfs_info_t *info,
                                         const char *name, char dir);

/**
 * @brief Extract DFS file
 * @param data Disk image data
 * @param size Data size  
 * @param file File entry to extract
 * @param buffer Output buffer
 * @param buf_size Buffer size
 * @return Bytes extracted, or -1 on error
 */
int uft_dfs_extract_file(const uint8_t *data, size_t size,
                          const uft_dfs_file_t *file,
                          uint8_t *buffer, size_t buf_size);


/**
 * @brief Add file to DFS disk image
 * @param data Disk image (modified in place)
 * @param size Image size
 * @param file File entry (start_sector filled in on success)
 * @param file_data File content
 * @return 0 on success, -1 on error
 */
int uft_dfs_add_file(uint8_t *data, size_t size,
                      uft_dfs_file_t *file,
                      const uint8_t *file_data);



/*===========================================================================
 * API Functions - ADFS
 *===========================================================================*/






/*===========================================================================
 * API Functions - Conversion
 *===========================================================================*/





/*===========================================================================
 * Utility Functions
 *===========================================================================*/





#ifdef __cplusplus
}
#endif

#endif /* UFT_BBC_FS_H */
