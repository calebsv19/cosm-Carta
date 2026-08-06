# Apple M2 / MoltenVK S2 Compute Profile

Observed: 2026-07-26
Module: `vk_runtime 0.2.0`
Schemas:

- `codework_vk_shader_manifest_v1`
- `codework_gpu_compute_report_v1`

## Workloads

Both fixtures contain 257 values and dispatch five 64-lane workgroups.

| Kernel | Contract | Result |
| --- | --- | --- |
| `codework_u32_transform_v1` | `output = input * 3u + 7u` | exact parity |
| `codework_f32_transform_v1` | `output = input * 1.25f + 0.5f` | parity under absolute/relative `1e-6`; observed maximum error `0` |

Float fixtures are finite-only. NaN, infinity, signed-zero, and subnormal
classification remain later numeric hardening.

## Shader identity

| Kernel | SPIR-V SHA-256 |
| --- | --- |
| unsigned integer | `076935ff67b898e7a80f41a48b0fb5086c3aace461f0ccba2220292d742d9c35` |
| float | `daf4614fe9a95530d30323d47ae11242ffbcef914493ea7c4b4c6ab79bb784c8` |

Both shaders were compiled with the recorded local `glslc` identity using
`--target-env=vulkan1.1 -O`. The manifests bind the descriptor layout, `main`
entry point, and `[64,1,1]` workgroup size.

## Failure and validation evidence

- binding preflight: `descriptor_binding_invalid`;
- shader preflight: `shader_code_invalid`;
- validation requested, available, and enabled;
- zero validation warnings;
- zero validation errors through compute teardown and runtime close.

Two independent runs produced identical device, shader, fixture, oracle, and
GPU result fields. Timing is measured separately and intentionally excluded
from deterministic comparison.

## Timing interpretation

The report separates host buffer creation/upload, queue submit plus fence wait,
and host readback. These S2 values prove instrumentation shape only. The
buffers are host-visible/coherent and each dispatch recreates its Vulkan
objects, so this is not a device-local performance baseline or speedup claim.

## Reproduction

```sh
make -C shared/vk_runtime test-shaders
make -C shared/vk_runtime test-compute-live
make -C shared/vk_runtime test-compute-live-validation
```

The live tests require Metal/MoltenVK access and may fail in a GPU-restricted
sandbox even when the host GPU is healthy.
