# Apple M2 / MoltenVK S3 Residency Profile

Observed: 2026-07-26
Module: `vk_runtime 0.3.0`
Schema: `codework_gpu_residency_report_v1`

## Device and memory roles

- selected compute queue family: `0`;
- host-staging memory property flags: `15`
  (`DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT | HOST_CACHED`);
- device buffer A property flags: `1` (`DEVICE_LOCAL`);
- device buffer B property flags: `1` (`DEVICE_LOCAL`);
- all buffers use exclusive sharing and remain owned by the selected compute
  queue family;
- upload, compute, and readback commands therefore require no queue-family
  ownership transfer in this profile.

The staging allocation is persistently mapped and reused for both upload and
readback. Device buffers are not host-mapped.

## Dependent-dispatch proof

The proof uses the manifest-bound `codework_u32_transform_v1` shader:

`output = input * 3u + 7u`

SPIR-V SHA-256:
`076935ff67b898e7a80f41a48b0fb5086c3aace461f0ccba2220292d742d9c35`

Each chain:

1. uploads 1,025 unsigned values from staging to device buffer A;
2. dispatches A to B;
3. records an explicit
   `COMPUTE_SHADER/SHADER_WRITE -> COMPUTE_SHADER/SHADER_READ|SHADER_WRITE`
   memory barrier;
4. dispatches B to A;
5. reads A back only after the second dispatch completes;
6. compares every value exactly against two CPU applications of the same
   unsigned transform.

Four full create/use/destroy cycles produced:

- 13 upload/chain/final-readback sequences;
- 39 queue submissions and 39 bounded completions;
- 26 compute dispatches;
- 13 explicit inter-dispatch barriers;
- 13 final readbacks;
- exact parity for every checked value;
- zero live buffers, programs, or submissions before every session close.

Two independent reports matched in every non-timing field.

## Reuse and lifecycle evidence

Within each lifecycle, the proof reuses:

- one staging buffer for upload and readback;
- two device-local buffers across every chain;
- two persistent descriptor sets and pipelines;
- one command pool;
- one command buffer;
- one fence.

Typed fixtures prove:

- null and double session initialization: `invalid_argument`;
- oversized transfer: `buffer_range_invalid`;
- empty program bindings: `descriptor_binding_invalid`;
- destruction of a descriptor-referenced buffer: `resource_in_use`;
- session close with live resources: `resource_in_use`.

The zero-timeout fixture submits a chain, returns
`fence_wait_timeout` without an unbounded drain, exposes `in_flight=true`,
refuses close with `resource_in_use`, and recovers through a later bounded
`vk_runtime_compute_session_wait`.

## Validation and timing interpretation

Khronos validation was required and enabled through all four lifecycles,
timeout recovery, resource destruction, runtime close, and report emission:

- warnings: `0`;
- errors: `0`.

The report separates aggregate host copy, upload wait, dispatch wait, and
readback wait instrumentation. These timings prove measurement plumbing only.
They are not a performance, crossover, dedicated-transfer-queue, or native
Linux Vulkan claim.

## Reproduction

```sh
make -C shared/vk_runtime test-resident-live
make -C shared/vk_runtime test-resident-live-validation
```

The live tests require Metal/MoltenVK access and may fail in a GPU-restricted
sandbox even when the host GPU is healthy.
