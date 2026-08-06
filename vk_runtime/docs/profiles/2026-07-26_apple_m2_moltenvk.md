# Apple M2 / MoltenVK S1 Capability Profile

Observed: 2026-07-26
Module: `vk_runtime 0.1.0`
Schema: `codework_gpu_capability_report_v1`

## Host result

| Field | Observed value |
| --- | --- |
| OS / architecture | macOS / arm64 |
| GPU | Apple M2 integrated GPU |
| Vulkan loader | `1.4.321` |
| Requested / negotiated API | `1.2.0` / `1.2.0` |
| Device API | `1.2.323` |
| Driver | MoltenVK `1.4.0` |
| Reported device-local heap | `17179869184` bytes |
| Queue families | four; each reports graphics, compute, and transfer |
| Selected role families | graphics `0`, compute `0`, transfer `0` |
| Subgroup size | `32` |
| `shaderInt64` / `shaderFloat64` | true / false |
| Portability enumeration/subset | enabled / supported |

Two independent headless reports were byte-identical.

## Validation result

The Homebrew validation manifest is discoverable, but its relative dylib name
does not load from the default command environment. The runtime correctly
reports this distinction as:

```json
{
  "available": true,
  "enabled": false,
  "load_failed": true
}
```

For the required-validation proof, the test target resolves the installed
Homebrew validation library directory for the probe process only. The result
was:

```json
{
  "available": true,
  "enabled": true,
  "load_failed": false,
  "warnings": 0,
  "errors": 0
}
```

The runtime does not mutate caller environment. Packaged/debug-host validation
library resolution remains a later integration concern.

## Reproduction

```sh
make -C shared/vk_runtime test-live
make -C shared/vk_runtime test-live-validation
```

Both commands require access to Metal/MoltenVK and therefore may fail inside a
GPU-restricted sandbox even when the host GPU is healthy.
