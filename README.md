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

## Shared Commit Staging Rule
When committing shared-library changes, stage full library/doc subsets instead of individual files.

Required workflow:
1. Run `git -C shared status --short`.
2. Identify new module directories (`?? core/core_*`, `?? kit/kit_*`) and updated module/doc directories.
3. Stage by top-level subset directory (or explicitly removed subset dir) so each commit captures complete module state.
4. Request/confirm commit title from user before any `git commit` command.

Staging helper (review first, then run):
```sh
git -C shared status --short
```

Example full-subset staging pattern:
```sh
git -C shared add \
  core/<module_dir> \
  kit/<module_dir> \
  docs \
  showcase/<removed_or_updated_subset>
```

Commit title delegation rule:
- The commit title is always user-owned.
- Do not run `git commit` until the user explicitly confirms the exact commit title text.
