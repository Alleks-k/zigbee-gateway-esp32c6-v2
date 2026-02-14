# Zigbee Gateway Architecture

## Component Topology

### App Entry
- `main/`
- Bootstraps Zigbee, Wi-Fi runtime, Web stack, event wiring.
- Does not contain business logic besides startup orchestration.

### Core Facade
- `components/gateway_core`
- Thin facade that re-exports domain interfaces and composes core subcomponents.

### Core Subcomponents
- `components/gateway_core_zigbee`
  - Zigbee services, device operations, Zigbee-facing domain orchestration.
- `components/gateway_core_wifi`
  - Wi-Fi domain service contract (save/apply settings use-cases).
- `components/gateway_core_system`
  - Reboot/factory-reset/use-case level system operations.
- `components/gateway_core_storage`
  - NVS/schema/device persistence (`settings_manager`, storage ownership).
- `components/gateway_core_events`
  - Domain event bus + event bridge.
- `components/gateway_core_jobs`
  - Async jobs queue/execution (`scan`, `lqi_refresh`, `reboot`, `factory_reset`).

### Network Adapter
- `components/gateway_net`
- Platform adapter layer for ESP-IDF networking runtime:
  - STA/AP fallback,
  - hostname/mDNS-related runtime support,
  - Wi-Fi scan/runtime telemetry adapter pieces.

### Web Facade
- `components/gateway_web`
- Thin facade over transport subcomponents.

### Web Subcomponents
- `components/gateway_web_api`
  - HTTP routes, handlers, request/response mapping, DTO contracts.
- `components/gateway_web_ws`
  - WebSocket session lifecycle and broadcasts (`devices_delta`, `health_state`, `lqi_update`).
- `components/gateway_web_static`
  - Static asset serving for `main/web/www/*`.

## Layering Rules

1. `web_* -> gateway_core*` only via service/use-case interfaces, not SDK calls.
2. `gateway_web_*` must not call Zigbee SDK directly.
3. `gateway_core_storage` is the single owner of app NVS keys/schema.
4. Cross-cutting notifications should go through `gateway_core_events`.
5. `gateway_net` is adapter/platform runtime; business decisions stay in core services.
6. Public transport contract is `/api/v1/*` + WS typed envelopes.

## Runtime Flows (Canonical)

1. Control ON/OFF
- `POST /api/v1/control`
- `gateway_web_api -> api_usecases -> gateway_core_zigbee`

2. Health/Status
- `GET /api/v1/status`, `GET /api/v1/health`
- `gateway_web_api -> api_usecases -> gateway_core_* + gateway_net adapters`

3. LQI
- `GET /api/v1/lqi`
- `POST /api/v1/jobs {type:lqi_refresh}`
- `gateway_web_api -> gateway_core_jobs -> gateway_core_zigbee`
- WS push: `type: lqi_update` from `gateway_web_ws`

4. Wi-Fi settings
- `POST /api/v1/settings/wifi`
- `gateway_web_api -> gateway_core_wifi/system -> gateway_net runtime apply`

## Factory Reset Contract

Factory reset is grouped and reported:
- `wifi` keys/settings,
- `devices` snapshot keys,
- Zigbee partitions (`zigbee_storage`, `zigbee_fct` when configured).

Response reports per-group status in JSON `details`.

## Current Technical Debt

1. `gateway_web_api` still has some large handler/builder files; continue mapper/use-case extraction.
2. `gateway_web -> gateway_net` compile-time coupling should be reduced further (prefer core facade contracts).
3. Integration coverage gaps:
- WS real socket lifecycle/backpressure,
- browser-level E2E contract for new health/LQI fields.

## Configuration & Secrets

Use tracked defaults with local overrides:
- `components/gateway_net/include/wifi_settings.h`
- optional local override headers (`*_local.h`, gitignored),
- example headers remain in repo for onboarding.
