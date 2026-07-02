---
name: uft-audit-trail
description: |
  Use when implementing, extending, or wiring UFT's forensic audit trail
  and report-export subsystem (`include/uft/uft_audit_trail.h`,
  `include/uft/uft_forensic_report.h`). Trigger phrases: "audit trail",
  "audit event", "forensic report", "hash chain", "audit-trail wiring",
  "report exporter", "PDF/HTML/JSON report", "neuer event type", "risk
  scoring". Covers the 37 event types, 6 export formats (JSON/HTML/PDF/
  Markdown/Text/XML), 14 report sections, hash-chain integrity, and
  chain-of-custody invariants. DO NOT use for: generic logging
  (`logging_setup.py` style — that's not forensic), debug printf
  reformatting, GUI-side notification wiring (→ `uft-qt-widget`).
---

# UFT Audit Trail & Forensic Report

Use this skill when touching the audit-trail / forensic-report subsystem.
Both header APIs are currently `UFT_SKELETON_PLANNED` (22 + 13 declared
functions, 0 implementations) — this skill exists so that **whoever
implements them** preserves the contract that the rest of the codebase
already assumes.

This is the project's forensic USP: "Kein Bit verloren. Keine stille
Veränderung. Keine erfundenen Daten." The audit trail is the *evidence*
that the philosophy was followed. Wrong here = forensic credibility
loss — worse than a bug, the tool becomes untrustworthy for archive use.

## When to use this skill

- Implementing one of the 22 functions in `uft_audit_trail.h`
- Implementing one of the 13 functions in `uft_forensic_report.h`
- Adding a new `UFT_AE_*` event type
- Adding a new export format
- Adding a new `UFT_REPORT_SEC_*` section
- Wiring an existing UFT operation (read/write/recovery) to emit audit
  events that it currently doesn't
- Designing a hash-chain extension (new hash algo, new chain link)

**Do NOT use this skill for:**
- Replacing `log.info()` / `printf` debug output — that's not forensic,
  it's developer convenience. Audit events are user-facing evidence.
- GUI-side audit display widgets — use `uft-qt-widget`
- Hashing the *content* of a disk image — that's `uft-crc-engine`. This
  skill is about the chain *over* hashes, not the hashes themselves.

## The 6 forensic invariants (hard requirements)

These four words are the contract. Any change that loosens one is a
forensic regression and must be rejected.

| # | Invariant | What it forbids |
|---|---|---|
| **F1** | **Monotonic sequence** | `entry.sequence` strictly increases. No reordering, no gaps unless explicitly marked `UFT_AE_SEQUENCE_RESET`. |
| **F2** | **Append-only** | Once written, an entry is never edited or deleted. Correction = new entry referencing the prior `sequence`. |
| **F3** | **Hash-chain continuity** | Each entry's hash includes the previous entry's hash. Break in chain → tampering claim is provable. |
| **F4** | **No fabrication** | If a field is unknown (CRC before/after, bytes affected), use 0 + flag `UFT_AUDIT_FLAG_UNKNOWN`, never plausible-looking placeholder. |
| **F5** | **Wall-time + monotonic** | Both `timestamp_us` (monotonic from session start) AND `wall_time` (UTC) are recorded. NTP shifts don't corrupt evidence. |
| **F6** | **Severity-truthful** | A failed recovery is `UFT_AUDIT_ERROR`, not `UFT_AUDIT_WARNING` "to avoid alarming the user". The user *needs* to be alarmed. |

If a planned implementation needs to violate one of these to make a
test pass, the test is wrong. Stop, escalate.

## Event-type categorical map (37 events, 11 categories)

When adding a new `UFT_AE_*`, pick the right `0xXX00` band:

| Range | Category | Example | When to extend |
|---|---|---|---|
| `0x0001-0x000F` | Session | `SESSION_START`, `SESSION_END`, `CONFIG_CHANGE` | New session-lifecycle phase |
| `0x0100-0x010F` | File | `FILE_OPEN/READ/WRITE/DELETE` | New on-disk artifact lifecycle |
| `0x0200-0x020F` | Format | `FORMAT_DETECT/VERIFY/CONVERT` | New format-level operation |
| `0x0300-0x030F` | Track | `TRACK_READ/WRITE/DECODE/ENCODE/REPAIR` | New track-level operation |
| `0x0400-0x040F` | Sector | `SECTOR_READ/WRITE/VERIFY/REPAIR` | New sector-level operation |
| `0x0500-0x050F` | Hardware | `HW_CONNECT/READ_FLUX/WRITE_FLUX` | New HAL interaction class |
| `0x0600-0x060F` | Recovery | `RECOVERY_START/SUCCESS/FAIL/PARTIAL` | New recovery outcome class |
| `0x0F00-0x0F0F` | Errors | `ERROR`, `CRC_MISMATCH`, `DATA_LOSS` | New error class — **must** be reviewed |
| `0x1000-0x100F` | Checksum | `CHECKSUM_INPUT/OUTPUT`, `HASH_COMPUTED` | New hash event |

**Adding a new band (e.g. `0x0700` Network)** is a public-ABI change.
Coordinate with `abi-bomb-detector` agent first. Never reuse a hole in
an existing band — semantics get muddled across versions.

## Report-format / section map

```
                  ┌── JSON     (machine-consumed, full fidelity)
                  ├── HTML     (browser, evidence packaging)
                  ├── PDF      (archive-grade, sealed)
exporter ─────────┤
                  ├── Markdown (human-edit, change-control trail)
                  ├── XML      (legal/regulatory ingest)
                  └── Text     (low-tech fallback, terminal-friendly)
```

Sections (14 bit-flags in `UFT_REPORT_SEC_*`): SUMMARY, METADATA, HASHES,
HASH_CHAIN, TRACK_MAP, TRACK_DETAIL, ERRORS, TIMELINE, PROTECTION,
FILESYSTEM, FLUX, HEATMAP, AUDIT, SIGNATURE. `UFT_REPORT_SEC_ALL =
0xFFFF` is the default for forensic export.

**Critical:** All 6 exporters must produce **byte-identical** evidence
from the same audit log + section mask. Verify with a roundtrip:

```bash
uft report --in trail.uftlog --format json --out a.json
uft report --in trail.uftlog --format json --out b.json
diff a.json b.json   # MUST be empty
```

Non-determinism (timestamps, random IDs, hash-map iteration order) in an
exporter = silent forensic corruption. Use sorted keys, fixed locale, no
`time.time()` in the output path.

## Workflow

### Step 1: Locate the gap

```bash
# Which functions are still skeleton?
grep -n "UFT_SKELETON_PLANNED" include/uft/uft_audit_trail.h \
                                 include/uft/uft_forensic_report.h

# Which callers already expect them?
grep -rn "uft_audit_log\|uft_report_generate" src/ tests/
```

The skeleton headers carry their own caveat block — read it before
adding new call sites.

### Step 2: Implement against the invariants

Before writing the body, write down which of F1–F6 each line of the
function preserves. If you can't name one for a critical line (hash
computation, sequence increment, file fsync), the implementation is
incomplete.

