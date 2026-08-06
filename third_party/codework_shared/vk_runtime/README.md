# CodeWork Vulkan Runtime

`vk_runtime` is the shared, SDL-independent Vulkan foundation for CodeWork.
It discovers and selects devices, owns a headless logical-device lifecycle,
reports deterministic capabilities, executes bounded one-shot compute, and
owns reusable device-local buffer/compute sessions.

Current version: `0.6.0`

`VERSION` is the canonical module identity. The build injects that value into
the library as `VK_RUNTIME_BUILD_VERSION`, and reports obtain it through
`vk_runtime_version_string()`. Validators read the same file. This prevents a
payload version transition from silently leaving capability, compute,
residency, or timing evidence on an older module identity.

## Implemented S1 through local S4 boundary

S1 provides:

- Vulkan loader/API negotiation;
- MoltenVK portability enumeration;
- optional or required `VK_LAYER_KHRONOS_validation`;
- debug-utils warning/error accounting through teardown;
- headless device, queue, memory, feature, extension, subgroup, and driver
  discovery;
- deterministic device selection and capability JSON;
- typed runtime and device-rejection failures.

S2 adds:

- one-shot host-visible/coherent storage-buffer allocation and mapping;
- upload, sequential descriptor bindings, compute pipeline, command
  pool/buffer, bounded fence wait, readback, and reverse teardown;
- exact unsigned-integer CPU/GPU parity;
- finite floating-point parity with explicit absolute/relative tolerance;
- typed invalid shader and invalid binding fixtures;
- versioned shader manifests binding source, compiler, flags, descriptor
  layout, entry point, workgroup size, and SPIR-V SHA-256;
- deterministic compute evidence outside measured timing fields.

S3 adds:

- explicit host-visible/coherent staging and device-local buffer roles;
- persistent device-buffer, descriptor-set, pipeline, command-pool,
  command-buffer, and fence ownership;
- reusable upload and final-readback transfers through caller-selected staging;
- multiple dependent dispatches recorded into one submission;
- an explicit compute-shader write to compute-shader read/write memory barrier
  between every dependent dispatch;
- one exclusive compute queue-family owner for transfer and compute commands;
- bounded waits that preserve timed-out submissions as in-flight state for
  explicit recovery rather than draining indefinitely;
- resource reference counts and close refusal while buffers, programs, or
  submissions remain live;
- typed buffer-range, resource-in-use, and device-loss diagnostics;
- deterministic `codework_gpu_residency_report_v1` evidence.

S4 adds:

- one persistent two-slot timestamp query pool per compute session when the
  selected queue family reports timestamp support;
- valid-bit wrap handling and conversion through the physical device's
  `timestampPeriod`;
- GPU elapsed time on upload, dependent compute, readback, and recovered
  timeout results;
- separated CPU reference, host-copy, submit/fence-wait, GPU execution,
  transfer-GPU, and total-wall timing;
- a seven-size deterministic sweep from 256 through 1,048,576 unsigned values
  with two warmups and seven measured samples per size;
- exact CPU/GPU parity, median aggregation, timestamp/resource accounting,
  and `codework_gpu_timing_report_v1`;
- explicit execution-only and end-to-end crossover results, including a
  truthful `not observed` outcome.

S4 platform portability hardening adds:

- opt-in consumption of compiler-produced SPIR-V on hosts without `glslc`;
- SHA-256 binding for shader source, SPIR-V, and the per-kernel manifest;
- deterministic `codework_vk_prebuilt_shader_bundle_v1` export metadata;
- validation of fixed kernel identities, safe artifact filenames, source
  parity, SPIR-V parity, and cross-manifest parity before dispatch;
- tamper fixtures for source, SPIR-V, and shader-manifest changes;
- a compiler-free `SHADER_MODE=prebuilt` lane that survives `make clean`
  without weakening the default compile path.

The S5 presentation bridge adds an SDL-independent two-stage lifecycle:

- `vk_runtime_initialize_instance(...)` accepts caller-required instance
  extensions before any surface exists;
- `vk_runtime_initialize_device(...)` selects graphics/present queues against
  a caller-owned `VkSurfaceKHR` and enables caller-required device extensions;
