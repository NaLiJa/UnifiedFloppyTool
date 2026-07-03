# UFI Emulator — Coverage Matrix

Aggregate ~85% — the highest of the nine, because UFI drives the
PRODUCTION HAL end-to-end via an injected backend rather than
reimplementing the protocol. The uncovered part is the platform backends
(SG_IO / SCSI_PASS_THROUGH) against real hardware and the unmodelled
CDBs.

Legend: ✓ modelled & tested · ~ partial · ✗ not modelled · n/a out of scope

## Layer 1 — SCSI CDB wire (production HAL + mock backend)

| Aspect | Status | Note |
|--------|--------|------|
| INQUIRY (0x12) build + parse | ✓ | production HAL, field offsets asserted |
| TEST_UNIT_READY (0x00) | ✓ | |
| REQUEST_SENSE (0x03) key/ASC/ASCQ | ✓ | production HAL parse |
| READ_CAPACITY (0x25) BE decode | ✓ | production HAL, 2880/512 |
| READ(10) (0x28) LBA encode + data | ✓ | production HAL, round-trip |
| WRITE(10) (0x2A) | ~ | UFI-3 — accepted, not persisted |
| FORMAT_UNIT / MODE_SENSE / VERIFY | ✗ | UFI-4 — not in the forensic read flow |
| READ_FORMAT_CAPACITIES multi-density | ~ | UFI-6 — single geometry |

## Layer 2 — device / firmware behaviour (mock backend)

| Aspect | Status | Note |
|--------|--------|------|
| Backend vtable install (uft_ufi_set_backend) | ✓ | |
| Backend-not-set → NOT_IMPLEMENTED | ✓ | HAL honest-stub guard |
| Disk-present gating | ✓ | |
| Contemporaneous sense latch/clear | ✓ | UFI-1 — basic SCSI model |
| Medium-error on bad LBA | ✓ | surfaced, no fabrication |
| No-disk → NOT_READY sense | ✓ | |
| Partial-data-before-error | ~ | UFI-2 — whole-command fail modelled |
| Deferred / unit-attention sense | ✗ | UFI-1 |

## Layer 3 — synthetic disk generator

| Aspect | Status | Note |
|--------|--------|------|
| 1.44 MB geometry (2880 × 512) | ✓ | |
| Deterministic sector payloads | ✓ | seeded xorshift64* |
| Bad-sector map (medium error) | ✓ | |
| Device identity strings | ✓ | fixed-width, HAL-parsed |
| lba_ok range/bad helper | ✓ | |
| Multi-density images | ✗ | UFI-6 |
| Flux capture | n/a | UFI is sector-domain |

## Test summary

`test_ufi_emulator.c`: **34 assertions, 6 groups (A–F), 0 fail.**
Deterministic across runs. Every HAL call (inquiry, test_unit_ready,
read_capacity, read_sectors, request_sense) is exercised against the
production `src/hal/ufi.c`. Sibling emulators unaffected (SCP 37, GW 37,
XUM1541 56, Applesauce 111, KryoFlux 51, FluxEngine 35, FC5025 39,
ADFCopy 48).

## Bench-verification gate (M3.4 UFI)

The HAL parse layer is production-tested by this harness. Bench work: run
the platform backends (`ufi_linux.c` SG_IO + a Windows backend) against a
real UFI drive, and widen the mock (UFI-1 sense variants, UFI-4 CDBs,
UFI-6 densities). Backend against silicon = bench-only; HAL parsing =
already covered.
