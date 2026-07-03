# UFI Emulator — Divergences from Real Hardware

Forensic-honesty ledger. `SPEC_STATUS: VendorDocumented` — USB Mass
Storage UFI Command Specification Rev. 1.0 (SFF-8070i). UFI is a SCSI-
like command set over USB Mass Storage; the drive is a SECTOR device
(LBA sectors via READ(10)/READ_CAPACITY/INQUIRY CDBs).

**This is the strongest-coupled emulator of the nine.** UFI's C HAL
(`src/hal/ufi.c`) exposes an injectable backend vtable
(`uft_ufi_ops_t`), so the emulator installs a mock backend and drives
the REAL HAL functions end-to-end. Most of the "divergences" other
controllers list (protocol reconstruction, byte framing) do not apply
here — the HAL's CDB building and response parsing are the production
code, exercised directly. The divergences are only in the mock device's
behaviour.

Severity: **HIGH** could mask a bug / break on real hardware · **MEDIUM**
plausible but unverified · **LOW** cosmetic / tolerance.

| ID | Sev | Divergence |
|----|-----|------------|
| **UFI-1** | MEDIUM | Sense semantics: the mock latches a single contemporaneous sense (key/ASC/ASCQ) set by a failing command and cleared by REQUEST SENSE, per the basic SCSI model. Real UFI drives may use fixed vs descriptor sense format, deferred sense, or unit-attention conditions the mock does not model. The key/ASC pairs used (NOT_READY 0x02/0x3A, MEDIUM_ERROR 0x03/0x11) are the standard ones the forensic path cares about. |
| **UFI-2** | MEDIUM | The mock READ(10) fails an entire command on the first bad LBA in the range (returns error + medium-error sense). A real drive may return partial data up to the bad sector, or retry internally. What is faithful — and forensically load-bearing — is that the HAL surfaces a non-OK result and does NOT fabricate the bad sector's bytes. |
| **UFI-3** | MEDIUM | WRITE(10) is accepted by the mock but not persisted (no readback). UFI floppies are genuinely read/write; UFT's forensic path is read-centric. The emulator models write acceptance for CDB coverage but does not model write-back verification. |
| **UFI-4** | LOW | Only the CDBs the HAL issues are handled (TEST_UNIT_READY, INQUIRY, REQUEST_SENSE, READ_CAPACITY, READ(10), WRITE(10)). FORMAT_UNIT, MODE_SENSE, READ_FORMAT_CAPACITIES, VERIFY(10), START_STOP are defined in the opcode table but not modelled by the mock (the HAL's format-capacities/verify paths are exercised less in the forensic read flow). |
| **UFI-5** | LOW | INQUIRY returns a fixed identity (vendor "UFTEMU", product "USB FLOPPY DRIVE", rev "1.0"). Real drives report their true vendor/product; the emulator's point is that the HAL parses the fixed-width fields correctly (bytes 8/16/32), not that the identity is realistic. |
| **UFI-6** | LOW | Geometry is fixed at 1.44 MB (2880 LBA × 512 B). Real UFI drives support multiple densities (720K, 1.2M, 2.88M) reported via READ_FORMAT_CAPACITIES; the mock models the single most-common geometry. |

## What is NOT divergent (this is production code)

- **The HAL's CDB construction and response parsing are the production
  functions in `src/hal/ufi.c`**, driven end-to-end. INQUIRY field
  offsets, READ_CAPACITY big-endian decoding, READ(10) LBA encoding,
  REQUEST_SENSE key/ASC/ASCQ extraction — all exercised against the mock
  and asserted.
- The forensic invariant holds and is tested: a medium-error sector
  returns a non-OK rc and REQUEST_SENSE reports MEDIUM_ERROR — the HAL
  never fabricates the bad sector's data (group E).
- No-disk returns NOT_READY sense, never a silent empty success.
- The backend-not-set guard returns UFT_ERR_NOT_IMPLEMENTED (the HAL's
  own honest-stub behaviour when no platform backend is registered).
- Determinism: same seed → identical disk image.

## Bench-verification gate (M3.4 UFI)

The HAL itself is production and exercised here; the bench work is to
confirm the platform backends (`ufi_linux.c` SG_IO, a Windows
SCSI_PASS_THROUGH backend) drive a REAL UFI drive, and to widen the mock
gaps (UFI-1 sense variants, UFI-4 unmodelled CDBs, UFI-6 multi-density)
against actual devices. Until a real drive is benched: SIMULATED
(FIRMWARE-REALISTIC) for the backend, though the HAL-parse layer is
already production-tested by this harness.
