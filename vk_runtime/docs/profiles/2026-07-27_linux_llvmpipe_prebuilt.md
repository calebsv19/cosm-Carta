# Linux llvmpipe Compiler-Free S4 Portability Profile

Observed: 2026-07-27
Source payload: `vk_runtime 0.5.0`
Report identity: `0.5.0`
Schemas: capability, compute, residency, and timing v1

## Evidence identity

The canonical corrected-identity Linux item was:

`20260727T024221Z--vk-runtime-s4-linux-native-20260726d`

Its root manifest SHA-256 reproduced remotely as:

`435f274ecf0e586c7ffb59d67e3db637acba3614f5384d229f5fcdbb0676a1b7`

The earlier functional `20260726c` run exposed a public-header
`module_version: 0.4.0` literal under `VERSION=0.5.0`. That evidence remains
retained without relabeling. The corrected runtime derives its compiled
identity from `VERSION`, and this canonical rerun proved `module_version:
0.5.0` across capability, required-validation probe, compute, residency, and
timing reports.

## Platform

- operating system: openSUSE Tumbleweed `20260524`;
- architecture: `x86_64`;
- compiler: GCC `15.2.1`;
- Vulkan loader: `1.4.350`;
- negotiated Vulkan API: `1.2.0`;
- selected implementation: `llvmpipe (LLVM 22.1.5, 256 bits)`;
- driver: Mesa `26.1.0` / llvmpipe;
- device type: `cpu`;
- selected graphics/compute/transfer queue family: `0 / 0 / 0`;
- timestamp valid bits: `64`;
- timestamp period: `1 ns`;
- Khronos validation layer: absent;
- shader compiler and SPIR-V tools: absent.

This is a native Linux Vulkan loader/runtime proof using a software CPU
implementation. It is not Linux hardware-GPU proof.

## Compiler-free artifact proof

The host reproduced the transported artifact identities:

- bundle manifest:
  `a96d2b63623302cfae1d7dec8c7c5fae97f2d59b7773c74f0f6c5a2b0b577d2c`;
- unsigned-integer manifest:
  `bc8f654680c543a34ee2b09ab8ac049b7ee87485da4c8d4216885ec64dac44a9`;
- float manifest:
  `db5229fcbca045e5cc640932e1354ddc3adc273f659c206388da3fe119bdea09`;
- unsigned-integer SPIR-V:
  `076935ff67b898e7a80f41a48b0fb5086c3aace461f0ccba2220292d742d9c35`;
- float SPIR-V:
  `daf4614fe9a95530d30323d47ae11242ffbcef914493ea7c4b4c6ab79bb784c8`.

The prebuilt source/SPIR-V/manifest contract passed without installing or
invoking a shader compiler.

## S1 through S4 results

- build and contract tests: pass;
- ordinary LeakSanitizer: blocked by the report-inbox runner's ptrace
  environment;
- ASan/UBSan with leak detection disabled: pass;
- repeated capability reports: byte-identical;
- S2 unsigned and finite-float CPU/GPU parity: exact;
- S3 lifecycle cycles: `4`;
- S3 submissions/completions: `39 / 39`;
- S3 uploads/readbacks: `13 / 13`;
- S3 dispatches/barriers: `26 / 13`;
- S3 final live resource counts: zero;
- S4 timestamp measurements: `189`;
- S4 workload ladder: `256` through `1,048,576` values;
- S4 parity: exact at every size;
- S4 execution-only crossover: not observed;
- S4 end-to-end crossover: not observed;
- validation-required variants: blocked by the absent Khronos layer.

## Interpretation and remaining boundary

This run proves that the explicit compiler-free path can transport and execute
the same canonically identified S1-S4 contracts on a Linux Vulkan
implementation with no shader toolchain installed. It does not prove:

- Linux hardware-GPU visibility;
- Khronos-validation-clean Linux execution;
- an acceleration crossover;
- application adoption.

The retained terminal reply is:

`_private_workspace_artifacts/codework_report_inbox/linux-pc-vk-runtime-s4-proof-20260726d/vps/reply_body.md`
