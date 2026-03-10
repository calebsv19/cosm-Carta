# CodeWork Shared Libraries

`shared/` is the reusable foundation layer for CodeWork programs.

## Design Law
Core defines meaning. Kits define expression. Apps define purpose.

## Layout
- `core/`: stable cross-app core contracts and runtimes (`core_*`).
- `kit/`: optional higher-level integration/rendering kits (`kit_*`).
- `sys_shims/`: system include compatibility overlays and conformance tests.
- `shape/`: shared shape library/import tooling.
- `vk_renderer/`: Vulkan renderer backend layer.
- `timer_hud/`: shared timing/profiling HUD utilities.
- `assets/`: shared assets (fonts/scenes/shapes).
- `showcase/`: demonstration apps (not required for consumers).
- `docs/`: public shared-library documentation.

## Public Release Policy
- Keep public docs in `docs/`.
- Keep private/internal working docs outside this tree (for example `../_private_docs/shared/`).
- Keep generated local artifacts out of versioned source.

## Build And Validation
Root checks:
- `make -C shared core-test`

Module checks:
- `make -C shared/core/<module> test`
- `make -C shared/kit/<module> test`

## Versioning
Shared module versioning policy is documented in `docs/VERSIONING.md`.
