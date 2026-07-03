# KryoFlux Emulator — Coverage Matrix

What the emulator covers versus real hardware. Aggregate ~75%. The
distinguishing strength: the flux layer is validated against the
PRODUCTION parser `uft_kf_decode()`, so its coverage is real conformance,
not self-consistency. The gaps are the K-1 exit-code vocabulary and K-2
byte-capture questions that only a bench can close.

Legend: ✓ modelled & tested · ~ partial · ✗ not modelled · n/a out of scope

## Layer 1 — DTC invocation contract (the "wire")

| Aspect | Status | Note |
|--------|--------|------|
| Device detection (`dtc -i`) | ✓ | count + READY transition |
| Capture request (track/side/revs) | ✓ | range-validated like the HAL |
| Exit code OK / NO_DEVICE / NO_DISK | ✓ | |
| Exit code NO_INDEX / BAD_ARGS / IO | ✓ | |
| Write refusal exit code | ✓ | K-8, always denied |
| Real DTC exit-code vocabulary | ~ | K-1 — modelled, bench-gated |
| DTC stdout text parsing | ✗ | HAL parses text; not modelled here |
| Double-step `-k2` | ~ | K-7 — accepted, no stream effect |

## Layer 2 — DTC + device state machine

| Aspect | Status | Note |
|--------|--------|------|
| DISCONNECTED→READY→CAPTURING→ERROR | ✓ | |
| No-device / no-disk / no-index faults | ✓ | fail empty, never short read |
| Retry model (`-t` count) | ✓ | K-5 — soft-fault retry, hard fails fast |
| Retry exhaustion → IO error | ✓ | |
| Soft vs hard fault classification | ~ | K-5 — simplified |
| Board firmware version reporting | ✗ | K-6 |

## Layer 3 — KryoFlux RAW stream generator

| Aspect | Status | Note |
|--------|--------|------|
| Flux1 (0x0E-0xFF) encoding | ✓ | white-box boundary test |
| Flux2 (0x00-0x07) encoding | ✓ | incl. sub-0x0E values |
| Flux3 (0x0C) encoding | ✓ | |
| Ovl16 (0x0B) overflow prefix | ✓ | 0x10000+ values |
| OOB StreamInfo / Index / StreamEnd / EOF | ✓ | matches decoder |
| **Decodes via production `uft_kf_decode()`** | ✓ | flux count, index count, RPM |
| Deterministic (seeded xorshift64*) | ✓ | byte-identical across runs |
| Defect: MISSING_END → parser MISSING_END | ✓ | group F |
| Defect: DEV_NO_INDEX → parser DEV_INDEX | ✓ | |
| Defect: DEV_BUFFER → parser DEV_BUFFER | ✓ | |
| Defect: TRUNCATED → MISSING_DATA/END | ✓ | |
| Defect: NO_INDEX_BLK → index_count 0 | ✓ | |
| Defect: weak bits (≤10% jitter) | ✓ | medium-safe |
| Defect: long damaged cell | ✓ | within MAX_NS |
| Index `flux_position` exactness | ~ | K-3 — RPM exact, position approximate |
| 24.027 MHz clock vs HAL SSOT | ✓ | cross-checked |
| Medium-safety refusal | ✓ | out-of-spec revs/track rejected |
| GCR / FM / HD cell families | ✗ | K-4 — DD-MFM shaped only |

## Test summary

`test_kryoflux_emulator.c`: **51 assertions, 6 groups (A–F), 0 fail.**
Deterministic across runs. Sibling emulators unaffected (SCP 37, GW 37,
XUM1541 56, Applesauce 111).

## Bench-verification gate (M3 KryoFlux)

Two HIGH divergences (K-1 real DTC exit-code/stdout vocabulary, K-2
RAW-stream byte-capture vs parser assumptions) are the gate before the
KryoFlux path is called "production". Until then: SIMULATED
(FIRMWARE-REALISTIC), Tier-3 PASS is bench-only.
