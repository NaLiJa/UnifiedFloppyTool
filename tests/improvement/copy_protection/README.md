# improvement/copy_protection/

Tests that prove UFT detects and documents historical copy-protection
schemes — `gw` captures flux but does not analyse protection.

Planned tests (TESTER_STRATEGY §3):

| Test | Proves | gw fails because |
|------|--------|------------------|
| `test_protection_dungeon_master.py` | Dungeon Master fuzzy-bit detection | gw does not classify protection |
| `test_protection_lenslok.py` | Lenslok scheme detection | gw does not classify protection |
| `test_protection_long_track.py` | long-track / over-format detection | gw does not classify protection |

Each test feeds a fixture with a known protection signature and asserts
UFT's protection report names the scheme. Fixtures come from
`tests/gw_corpus/inputs/` or a documented synthetic generator.
