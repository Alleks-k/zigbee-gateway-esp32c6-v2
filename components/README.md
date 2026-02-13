# Components Migration

This folder contains local IDF components split from `main/`:

- `gateway_core`
- `gateway_net`
- `gateway_web`

Current state:

- Production sources are moved into these components.
- Build ownership is in components (`gateway_core`, `gateway_net`, `gateway_web`).
- `main/` keeps entrypoint sources and web static assets (`main/web/www`).

Migration order used:

1. Bootstrap components (Phase 0)
2. Move build ownership for `core` (Phase 1)
3. Move build ownership for `net` (Phase 2)
4. Move build ownership for `web` (Phase 3)
5. Physically move source files into `components/*` (Phase 4+)