### Step 3: Hash chain

```c
/* Chain link computation (canonical pattern) */
uint8_t prev_hash[32];  /* SHA-256 of previous entry */
uft_audit_entry_hash(&session->entries[N-1], prev_hash);

uft_audit_entry_t *e = &session->entries[N];
e->sequence = session->next_sequence++;
memcpy(e->prev_hash, prev_hash, 32);
e->wall_time = time(NULL);            /* F5: UTC, never local */
e->timestamp_us = monotonic_us_since(session->session_start_us);
/* ... populate event-specific fields ... */
uft_audit_entry_hash(e, e->self_hash);  /* covers all prior fields */
```

The hash MUST include `prev_hash`. That's the chain. Without it, the
log is a list, not a chain.

### Step 4: Persist atomically

```c
/* Write-then-fsync per entry. Forensic >> throughput. */
fwrite(e, sizeof(*e), 1, session->log_file);
fflush(session->log_file);
fsync(fileno(session->log_file));
```

Batching is **forbidden** in forensic mode. A power loss after 100
batched-but-unflushed entries = 100 missing pieces of evidence.

### Step 5: Test for tampering detection

```c
/* Modify byte 0 of entry 5. Verify chain breaks. */
log[5 * sizeof(uft_audit_entry_t)] ^= 0x01;
ASSERT(uft_audit_verify_chain(log) == UFT_AUDIT_TAMPERED);
ASSERT(uft_audit_verify_chain_first_break(log) == 5);
```

