/**
 * @file tests/emulators/ufi/firmware_state_machine.h
 * @brief Mock UFI backend — implements the production HAL's uft_ufi_ops_t.
 *
 * UFI is the one controller whose C HAL (src/hal/ufi.c) exposes an
 * INJECTABLE backend vtable (uft_ufi_ops_t: open/close/exec_cdb, set via
 * uft_ufi_set_backend). So instead of reimplementing the protocol, this
 * "firmware state machine" IS a mock backend that answers SCSI CDBs from
 * a synthetic disk, and the test drives the REAL HAL functions
 * (uft_ufi_inquiry, uft_ufi_test_unit_ready, uft_ufi_read_capacity,
 * uft_ufi_read_sectors, uft_ufi_request_sense) end-to-end against it.
 * That makes the emulator an end-to-end conformance harness for the UFI
 * HAL's CDB construction and response parsing — the strongest coupling
 * to production code of the nine controllers.
 *
 * SPEC_STATUS: VendorDocumented — USB Mass Storage UFI Command
 * Specification Rev. 1.0 (SFF-8070i). CDB opcodes and INQUIRY/
 * READ_CAPACITY/REQUEST_SENSE response layouts are per that spec.
 *
 * Forensic invariants:
 *   - No fabricated data on error: a READ(10) of a medium-error sector
 *     returns a non-OK rc and the HAL surfaces it; REQUEST_SENSE reports
 *     MEDIUM_ERROR (key 3, ASC 0x11). The sector bytes are never faked
 *     into a "clean" read.
 *   - No-disk (TEST_UNIT_READY / READ) returns NOT_READY sense, never a
 *     silent empty success.
 */
#ifndef UFT_TESTS_UFI_MOCK_BACKEND_H
#define UFT_TESTS_UFI_MOCK_BACKEND_H

#include "flux_gen.h"
#include "uft/hal/ufi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Install the mock backend and point it at `disk`. Registers the vtable
 *  via uft_ufi_set_backend(). The disk must outlive the test. Passing
 *  NULL clears the backend (uft_ufi_set_backend(NULL)). */
void ufi_mock_install(const uft_ufi_disk_t *disk);

/** Number of exec_cdb calls since install (for read-count assertions). */
uint64_t ufi_mock_cdb_count(void);

/** Number of READ(10) CDBs seen. */
uint64_t ufi_mock_read_count(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TESTS_UFI_MOCK_BACKEND_H */
