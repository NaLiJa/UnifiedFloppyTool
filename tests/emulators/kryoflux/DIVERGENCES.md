# KryoFlux Emulator — Divergences from Real Hardware

Forensic-honesty ledger. `SPEC_STATUS: REVERSE-ENGINEERED` — modelled on
the DTC argv contract in `src/hal/uft_kryoflux_dtc.c` and the KryoFlux
RAW stream protocol parsed by `src/flux/uft_kryoflux_stream.c`. KryoFlux
is unusual: there is no wire protocol on the UFT side. The HAL runs the
DTC command-line tool as a subprocess, which drives the board and writes
a RAW stream file. This emulator models the **DTC + device pair**, not a
byte protocol.

Severity: **HIGH** could mask a bug / break on real hardware · **MEDIUM**
plausible but unverified · **LOW** cosmetic / tolerance.

| ID | Sev | Divergence |
|----|-----|------------|
| **K-1** | HIGH | DTC exit codes (`KF_DTC_EXIT_*`: NO_DEVICE=1, NO_DISK=2, NO_INDEX=3, BAD_ARGS=4, WRITE_DENY=5, IO=6) are MODELLED. The real DTC exit-code vocabulary is not fully documented; the HAL parses DTC *stdout text*, not exit codes, for some conditions. The mapping here is internally consistent and drives the test, but the real code→condition table must be confirmed at the M3-KryoFlux bench. |
| **K-2** | HIGH | The RAW stream framing (Flux1/2/3, Ovl16, OOB StreamInfo/Index/StreamEnd/EOF) matches the UFT parser `uft_kf_decode()` exactly — which is the point: defects here map to real parser status codes. But the parser is itself only in-tree-verified against public protocol docs (SPS/a2r), not against a byte-capture from a physical KryoFlux. If the real DTC emits a stream variant the parser mis-handles, this emulator would not catch it (it shares the parser's assumptions). |
| **K-3** | MEDIUM | The Index OOB `stream_pos` field is written as the emulator's current buffer offset, not the true cell-stream position (which excludes OOB bytes). The production parser resolves `flux_position` from it by linear search; because our OOB blocks are small, the resolved cell is close but not exact. RPM (derived from `sample_counter` deltas, which ARE exact) is unaffected — only `indexes[n].flux_position` is approximate. |
| **K-4** | MEDIUM | Cell timing is a deterministic 4/6/8 µs MFM-DD mix summed to a 200 ms (300 RPM) revolution. Real disks have a continuous jitter distribution and platform-specific cell families (GCR, FM, HD). The generator is DD-MFM-shaped only; other densities are not modelled. |
| **K-5** | MEDIUM | The retry model (`-t` count) treats every transient failure as recoverable within N retries. Real DTC distinguishes hard errors (no index) from soft (CRC/buffer) and only retries the latter — modelled here by only injecting soft IO faults into the retry path; hard faults fail immediately (correct), but the soft/hard classification of a real DTC run is not fully reproduced. |
| **K-6** | LOW | `dtc -i` device detection returns a configured count; real detection enumerates USB and can report board firmware version, which this model does not carry. |
| **K-7** | LOW | Double-step (`-k2`) is accepted as a request flag but does not change the generated stream (a 40-track disk on an 80-track drive would read the same cells at doubled physical steps — the flux content is identical, only the stepper motion differs, which a stream-level model cannot show). |
| **K-8** | LOW | Write is always refused (`KF_DTC_EXIT_WRITE_DENY`) — matching UFT's read-only KryoFlux policy and the HAL's honest-stub write path. Real DTC *can* write with `-w`; the emulator deliberately never exercises it (forensic-safety, same stance as the other controllers). |

## What is NOT divergent (bench-relevant confidence)

- Generated streams decode cleanly through the PRODUCTION parser
  `uft_kf_decode()` — this is a live conformance test of the decoder,
  not a self-consistent mock. Group E/F assert the round-trip.
- Container-level defect classes map to the EXACT parser status codes:
  MISSING_END, DEV_INDEX, DEV_BUFFER, MISSING_DATA — verified in group F.
- The 24.027428 MHz sample clock is cross-checked against the HAL SSOT
  `uft_kf_get_sample_clock()` at test time.
- Flux opcode encoding boundaries (Flux1 0x0E-0xFF, Flux2 0x00-0x07,
  Flux3 0x0C, Ovl16 0x0B) are white-box tested against the exact
  boundaries the decoder switches on.
- Medium-safety: the generator refuses out-of-spec revolutions/tracks
  and keeps all intervals within [1 µs, 200 µs]; `count_unsafe` is 0 on
  every clean and weak-bit stream.

## Bench-verification gate (M3 KryoFlux)

Before the KryoFlux HAL path is called "production", a bench session
must resolve K-1 (real DTC exit-code / stdout vocabulary) and K-2
(RAW stream byte-capture vs the parser's assumptions). Until then:
SIMULATED (FIRMWARE-REALISTIC), Tier-3 PASS is bench-only.
