# improvement/concurrency/

Tests that prove UFT's multi-device session model — `gw` has no concept
of running more than one device at once.

Planned tests (TESTER_STRATEGY §3):

| Test | Proves | gw fails because |
|------|--------|------------------|
| `test_parallel_drives.py` | two drives can be imaged concurrently without state corruption | gw is single-device, single-session |
| `test_long_running_session.py` | a long session stays stable — no handle/memory leak, no audit-chain drift | gw has no session model to stress |

Where real parallel hardware is absent these use mock backends; with no
mock the test skips and is noted for the HIL layer (P3.4).