A chain that can't detect a single-byte mutation is not a chain.

## Pre-Done Checklist (vor dem „fertig"-Report verpflichtend)

Bevor du eine Implementierung in diesem Subsystem als done meldest,
gehst du diese Checkliste durch. Jeder Punkt = explizite Antwort, keine
implizite Annahme. Diese Liste ersetzt die frühere passive „Common
pitfalls"-Sektion durch einen aktiven Pass — der Agent muss bestätigen
oder einen STOP setzen.

| # | Frage | Wie geprüft |
|---|---|---|
| **C1** | **Severity richtig?** Steht für jeden `uft_audit_log()` Aufruf der korrekte `UFT_AUDIT_*`-Level — DATA_LOSS niemals INFO, CRC_MISMATCH niemals DEBUG? | `grep -n "uft_audit_log.*DATA_LOSS\|CRC_MISMATCH\|RECOVERY_FAIL"` im geänderten Code, jede Zeile manuell verifiziert. |
| **C2** | **Sequence persistiert?** Setzt KEIN Code-Pfad `next_sequence = 0` außer beim *neuen* Session-Start? Wenn der Log re-opened wird: ist `highest_sequence_in_log + 1` der Start? | `grep -rn "next_sequence\s*=" src/` — jeder Treffer muss either NEW-Session sein oder „resume from N+1". |
| **C3** | **Beide Uhren?** Schreibt jede neue Entry sowohl `wall_time` (`time(NULL)`) ALS AUCH `timestamp_us` (monotonic seit Session-Start)? F5 ist binär — eines reicht nicht. | Read-back der Entry-Init-Sequenz; beide Felder gesetzt, nicht `= 0`. |
| **C4** | **Unknown ≠ 0?** Setzt jeder Pfad der ein unbekanntes Feld (CRC vorher, betroffene Bytes) auf 0 setzt ALS AUCH das `UFT_AUDIT_FLAG_UNKNOWN`-Flag? Sonst liest der Konsument 0 als „all-zero CRC". | `grep -n "= 0;" um die geänderten Audit-Felder herum; Flag-Pfad nachweisen. |
| **C5** | **Hash-Chain enthält prev?** Berechnet `uft_audit_entry_hash()` den Hash über `prev_hash` *plus* alle anderen Felder, sodass ein Single-Byte-Flip in Entry N die Chain ab N bricht? | Manueller Tamper-Test (siehe „Verification" §3) im Output zeigen. |
| **C6** | **Atomare Persistenz?** Folgt jedes `fwrite` einem `fflush` + `fsync(fileno(...))`? Kein Batching im forensic mode? | `grep -nB1 "fwrite.*log_file"` zeigt `fflush`+`fsync` direkt danach. |
| **C7** | **Locale fixiert in Exportern?** Ruft jeder neue Exporter `setlocale(LC_NUMERIC, "C")` auf bevor er Floats ausgibt? Sonst kippt JSON in DE-Locale auf Komma. | `grep -n "fprintf.*%[df]" src/export/<new>` → `setlocale` davor. |
| **C8** | **Roundtrip-Determinismus geprüft?** Hat der Exporter byte-identische Ausgabe bei zwei Aufrufen mit demselben Input (kein `time.time()`, keine Hash-Map-Iter-Order)? | `diff a.json b.json` empty, Output im Report zeigen. |
| **C9** | **Tamper-Test grün?** Wurde die `tamper_chain.py`-Probe gegen den geänderten Code gelaufen und JEDES Byte-Flip wird erkannt? | Konkretes Testergebnis im Report, nicht „müsste passieren". |
| **C10** | **F1-F6 verletzt?** Würde irgendein neuer Pfad eine der sechs Invarianten brechen (Monotonic / Append-only / Hash-chain / No-fab / Wall+mono / Severity-truth)? | Wenn JA → STOP, kein „kleiner Bypass". Eskaliere via CONSULT an `forensic-integrity`. |

Wenn auch nur EIN Punkt nicht beantwortet werden kann oder rot ist: die
Aufgabe ist offen, nicht done. „90 % done" gibt es im forensischen
Subsystem nicht — entweder die Chain hält oder sie hält nicht.

## Common pitfalls (Beispiel-Anti-Patterns)

Die Pre-Done-Checkliste oben ist die *aktive* Pflicht; die folgenden
Beispiele sind die *Belegquellen* dafür, warum es jeden Punkt gibt.

### Silent severity downgrade

```c
/* WRONG — a real data loss reported as a polite info */
if (sector_bytes_lost > 0) {
    uft_audit_log(s, UFT_AE_DATA_LOSS, UFT_AUDIT_INFO, "%d bytes",
                  sector_bytes_lost);
}

/* RIGHT — data loss is at minimum ERROR */
if (sector_bytes_lost > 0) {
    uft_audit_log(s, UFT_AE_DATA_LOSS, UFT_AUDIT_ERROR, "%d bytes",
                  sector_bytes_lost);
}
```

### Reused-sequence on replay

```c
/* WRONG — re-reading the log resets the counter */
session->next_sequence = 0;  /* on reopen */

/* RIGHT — sequence persists across session boundaries */
session->next_sequence = highest_sequence_in_log + 1;
```

### Wall-time only

```c
/* WRONG — NTP step backwards looks like reordering */
e->wall_time = time(NULL);

/* RIGHT — F5: both clocks */
e->wall_time = time(NULL);
e->timestamp_us = monotonic_us_since(session->session_start_us);
```

### Plausible-default fabrication

```c
/* WRONG — CRC unknown gets a fake "00000000" that looks computed */
e->crc_before = 0;

/* RIGHT — explicit unknown flag, never let consumer mistake 0 for "all-zero CRC" */
e->crc_before = 0;
e->flags |= UFT_AUDIT_FLAG_CRC_BEFORE_UNKNOWN;
```

### Locale-leaking in exporter

```c
/* WRONG — number format depends on $LANG */
fprintf(f, "%f", risk_score);   /* "0,75" in DE, "0.75" in US */

/* RIGHT — fixed locale for forensic output */
setlocale(LC_NUMERIC, "C");
fprintf(f, "%f", risk_score);   /* always "0.75" */
```

## Verification

```bash
# 1. All skeleton functions either implemented or still flagged
grep -c "UFT_SKELETON_PLANNED" include/uft/uft_audit_trail.h \
                               include/uft/uft_forensic_report.h
# expected: 1 in each (the block header), 0 stray references

# 2. Roundtrip determinism per exporter
for fmt in json html pdf markdown text xml; do
    ./uft report --in fixtures/audit_canon.uftlog --format $fmt --out /tmp/a.$fmt
    ./uft report --in fixtures/audit_canon.uftlog --format $fmt --out /tmp/b.$fmt
    diff -q /tmp/a.$fmt /tmp/b.$fmt || echo "NON-DETERMINISTIC: $fmt"
done

# 3. Tamper detection on every byte
python3 tests/tamper_chain.py fixtures/audit_canon.uftlog
# expected: every flipped byte detected, exact entry index reported

# 4. Severity audit (no DATA_LOSS at INFO/DEBUG severity)
python3 scripts/audit_severity_match.py fixtures/audit_canon.uftlog
```

## Related

- `include/uft/uft_audit_trail.h` — event types, session struct, 22 APIs
- `include/uft/uft_forensic_report.h` — 13 report APIs, 6 formats, 14 sections
- `docs/KNOWN_ISSUES.md` — tracks the skeleton APIs as "Planned APIs"
- `docs/DESIGN_PRINCIPLES.md` Principle 1 (kein Bit verloren) +
  Principle 4 (keine erfundenen Daten) — directly enforced here
- `.claude/agents/forensic-integrity.md` — reviews any change to this
  subsystem; consult-block on any F1–F6 question
- `.claude/skills/uft-crc-engine/` — hash primitives this skill calls,
  not duplicates
- `.claude/skills/uft-recovery-integrity/` — recovery operations are
  *callers* of the audit log; their integrity rules feed event severity
