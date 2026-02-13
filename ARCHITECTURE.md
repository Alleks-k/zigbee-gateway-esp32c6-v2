# Zigbee Gateway Architecture

## Layers

1. Web Layer (`main/web/*`)
- HTTP routes, API handlers, WebSocket broadcast, static files.
- Must not call Zigbee SDK directly.
- Talks to services only (`zigbee_service`, `wifi_service`, `system_service`).

2. Service Layer (`main/core/*service*`)
- Use-case/business orchestration for Zigbee, Wi-Fi, reboot/factory reset.
- Converts transport-level input into domain operations.
- Publishes/consumes events via ESP Event Loop where needed.

3. Domain/State Layer (`main/core/device_manager.*`, `main/core/gateway_events.*`)
- In-memory device state and domain events.
- No HTTP details, no UI payload formatting.

4. Persistence Layer (`main/core/settings_manager.*`)
- Single owner of NVS access for app settings and device snapshots.
- Factory reset logic and grouped erase/reporting live here.

5. Platform/Drivers Layer (`main/net/*`, ESP-IDF/Zigbee SDK)
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
- `main/net/wifi_settings.h` + optional `wifi_settings_local.h` (gitignored),
- `main/net/wifi_credentials.h` + `wifi_credentials_local.h` (gitignored),
- examples in `*.example`.