- the original `vk_runtime_initialize(...)` remains the compatibility wrapper
  for headless compute callers.

Still not implemented:

- SDL surfaces or swapchain ownership (these remain `vk_renderer` concerns);
- images, semaphores, cross-queue transfers, non-coherent staging fallback,
  allocator suballocation, or application workloads;
- worker inventory or program subtree rollout.

The Apple M2/MoltenVK S4 proof, canonical Linux llvmpipe compiler-free
profile, and validation-clean physical RTX 3060 profile are complete. The
earlier Linux PC host inventory records its recovery-era RTX 3060 state,
installed NVIDIA ICD, missing proof-environment device nodes, and absent
Khronos validation layer under
`docs/profiles/2026-07-27_linux_rtx3060_host_visibility.md`; it is superseded
for current readiness by
`docs/profiles/2026-08-05_linux_rtx3060_s4_hardware.md`. Supported Intel macOS
evidence remains required before the wider multi-platform S4
platform matrix is closed.

## Build and test

```sh
make -C shared/vk_runtime
make -C shared/vk_runtime test
make -C shared/vk_runtime test-sanitize
make -C shared/vk_runtime test-live
make -C shared/vk_runtime test-live-validation
make -C shared/vk_runtime test-shaders
make -C shared/vk_runtime test-compute-live
make -C shared/vk_runtime test-compute-live-validation
make -C shared/vk_runtime test-resident-live
make -C shared/vk_runtime test-resident-live-validation
make -C shared/vk_runtime test-timing-live
make -C shared/vk_runtime test-timing-live-validation
make -C shared/vk_runtime test-prebuilt-shaders
make -C shared/vk_runtime test-prebuilt-live
```

`test-live` requires byte-identical capability reports from two independent
lifecycles. `test-compute-live` runs the integer and float kernels twice and
compares every non-timing field. Required-validation variants require the
Khronos layer to load and produce zero warnings and errors.

`test-resident-live` performs four complete session lifecycles and thirteen
resident chains. Every chain uploads once, runs two dependent dispatches with
one explicit barrier, and reads back only after the final dispatch. The proof
reuses staging, device buffers, descriptor/pipeline state, command state, and
fence state; compares 1,025 unsigned values exactly; verifies deterministic
non-timing evidence across independent runs; and requires all live resource
counts to return to zero.

`test-timing-live` reuses one max-sized staging allocation, two device-local
buffers, two programs, and one session across seven workload sizes. It runs
two warmups and seven measured samples per size, requires exact parity, and
compares repeated reports after excluding measured durations and derived
crossover fields. The required-validation variant additionally requires zero
validation warnings/errors.

Homebrew's macOS validation manifest names its dylib without an absolute path.
The validation targets resolve the installed Homebrew library directory and
set `DYLD_LIBRARY_PATH` only for the bounded test process. Normal
initialization never mutates process environment.

The Vulkan loader is resolved through `pkg-config vulkan`, with the existing
Apple Silicon Homebrew fallback used by `vk_renderer`. Shaders are compiled
with `glslc --target-env=vulkan1.1 -O`.

The default `SHADER_MODE=compile` behavior remains unchanged. To export the
currently compiled shaders into the portable artifact lane:

```sh
make -C shared/vk_runtime export-prebuilt-shaders
```

This writes deterministic artifacts to `prebuilt/shaders/` by default. A
compiler-free host consumes them explicitly:

```sh
make -C shared/vk_runtime \
  SHADER_MODE=prebuilt \
  PREBUILT_SHADER_DIR=prebuilt/shaders \
  test-compute-live test-resident-live test-timing-live
```

The consumer validates the bundle and each per-kernel manifest before running
Vulkan. It checks the current GLSL source digest as well as the transported
SPIR-V and manifest digests. It never silently falls back from `compile` to
`prebuilt`; selecting transported executable shader code is an explicit build
decision.

## Public API

