# UFI / USBFloppy Firmware-Realistic Emulator (9/9 — the finale)

Ninth and final controller in the `hardware-emulation-author` sequence,
after SCP, Greaseweazle, XUM1541, Applesauce, KryoFlux, FluxEngine,
FC5025 and ADFCopy. **This completes the nine-controller emulation
matrix.**

## What is different about UFI

UFI (USB Floppy Interface, SFF-8070i) is a SCSI-like command set over USB
Mass Storage — a SECTOR device serving LBA sectors via CDBs. It is the
one controller whose C HAL (`src/hal/ufi.c`) exposes an **injectable
backend vtable** (`uft_ufi_ops_t`: open / close / exec_cdb, installed via
`uft_ufi_set_backend`).

So this emulator is the strongest-coupled of the nine: instead of
reimplementing the protocol, it installs a **mock backend** that answers
SCSI CDBs from a synthetic disk, and drives the **production HAL
functions** end-to-end:

```
test → uft_ufi_read_sectors()   [PRODUCTION src/hal/ufi.c]
         builds READ(10) CDB, calls exec_cdb →
       mock_exec_cdb()          [this emulator]
         reads synthetic disk, fills buffer →
       uft_ufi_read_sectors() parses the result → test asserts
```

Every HAL call — `uft_ufi_inquiry`, `uft_ufi_test_unit_ready`,
`uft_ufi_read_capacity`, `uft_ufi_read_sectors`, `uft_ufi_request_sense`
— is a live conformance test of the HAL's CDB construction and response
parsing.

Three layers:

1. **SCSI CDB wire** (production HAL + mock `exec_cdb`) — INQUIRY,
   TEST_UNIT_READY, REQUEST_SENSE, READ_CAPACITY, READ(10), WRITE(10).
2. **Mock device / firmware** (`firmware_state_machine.{h,c}`) — the
   `uft_ufi_ops_t` backend: disk-present gating, contemporaneous sense
   latch (NOT_READY / MEDIUM_ERROR), read-count instrumentation.
3. **Synthetic disk** (`../../flux_gen/ufi/`) — a 1.44 MB image (2880 LBA
   × 512 B) with deterministic sector data, a bad-sector map, and device
   identity strings.

## The forensic core

A READ(10) of a **medium-error** sector returns a non-OK result from the
production HAL, and `uft_ufi_request_sense` reports MEDIUM_ERROR (key
0x03, ASC 0x11). The HAL never fabricates the bad sector's bytes — it
surfaces the error. Good sectors around the bad one still read correctly,
and the error is repeatable. No-disk returns NOT_READY sense (key 0x02,
ASC 0x3A), never a silent empty success.

## Build & run

Registered in `tests/CMakeLists.txt` as `test_ufi_emulator` (links
`src/hal/ufi.c` — the production HAL; no libm, MF-305).

```
ctest -R test_ufi_emulator --output-on-failure
```

Standalone:

```
gcc -Wall -Wextra -I include -I src \
    -I tests/emulators/ufi -I tests/flux_gen/ufi \
    tests/emulators/ufi/test_ufi_emulator.c \
    tests/emulators/ufi/firmware_state_machine.c \
    tests/flux_gen/ufi/flux_gen.c src/hal/ufi.c -o ufi_test && ./ufi_test
```

## Status

**34 assertions, 6 groups (A–F), 0 fail. Deterministic.** Coverage ~85%
aggregate — the highest of the nine, because the HAL parse layer is
production code exercised end-to-end. Bench work: run the platform
backends (`ufi_linux.c` SG_IO + a Windows SCSI_PASS_THROUGH) against a
real drive; widen the mock (UFI-1 sense variants, UFI-4 CDBs, UFI-6
densities). The HAL-parse layer is already production-tested here.

See also: `DIVERGENCES.md`, `coverage_matrix.md`.
