# improvement/gui/

Tests that prove UFT's GUI behaviour — `gw` is CLI-only.

**Requires `pytest-qt`.** Tests in this directory carry
`@pytest.mark.requires_qt` and are auto-skipped when pytest-qt is not
installed (see `../conftest.py`). They run headless.

Planned tests (TESTER_STRATEGY §3):

| Test | Proves | gw fails because |
|------|--------|------------------|
| `test_main_window_smoke.py` | the main window builds and shows without crashing | gw has no GUI |
| `test_hardware_tab_capability_disable.py` | the Hardware tab greys out actions a provider's capability set does not cover (the `wire_action<cap::X>` codegen, post-P1) | gw has no GUI and no capability model |
| `test_format_tab_workflow.py` | the format-conversion tab walks a path end-to-end | gw has no GUI |

`test_hardware_tab_capability_disable.py` is the executable proof of the
type-driven HAL refactor's GUI half: a read-only provider (FC5025,
KryoFlux) must have its Write button structurally disabled because the
provider type does not satisfy the write capability concept.