- `vk_runtime_version_string`
- `vk_runtime_config_defaults`
- `vk_runtime_initialize`
- `vk_runtime_close`
- `vk_runtime_get_capability_report`
- `vk_runtime_capability_report_to_json`
- `vk_runtime_compute_dispatch`
- `vk_runtime_compute_session_initialize`
- `vk_runtime_compute_session_get_info`
- `vk_runtime_compute_session_wait`
- `vk_runtime_compute_session_dispatch`
- `vk_runtime_compute_session_close`
- `vk_runtime_resident_buffer_create`
- `vk_runtime_resident_buffer_get_info`
- `vk_runtime_resident_buffer_upload`
- `vk_runtime_resident_buffer_readback`
- `vk_runtime_resident_buffer_destroy`
- `vk_runtime_compute_program_create`
- `vk_runtime_compute_program_destroy`
- `vk_runtime_shutdown`

Callers can require graphics, compute, or transfer queues independently. The
default profile requires compute and creates a logical device without
requiring presentation, SDL, or a surface.

`vk_runtime_close` destroys Vulkan handles while retaining final validation
and capability evidence.

`vk_runtime_compute_dispatch` accepts caller-owned SPIR-V, sequential
storage-buffer bindings, workgroup counts, and a nanosecond fence timeout. It
returns typed Vulkan failure evidence and host upload, submit/wait, and
readback durations. Caller upload/readback storage remains caller-owned; all
temporary Vulkan objects are destroyed inside the call.

If the bounded fence observation returns `VK_TIMEOUT`, S2 drains that finite
one-shot fence before resource destruction and still returns
`fence_wait_timeout`. Prompt cancellation/device-loss recovery is an S3
lifecycle contract, not an S2 guarantee.

The S3 session API uses opaque public handles with explicit lifecycle order:

1. initialize the runtime and compute session;
2. create one reusable host-staging buffer and one or more device-local
   buffers;
3. create persistent programs whose descriptor sets reference device-local
   buffers;
4. upload, dispatch a dependent chain, then read back only the final buffer;
5. destroy programs before their referenced buffers;
6. destroy all buffers before closing the session;
7. close/shutdown the runtime.

All session transfers and dispatches execute on the selected compute queue
family with exclusive buffer ownership. The inter-dispatch barrier is
`COMPUTE_SHADER/SHADER_WRITE` to
`COMPUTE_SHADER/SHADER_READ|SHADER_WRITE`.

When timestamps are supported, every session submission resets the persistent
query pair and records timestamps at `TOP_OF_PIPE` and `BOTTOM_OF_PIPE`.
`gpu_elapsed_ns` therefore brackets the complete recorded queue command
sequence for that upload, dispatch chain, or readback. It is distinct from
`submit_wait_ns`, which includes host submission and fence observation.
Callers must check both `gpu_timestamp_supported` and `gpu_timestamp_valid`;
unsupported queues remain usable but do not produce GPU elapsed time.

A session dispatch with `timeout_ns == 0` submits work but deliberately makes
no completion observation. It returns `fence_wait_timeout`, retains
`in_flight=true`, refuses resource/session destruction, and can be recovered
through `vk_runtime_compute_session_wait` with another bounded timeout.
Nonzero timeouts call the Vulkan fence wait directly. Any observed
`VK_ERROR_DEVICE_LOST` is translated to `device_lost`.

## Ownership

`vk_runtime` is non-core shared infrastructure:

- it owns Vulkan-specific device, resource, one-shot, and persistent
  buffer-compute mechanics;
- `vk_renderer` continues to own surfaces, swapchains, graphics presentation,
  textures, and current SDL compatibility;
- `kit_render` remains backend-neutral;
- applications own workload semantics, CPU oracles, fallback, and selection.

Schemas:

- `docs/codework_gpu_capability_report_v1.schema.json`
- `docs/codework_vk_shader_manifest_v1.schema.json`
- `docs/codework_vk_prebuilt_shader_bundle_v1.schema.json`
- `docs/codework_gpu_compute_report_v1.schema.json`
- `docs/codework_gpu_residency_report_v1.schema.json`
- `docs/codework_gpu_timing_report_v1.schema.json`

Typed failure contract:
`docs/FAILURE_CONTRACT.md`
