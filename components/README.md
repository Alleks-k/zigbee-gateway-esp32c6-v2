# Components Migration

This folder contains local IDF components split from `main/`:

- `gateway_app` (bootstrap/runtime orchestration, boundary adapters)
- `gateway_core` (core business rules/services)
- `gateway_core_facade` (public facade for web/api callers)
- `gateway_core_events`
- `gateway_core_state`
- `gateway_core_storage`
- `gateway_core_zigbee`
- `gateway_core_wifi`
- `gateway_core_system`
- `gateway_core_jobs`
- `gateway_shared_config`
- `gateway_net`
- `gateway_web` (facade/glue)
- `gateway_web_api`
- `gateway_web_ws`
- `gateway_web_static`

Current state:

- Production sources are moved into these components.
- Build ownership is in components (`gateway_core*`, `gateway_web*`, `gateway_net`).
- `main/` keeps thin entrypoint and web static assets (`main/web/www`).
- `device_service` in core is policy-only and emits notifications via notifier callbacks.
- ESP event publishing for device list/delete notifications is handled in `gateway_app`.

Migration order used:

1. Bootstrap components (Phase 0)
2. Move build ownership for `core` (Phase 1)
3. Move build ownership for `net` (Phase 2)
4. Move build ownership for `web` (Phase 3)
5. Physically move source files into `components/*` (Phase 4+)
6. Split `gateway_core` internals into dedicated IDF components (Phase 5)
7. Split `gateway_web` internals into API/WS/static IDF components (Phase 6)
