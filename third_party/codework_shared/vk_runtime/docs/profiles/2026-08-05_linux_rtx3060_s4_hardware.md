# Linux RTX 3060 S4 Hardware Profile

Date: 2026-08-05
Host: openSUSE Tumbleweed, x86_64
Runtime source: exact exported `vk_runtime 0.5.0` portability bundle
Status: canonical checksum-bound physical-hardware S4 proof

## Source and execution identity

- immutable item: `20260805T232713Z--vk-runtime-s4-linux-native-20260805a`
- manifest SHA-256: `e5e2b422b4fade537f30cd09398af33411dfb11cddd271dd0cb6b2fd82eb04ff`
- 49-file payload-tree SHA-256: `73f7abc1bf39733d6434fd25ff2b9f118fb9ca9eaa7632f62ab5e2461ecb45ee`
- strict helper SHA-256: `42ecfe71052a13590dec3ba302f6a9dd49727c72ded49d823f37ae25bb29d264`
- report-inbox thread: `linux-pc-vk-runtime-s4-proof-20260805c`
- run id: `host-s4-hardware-proof-20260805c`
- independently recomputed result SHA-256: `dc791c5f3f7bff314054bac11b460b3943c045224a2096da93f474c565f87f95`

The host validation prerequisite was independently read back before execution:
`VK_LAYER_KHRONOS_validation 1.4.357`, its explicit-layer manifest, and its
shared library were visible to both normal and NVIDIA-only loader probes.

## Selected hardware

- device: NVIDIA GeForce RTX 3060, discrete GPU
- vendor/device: `0x10de` / `0x2503`
- driver: NVIDIA `580.159.03`
- device UUID: `5f08efe46cd9e5091b3640a6564c808e`
- compute queue family: `2`
- Vulkan device API: `1.4.312`; negotiated runtime API: `1.2.0`
- primary device-local heap: 12 GiB
- timestamp support: 64 valid bits, 1 ns period

## Proof results

All 12 fixed gates completed with exit status zero: capability and repeat,
validation-required capability, one-shot compute and repeat,
validation-required compute, resident compute and repeat,
validation-required residency, timing and repeat, and validation-required
timing.

- S2: exact unsigned parity across 257 values and finite-float parity across
  257 values within absolute/relative tolerance `1e-6`.
- S3: four lifecycle cycles, 13 resident chains, 26 dependent dispatches, 13
  explicit barriers, 13 final readbacks, 39 completed submissions, exact
  1,025-value parity, bounded timeout recovery, and zero final live resources.
- S4: seven workloads from 256 through 1,048,576 values, exact parity at every
  size, 189 timestamp measurements, and deterministic repeat comparison.
- Validation: capability, compute, residency, and timing each ran with
  validation enabled and reported zero warnings and zero errors.

Execution-only crossover was first observed at 16,384 values. End-to-end
crossover was not observed through 1,048,576 values. This profile therefore
proves functional hardware execution and measurement integrity; it does not
justify a general application speedup claim.

## Retention and interpretation

The exact fetched execution envelope is retained at
`_private_workspace_artifacts/vk_runtime_s4_linux_pc/retained/host-s4-hardware-proof-20260805c/source_exec_output.md`.
Its SHA-256 is
`3e2768789327d8b1daf568ea5a78aa318b461bb471065eedf1c6a359885a8497`.
The result document and 12 canonicalized JSON reports were independently
validated with the exact `0.5.0` validators from the transported bundle.
Canonicalized report bytes are not represented as the original remote report
byte streams; those original documents remain embedded in the exact execution
envelope.

This closes the physical RTX/Linux S4 hardware-proof boundary. Local shared
source has since advanced to `vk_runtime 0.6.0` for the renderer lifecycle
integration; this profile intentionally remains bound to the exact pre-rebase
`0.5.0` source bundle that was executed.
