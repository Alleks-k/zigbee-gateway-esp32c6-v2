# Zigbee Gateway Architecture

## Layers

1. Web Layer (`components/gateway_web/*`)
- HTTP routes, API handlers, WebSocket broadcast, static files.
- Must not call Zigbee SDK directly.
- Talks to services only (`zigbee_service`, `wifi_service`, `system_service`).

2. Service Layer (`components/gateway_core/*service*`)
- Use-case/business orchestration for Zigbee, Wi-Fi, reboot/factory reset.
- Converts transport-level input into domain operations.
- Publishes/consumes events via ESP Event Loop where needed.

3. Domain/State Layer (`components/gateway_core/include/device_manager.h`, `components/gateway_core/include/gateway_events.h`)
- In-memory device state and domain events.
- No HTTP details, no UI payload formatting.

4. Persistence Layer (`components/gateway_core/include/settings_manager.h`)
- Single owner of NVS access for app settings and device snapshots.
- Factory reset logic and grouped erase/reporting live here.

5. Platform/Drivers Layer (`components/gateway_net/*`, ESP-IDF/Zigbee SDK)
- Wi-Fi init/STA/AP fallback, esp_zb stack integration, system primitives.

## Dependency Rules

1. `web -> service -> domain/persistence -> platform`.
2. `web` must not include `esp_zigbee_*` SDK headers.
3. `settings_manager` is the only module that writes app NVS keys.
4. Cross-module notifications should go through `gateway_events` (ESP Event Loop), not direct tight coupling.
5. Frontend API contract should remain stable under `/api/v1/*`; legacy `/api/*` is compatibility alias.

## Factory Reset Contract

Factory reset is grouped and reported:
- `wifi` (credentials keys),
- `devices` (saved device list),
- `zigbee_storage` partition,
- `zigbee_fct` partition.

API response includes per-group status in JSON `details`.

## Configuration & Secrets

Use tracked defaults + local overrides:
- `components/gateway_net/include/wifi_settings.h` + optional `wifi_settings_local.h` (gitignored),
- `components/gateway_net/include/wifi_credentials.h` + `wifi_credentials_local.h` (gitignored),
- examples in `*.example`.
