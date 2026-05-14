# improvement/format_extension/

Tests that prove UFT decodes formats outside `gw`'s set.

Planned tests (TESTER_STRATEGY §3):

| Test | Proves | gw fails because |
|------|--------|------------------|
| `test_ipf_decode.py` | IPF (SPS/CAPS) decode | not in gw's format set |
| `test_stx_decode.py` | Atari STX decode | not in gw's format set |
| `test_kfx_decode.py` | KryoFlux stream decode | not in gw's format set |
| `test_proprietary_jp_decode.py` | Japanese formats (D88/D77/NFD/...) | not in gw's format set |

Each test decodes a fixture from `tests/gw_corpus/inputs/` (or a
documented synthetic generator — never hand-rolled bytes claiming to be
a real disk) and asserts the produced sector/track structure. The
assertion is on UFT's output; there is no `gw` side to diff against,
which is exactly the point.
