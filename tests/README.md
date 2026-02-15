# Test Matrix (P0.9)

## Host tests (Core logic, no ESP-IDF runtime)

Scope:
- `config_service` validation and storage-facing orchestration.

Run:

```bash
./tools/run_host_tests.sh
```

## Target tests (Storage, NVS/flash-dependent)

Scope:
- `storage_kv` read/write/erase behavior in `gateway_core_storage`.

Location:
- `components/gateway_core_storage/test/gateway_core_storage_self_tests.c`

Execution model:
- Built and executed only in self-test firmware mode (`CONFIG_GATEWAY_SELF_TEST_APP=y`).

Run firmware build gate:

```bash
./tools/run_target_self_tests.sh
```

Notes:
- Script always uses a fresh temporary `SDKCONFIG` to avoid stale config drift.
- Self-test defaults are defined in `tests/selftest.sdkconfig.defaults`.
