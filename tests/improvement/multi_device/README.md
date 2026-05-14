# improvement/multi_device/

Tests that prove UFT drives hardware `gw` cannot — `gw` only ever talks
to Greaseweazle.

Planned tests (TESTER_STRATEGY §3):

| Test | Proves | gw fails because |
|------|--------|------------------|
| `test_kryoflux_provider.py` | KryoFlux read path works through the V2 provider | gw has no KryoFlux support |
| `test_scp_provider.py` | SuperCard Pro read/write through the V2 provider | gw has no SCP support |
| `test_fluxengine_provider.py` | FluxEngine read/write through the V2 provider | gw has no FluxEngine support |
| `test_provider_switch_consistency.py` | switching the active provider keeps capability reporting + outcomes consistent | gw has a single fixed device model |

These exercise the post-refactor V2 providers
(`src/hardware_providers/*_provider_v2.cpp`). Where real hardware is
absent the tests use mock backends / recorded traces from
`tests/gw_corpus/inputs/mock_hardware_traces/`; with no mock available
the test skips and is noted for the HIL layer (P3.4).
