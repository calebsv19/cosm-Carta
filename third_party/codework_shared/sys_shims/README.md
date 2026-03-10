# sys_shims

`sys_shims` is the shared staging layer for system include compatibility wrappers and additive local extensions.

## Goals
- Keep shim behavior explicit and opt-in.
- Allow gradual growth of custom include behavior without polluting `core_*` modules.
- Preserve compatibility with standard C headers and normal GCC/Clang flows.

## Non-Goals
- Replacing libc implementations.
- Injecting implicit behavior into core libraries.
- Becoming a mandatory dependency for all projects.

## Layout
- `include/sys_shims/`: public shim headers
- `overlay/include/`: optional shadow-mode system header overlay
- `tests/`: shim smoke and conformance tests
- `LAYOUT.md`: directory and target contract
- `POLICY.md`: behavior and ownership rules
- `ROADMAP.md`: phased execution summary
- `Makefile`: module-local build/test entrypoints
- `VERSION`: module version source of truth
- `FISICS_ADOPTION.md`: current S5 adoption notes for compiler integration
- `LANGUAGE_PROFILE.md`: S6 include mapping and language/frontend profile contract

## Initial Policy
- Wrap standard headers first, then add prefixed helpers.
- Keep helpers small and portable.
- Favor additive aliases and utility functions over redefinitions.

## Commands
- `make -C shared/sys_shims test`
- `make -C shared/sys_shims test-matrix`
- `make -C shared/sys_shims conformance`
- `make -C shared/sys_shims clean`

`conformance` writes:
- `shared/sys_shims/build/sys_shims_conformance_report.json`

## Current Header Set
- `shim_stdbool.h`
- `shim_stddef.h`
- `shim_stdint.h`
- `shim_stdarg.h`
- `shim_stdio.h`
- `shim_stdlib.h`
- `shim_string.h`
- `shim_errno.h`
- `shim_time.h`
- `shim_math.h`
- `shim_limits.h`
- `shim_locale.h`
- `shim_ctype.h`
- `shim_signal.h`

## Planning Reference
- `shared/docs/ecosystem_north_star_docs/24_system_include_shims_plan.md`
