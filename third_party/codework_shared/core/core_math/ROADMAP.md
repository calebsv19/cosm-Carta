# core_math Roadmap

## Mission
Provide small, deterministic math primitives shared by core and kit modules.

## Near-Term Goals
- Keep API compact and dependency-light.
- Add only primitives used by multiple modules.
- Avoid scene/import policy logic (belongs in `core_scene`/`core_space`).

## Planned Growth
- Matrix primitives after concrete cross-app demand.
- Common interpolation and statistics helpers.
- Optional deterministic random/noise helpers when needed.
