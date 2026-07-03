# KryoFlux Firmware-Realistic Emulator (5/9)

Fifth controller in the `hardware-emulation-author` sequence, after SCP,
Greaseweazle, XUM1541 and Applesauce. Surfaces bugs in the UFT KryoFlux
HAL + RAW-stream parser in CI instead of only at a hardware bench.

## What is different about KryoFlux

KryoFlux has **no wire protocol on the UFT side**. The HAL
(`src/hal/uft_kryoflux_dtc.c`) runs the **DTC command-line tool** as a
subprocess; DTC drives the board and writes a KryoFlux **RAW stream
file**, which UFT then parses with `uft_kf_decode()`
(`src/flux/uft_kryoflux_stream.c`).

So the three layers map differently than the other controllers:

1. **DTC invocation contract** (`firmware_state_machine.{h,c}`) — models
   the DTC + device pair: device detection, per-track capture requests
   (track/side/revolutions/double-step), retry-on-error, and DTC-style
   exit codes. Write is always refused (KryoFlux is read-only in UFT).
2. **Device state machine** — DISCONNECTED → READY → CAPTURING → ERROR,
   with no-device / no-disk / no-index / bad-args faults that fail with
   an empty stream (never a short read the caller could mistake for
   success) and a soft-fault retry model mirroring DTC's `-t`.
3. **RAW stream generator** (`../../flux_gen/kryoflux/`) — emits the
   actual KryoFlux RAW container (Flux1/2/3, Ovl16, OOB StreamInfo /
   Index / StreamEnd / EOF) at 24.027 MHz, deterministic xorshift64*,
   with defect classes that map to specific parser status codes.

## The distinguishing feature

The generated streams are decoded by the **production parser**
`uft_kf_decode()`, and each container-level defect asserts the exact
`uft_kf_status_t` the parser returns:

| Defect | Parser status |
|--------|---------------|
| clean (StreamEnd+EOF) | `UFT_UFT_KF_STATUS_OK` |
| `MISSING_END` (no terminators) | `MISSING_END` |
| `DEV_NO_INDEX` (StreamEnd result) | `DEV_INDEX` |
| `DEV_BUFFER` (StreamEnd result) | `DEV_BUFFER` |
| `TRUNCATED` (cut mid-stream) | `MISSING_DATA` / `MISSING_END` |
| `NO_INDEX_BLK` (no Index OOB) | `OK`, `index_count == 0` |

That makes the generator a **live conformance fixture for the stream
decoder**, not just a self-consistent mock — the highest-value property
of this emulator.

## Forensic invariants

- No fabricated flux: a successful capture returns the generator's bytes
  verbatim; a failed capture returns a non-zero exit code AND an empty
  stream.
- Write is always refused (read-only policy, matching the HAL's honest
  stub).
- The 24.027428 MHz sample clock is cross-checked against the HAL SSOT
  `uft_kf_get_sample_clock()` at test time.

## Build & run

Registered in `tests/CMakeLists.txt` as `test_kryoflux_emulator` (links
`src/flux/uft_kryoflux_stream.c` for the real parser and
`src/hal/uft_kryoflux_dtc.c` for the SSOT clock; no subprocess is spawned
at test runtime).

```
ctest -R test_kryoflux_emulator --output-on-failure
```

Standalone:

```
gcc -Wall -Wextra -I include -I src \
    -I tests/emulators/kryoflux -I tests/flux_gen/kryoflux \
    tests/emulators/kryoflux/test_kryoflux_emulator.c \
    tests/emulators/kryoflux/firmware_state_machine.c \
    tests/flux_gen/kryoflux/flux_gen.c \
    src/flux/uft_kryoflux_stream.c src/hal/uft_kryoflux_dtc.c \
    -lm -o kf_test && ./kf_test
```

## Status

**51 assertions, 6 groups (A–F), 0 fail. Deterministic.** Coverage ~75%
aggregate. The two HIGH divergences (K-1 real DTC exit-code/stdout
vocabulary, K-2 RAW-stream byte-capture vs the parser's assumptions) are
the M3-KryoFlux bench-verification gate; until then this is SIMULATED
(FIRMWARE-REALISTIC), not a substitute for the bench.

See also: `DIVERGENCES.md`, `coverage_matrix.md`.
