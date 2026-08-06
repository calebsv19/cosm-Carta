# Apple M2 / MoltenVK S4 Timing Profile

Observed: 2026-07-26
Module: `vk_runtime 0.4.0`
Schema: `codework_gpu_timing_report_v1`

## Platform and timestamp contract

- operating system: `macos`;
- architecture: `arm64`;
- device: `Apple M2`;
- device UUID: `0000106b0f0702080000000000000000`;
- vendor ID: `4203`;
- device ID: `252117512`;
- selected compute queue family: `0`;
- timestamp valid bits: `64`;
- timestamp period: `1 ns`;
- timestamp measurements: `189`.

The session owns one persistent two-slot query pool. Each upload, dependent
dispatch chain, and readback resets the pair and records timestamps at
`TOP_OF_PIPE` and `BOTTOM_OF_PIPE`. These values measure the complete recorded
GPU command sequence for that operation, not host submission or fence latency.

## Workload and correctness

The proof sweeps:

`256, 1024, 4096, 16384, 65536, 262144, 1048576`

unsigned values. Every sample performs one upload, two dependent transform
dispatches with one explicit compute barrier, and one final readback. Each size
uses two warmups and seven measured samples. CPU timing is batched over at
least 16,777,216 operated values and reported per operation. All durations use
the median.

All seven sizes matched the CPU oracle exactly. Across the complete proof:

- submissions/completions: `189 / 189`;
- uploads/readbacks: `63 / 63`;
- dispatches: `126`;
- barriers: `63`;
- timestamp measurements: `189`;
- live work remaining at close: none.

Two independent reports matched after excluding measured duration values and
their derived crossover fields.

## Timing interpretation

The validation-required sample recorded no execution-only or end-to-end
crossover within the tested size ladder. This is a valid measured result, not
a test failure. Individual durations varied materially between independent
runs, so the profile does not freeze nanosecond values as portable constants.

The important contract is the separation of:

- CPU reference execution;
- host upload copy;
- upload submit/fence wait;
- upload GPU commands;
- dependent compute submit/fence wait;
- dependent compute GPU commands;
- download submit/fence wait;
- download GPU commands;
- host readback copy;
- combined transfer GPU time;
- total end-to-end wall time.

This profile does not claim that GPU execution is universally faster, that
MoltenVK results predict native Vulkan behavior, or that an application should
adopt this kernel. Crossover is workload-, driver-, device-, and run-specific.

## Validation and reproduction

Khronos validation was required and enabled:

- warnings: `0`;
- errors: `0`.

```sh
make -C shared/vk_runtime test-timing-live
make -C shared/vk_runtime test-timing-live-validation
```

The tests require Metal/MoltenVK access and may fail in a GPU-restricted
sandbox even when the host GPU is healthy.
