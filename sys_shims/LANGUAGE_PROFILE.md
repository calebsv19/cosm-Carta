# sys_shims Language Integration Profile

Status: Active (S6 baseline)

## Purpose
Define the include-mapping contract between the language frontend and
`shared/sys_shims` so compiler behavior is deterministic while remaining
compatible with normal C toolchains.

## Profile IDs
- `shim_profile_c_passthrough_v1`
  - Direct C includes, no overlay required.
- `shim_profile_lang_frontend_shadow_v1`
  - Language frontend/system-include resolution via shim overlay + shim include.

## Include Path Contract
For `shim_profile_lang_frontend_shadow_v1`, system include lookup must prepend:
1. `shared/sys_shims/overlay/include`
2. `shared/sys_shims/include`
3. existing system include paths

Local include lookup (`#include "x.h"`) stays unchanged and must continue to
search local/project paths before system include paths.

## Compiler Pin/Check Contract
When strict profile enforcement is enabled:
- `FISICS_SHIM_PROFILE_ENFORCE=1`
- `FISICS_SHIM_PROFILE_ID` must match expected profile id
- `FISICS_SHIM_PROFILE_VERSION` must match expected profile version
- `SYSTEM_INCLUDE_PATHS` must begin with:
  - `<overlay path>:<shim include path>:...`

Reference variables used by the S6 harness:
- `FISICS_SHIM_PROFILE_EXPECT_ID`
- `FISICS_SHIM_PROFILE_EXPECT_VERSION`
- `FISICS_SHIM_PROFILE_EXPECT_OVERLAY`
- `FISICS_SHIM_PROFILE_EXPECT_INCLUDE`

## Header Mapping Contract
| Requested include | Shim resolution domain | Required in profile |
| --- | --- | --- |
| `<stdbool.h>` | `sys_shims/shim_stdbool.h` | Required |
| `<stddef.h>` | `sys_shims/shim_stddef.h` | Required |
| `<stdint.h>` | `sys_shims/shim_stdint.h` | Required |
| `<stdarg.h>` | `sys_shims/shim_stdarg.h` | Required |
| `<stdio.h>` | `sys_shims/shim_stdio.h` | Required |
| `<stdlib.h>` | `sys_shims/shim_stdlib.h` | Required |
| `<string.h>` | `sys_shims/shim_string.h` | Required |
| `<errno.h>` | `sys_shims/shim_errno.h` | Required |
| `<time.h>` | `sys_shims/shim_time.h` | Required |
| `<math.h>` | `sys_shims/shim_math.h` | Required |
| `<limits.h>` | `sys_shims/shim_limits.h` | Optional (S7) |
| `<locale.h>` | `sys_shims/shim_locale.h` | Optional (S7) |
| `<ctype.h>` | `sys_shims/shim_ctype.h` | Optional (S7) |
| `<signal.h>` | `sys_shims/shim_signal.h` | Optional (S7) |

## Compatibility Rules
- Shim headers are additive wrappers around the system headers.
- Underlying standard headers are still reached via `#include_next`.
- No global keyword/type redefinition is allowed.
- Fallback baseline mode (`SHIM_MODE=off`) must remain available.

## Verification Targets
- `make -C fisiCs shim-parse-parity-quiet`
- `make -C fisiCs shim-language-profile`
- `make -C fisiCs shim-language-profile-negative`
- `make -C fisiCs shim-s6-gate`
- `make -C shared/sys_shims conformance`

## Known Limits (Current)
- Profile covers 10 baseline C domains plus optional S7 extended domains.
- Additional families beyond S7 remain on baseline system behavior until
  explicit shim domains are added.
