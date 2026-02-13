# Components Bootstrap (Phase 0)

This folder contains the migration scaffold for splitting `main/` into local IDF components:

- `gateway_core`
- `gateway_net`
- `gateway_web`

Current state (Phase 0):

- Components are registered and buildable.
- No production sources were moved yet.
- Existing application logic remains in `main/`.

Next migration phases should move code in this order:

1. `core/*` -> `components/gateway_core/`
2. `net/*` -> `components/gateway_net/`
3. `web/*` -> `components/gateway_web/`

