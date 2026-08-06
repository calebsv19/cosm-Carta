# vk_runtime Typed Failure Contract

The public initializer returns a `VkRuntimeStatus`; the capability report also
retains that status and the closest underlying `VkResult`.

| Status | Meaning |
| --- | --- |
| `ok` | Requested discovery/device contract completed |
| `invalid_argument` | Null or contradictory caller input |
| `out_of_host_memory` | Host allocation failed |
| `loader_unavailable` | Vulkan loader entry points were unavailable |
| `api_version_unsupported` | Requested/negotiated API cannot meet the runtime minimum |
| `instance_extension_missing` | A required instance extension was absent |
| `validation_layer_missing` | Validation was required but unavailable |
| `instance_create_failed` | `vkCreateInstance` failed |
| `debug_messenger_create_failed` | Required debug messenger creation failed |
| `no_physical_device` | Instance exposed no physical device |
| `no_suitable_device` | Devices existed but required queue roles were absent |
| `logical_device_create_failed` | Selected device could not create the requested queues |
| `device_wait_failed` | The logical device failed to become idle during close |
| `shader_code_invalid` | SPIR-V size, header, or entry-point input was invalid |
| `shader_module_create_failed` | Vulkan rejected shader-module creation |
| `descriptor_binding_invalid` | Storage-buffer binding metadata was incomplete or out of bounds |
| `buffer_create_failed` | Vulkan storage-buffer creation failed |
| `memory_type_unavailable` | No memory type matched the requested staging or device-local role |
| `memory_allocate_failed` | Allocation or buffer-memory binding failed |
| `memory_map_failed` | Host mapping failed |
| `descriptor_create_failed` | Descriptor layout, pool, or set creation failed |
| `compute_pipeline_create_failed` | Pipeline-layout or compute-pipeline creation failed |
| `command_create_failed` | Command pool/buffer, recording, or fence creation failed |
| `command_submit_failed` | Compute queue submission failed |
| `fence_wait_timeout` | Bounded fence wait expired |
| `fence_wait_failed` | Fence wait failed for a non-timeout reason |
| `timestamp_query_create_failed` | A timestamp-capable session could not create its persistent query pool |
| `timestamp_result_failed` | A completed submission's timestamp results could not be retrieved |
| `resource_in_use` | A submission, program reference, or owned buffer prevents safe teardown |
| `buffer_range_invalid` | A transfer or descriptor range is empty or exceeds its buffer |
| `device_lost` | Vulkan reported `VK_ERROR_DEVICE_LOST` during an S3 operation |
| `serialization_failed` | Output buffer/contract could not hold valid JSON |
| `internal_limit_exceeded` | A Vulkan count exceeded a fixed contract boundary |

Device rejection is independent from runtime failure. Each reported device
contains a `rejection_bits` mask and named rejection array for missing
graphics, compute, or transfer queue roles.

Close destroys Vulkan handles while the report remains available, so teardown
validation and device-wait failures are included in the final evidence.

The S2 compute result carries its own status and closest `VkResult`. A compute
failure does not overwrite the device capability report: discovery/lifecycle
truth and one dispatch outcome are separate evidence surfaces.

On `fence_wait_timeout`, S2 preserves safe destruction by draining the finite
one-shot fence before returning the timeout status. The timeout bounds the
first observation, not total cancellation latency; prompt cancellation and
device-loss recovery remain outside the S2 one-shot contract.

The S3 resident-operation result is separate from both the capability report
and S2 one-shot result. Resident sessions preserve timeout state:

- `timeout_ns == 0` submits without observing the fence and deterministically
  returns `fence_wait_timeout`;
- the session remains in-flight and refuses command reuse, program/buffer
  destruction, or close;
- `vk_runtime_compute_session_wait` performs another bounded observation;
- successful recovery clears in-flight state and increments completed
  submission accounting;
- no S3 path performs an implicit infinite timeout drain;
- `VK_ERROR_DEVICE_LOST` maps to `device_lost`, while injected device loss is
  excluded from live validation because it is not safe or deterministic on the
  current host.

Buffers and programs are reference-counted at the public lifecycle boundary.
A program must be destroyed before any buffer referenced by its descriptor
set. Sessions close only after all programs and buffers are gone and no
submission is in flight.

S4 timestamp behavior is capability-conditional:

- a queue family with zero timestamp-valid bits creates a usable session with
  `timestamp_supported=false`;
- a supported session owns one persistent two-query pool;
- every submission resets and writes both queries, and successful completion
  resolves them with valid-bit wrap handling;
- a timeout keeps the timestamp pending with the submission; a later
  successful bounded wait resolves that same measurement;
- query-pool creation and query-result retrieval have distinct typed failures;
- callers treat `gpu_elapsed_ns` as evidence only when both
  `gpu_timestamp_supported` and `gpu_timestamp_valid` are true.

Validation policy:

- `enable_validation=true, require_validation=false`: use validation when
  installed and report whether it was enabled.
- `require_validation=true`: absence of the Khronos validation layer or debug
  utilities is a typed hard failure.
- validation warnings and errors are counted separately;
- `validation_load_failed` distinguishes an enumerated manifest from a layer
  that the loader could actually load;
- live acceptance fails on validation errors, not merely on validation being
  unavailable when it was optional.
