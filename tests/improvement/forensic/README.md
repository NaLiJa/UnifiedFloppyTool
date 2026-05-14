# improvement/forensic/

Tests that prove UFT's forensic layer — the thing `gw` does not have at
all. Every test here makes a `docs/DESIGN_PRINCIPLES.md` principle
executable.

Planned tests (TESTER_STRATEGY §3), each written by `improvement-test-author`
once the behaviour is observable on a built `uft`:

| Test | DESIGN_PRINCIPLES property | gw fails because |
|------|----------------------------|------------------|
| `test_lossreport_emitted.py` | "Keine stille Veränderung" — every lossy step emits a `.loss.json` sidecar | gw silently drops timing/weak-bit data on flux→sector |
| `test_audit_chain_integrity.py` | chain-of-custody — the hash chain over audit events verifies | gw keeps no audit log |
| `test_marginal_data_preserved.py` | "Kein Bit verloren" — marginal/divergent reads survive to the output, never collapsed | gw majority-votes marginal reads away |
| `test_destructive_op_consent.py` | destructive ops (erase/overwrite) refuse without explicit consent | gw erases on command, no gate |
| `test_provenance_chain.py` | output records the tool, version and source that produced it | gw output is anonymous |

Implementation hints live in the relevant `src/core/` modules
(`uft_loss_report.c`, audit-chain code) — the test asserts *observed*
CLI/file behaviour, never reaches into internals.
