/**
 * @file uft_fat_badblock.h
 * @brief FAT Bad Block/Sector Management
 * @version 1.0.0
 * 
 * Bad block handling for FAT filesystems:
 * - Import bad block lists (dosfstools format)
 * - Export bad block lists
 * - Mark/unmark clusters as bad
 * - Surface scan integration
 * - Bad sector remapping
 * 
 * File format compatible with:
 * - dosfstools badblocks (-l option)
 * - e2fsck/badblocks utility output
 * - Custom sector lists
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PLANNED
 * PLANNED FEATURE — Filesystem layer
 *
 * This header declares 23 public functions, of which 23 have no
 * implementation in the source tree. Callers exist but will link-fail or
 * silently no-op until the feature is implemented.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 DOCUMENT-Welle).
 * Do NOT add new call sites to functions from this header without first
 * implementing them or removing the prototype.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_FAT_BADBLOCK_H
#define UFT_FAT_BADBLOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "uft/fs/uft_fat12.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum bad blocks in list */
#define UFT_BADBLOCK_MAX_ENTRIES    65536

/** Bad block list file magic (optional header) */
#define UFT_BADBLOCK_MAGIC          "BADBLK01"

/*===========================================================================
 * Bad Block Entry Types
 *===========================================================================*/

/**
 * @brief Bad block entry unit type
 */
typedef enum {
    UFT_BADBLOCK_SECTOR = 0,        /**< Entry is sector number */
    UFT_BADBLOCK_CLUSTER,           /**< Entry is cluster number */
    UFT_BADBLOCK_BYTE_OFFSET,       /**< Entry is byte offset */
    UFT_BADBLOCK_BLOCK_1K           /**< Entry is 1KB block (mkfs.fat) */
} uft_badblock_unit_t;

/**
 * @brief Bad block source
 */
typedef enum {
    UFT_BADBLOCK_SRC_MANUAL = 0,    /**< Manually added */
    UFT_BADBLOCK_SRC_FILE,          /**< Imported from file */
    UFT_BADBLOCK_SRC_SCAN,          /**< Found by surface scan */
    UFT_BADBLOCK_SRC_FAT            /**< Read from FAT table */
} uft_badblock_source_t;

/*===========================================================================
 * Bad Block Entry
 *===========================================================================*/

/**
 * @brief Single bad block entry
 */
typedef struct {
    uint64_t location;              /**< Location (interpretation depends on unit) */
    uft_badblock_unit_t unit;       /**< Unit type */
    uft_badblock_source_t source;   /**< How this was detected */
    uint32_t cluster;               /**< Corresponding cluster (if known) */
    bool marked_in_fat;             /**< Already marked in FAT */
} uft_badblock_entry_t;

/**
 * @brief Bad block list
 */
typedef struct {
    uft_badblock_entry_t *entries;  /**< Array of entries */
    size_t count;                   /**< Number of entries */
    size_t capacity;                /**< Allocated capacity */
    uft_badblock_unit_t default_unit; /**< Default unit for new entries */
} uft_badblock_list_t;

/*===========================================================================
 * Bad Block Statistics
 *===========================================================================*/

/**
 * @brief Bad block analysis results
 */
typedef struct {
    size_t total_bad;               /**< Total bad entries */
    size_t in_data_area;            /**< Bad blocks in data area */
    size_t in_reserved;             /**< Bad blocks in reserved area */
    size_t in_fat;                  /**< Bad blocks in FAT area */
    size_t in_root_dir;             /**< Bad blocks in root directory */
    size_t already_marked;          /**< Already marked in FAT */
    size_t needs_marking;           /**< Not yet marked in FAT */
    uint64_t bytes_affected;        /**< Total bytes in bad areas */
    uint32_t clusters_affected;     /**< Total clusters affected */
} uft_badblock_stats_t;

/*===========================================================================
 * API - List Management
 *===========================================================================*/

/**
 * @brief Create empty bad block list
 * @return New list or NULL
 */
uft_badblock_list_t *uft_badblock_list_create(void);

/**
 * @brief Destroy bad block list
 * @param list List to destroy
 */
void uft_badblock_list_destroy(uft_badblock_list_t *list);

/**
 * @brief Clear all entries from list
 * @param list List to clear
 */
void uft_badblock_list_clear(uft_badblock_list_t *list);

/**
 * @brief Add entry to list
 * @param list Target list
 * @param location Bad location
 * @param unit Location unit type
 * @return 0 on success
 */
int uft_badblock_list_add(uft_badblock_list_t *list, uint64_t location,
                          uft_badblock_unit_t unit);



/**
 * @brief Sort list by location
 * @param list List to sort
 */
void uft_badblock_list_sort(uft_badblock_list_t *list);

/**
 * @brief Remove duplicate entries
 * @param list List to deduplicate
 * @return Number of duplicates removed
 */
size_t uft_badblock_list_dedupe(uft_badblock_list_t *list);

/*===========================================================================
 * API - File Import/Export
 *===========================================================================*/



/**
 * @brief Import from string buffer
 * @param list Target list
 * @param data String data (newline separated)
 * @param size Data size
 * @param unit Unit type
 * @return 0 on success
 */
int uft_badblock_import_buffer(uft_badblock_list_t *list, const char *data,
                               size_t size, uft_badblock_unit_t unit);



/*===========================================================================
 * API - FAT Integration
 *===========================================================================*/





/*===========================================================================
 * API - Conversion
 *===========================================================================*/





/*===========================================================================
 * API - Utilities
 *===========================================================================*/


/**
 * @brief Get string name for unit type
 * @param unit Unit type
 * @return String name
 */
const char *uft_badblock_unit_str(uft_badblock_unit_t unit);

/**
 * @brief Get string name for source type
 * @param source Source type
 * @return String name
 */
const char *uft_badblock_source_str(uft_badblock_source_t source);



#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_BADBLOCK_H */
