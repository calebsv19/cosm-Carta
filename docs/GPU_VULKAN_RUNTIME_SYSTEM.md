# CodeWork GPU and Vulkan Runtime System

Status: current-state reference and approved architectural direction
Last verified from source: 2026-08-09
Owning lane: `shared`

## Purpose

CodeWork has both a real Vulkan presentation backend and an independently
proved headless Vulkan runtime. This document separates their current
ownership and defines the boundary future rendering and compute work share.

The system goal is:

> One versioned, capability-driven Vulkan foundation that can discover and
> validate a GPU, support windowed rendering and headless compute, preserve CPU
> reference paths, and be adopted by programs without each program owning
> Vulkan initialization, synchronization, or platform policy.

This is a Vulkan-first architecture, not a Vulkan-only product promise.
MoltenVK is the macOS Vulkan implementation path, native Vulkan is the expected
Linux path, and every optional feature must be selected from observed
capabilities rather than an assumed device model.

## Current System

The checked-in shared layers have distinct responsibilities:

- `shared/vk_runtime` `0.6.0` is the SDL-independent S1-S4 Vulkan foundation
  plus the shared S5 instance/device lifecycle used by presentation. It owns
  staged or headless loader/API negotiation, validation policy, device and queue
  discovery, logical-device creation, typed failures, and deterministic
  capability JSON. Its S2 lane also owns bounded one-shot storage-buffer
  allocation, descriptor/pipeline creation, dispatch, fence wait, readback,
  timing, shader identity, and deterministic CPU/GPU parity evidence.
  Its S3 lane owns persistent device-local buffers, reusable coherent staging,
  persistent descriptor/pipeline and command/fence state, dependent dispatch
  chains, explicit compute barriers, bounded timeout recovery, and resource
  accounting. Its local S4 lane owns persistent timestamp queries, valid-bit
  and timestamp-period conversion, separated host/submit/GPU/wall timing,
  deterministic workload sweeps, and explicit crossover evidence. Its S4
  platform-portability lane exports and explicitly consumes deterministic,
  source-bound precompiled SPIR-V so a proof host does not need a local shader
  compiler.
- `shared/kit_render` is the backend-neutral rendering policy layer. It owns
  frame and command expression, null/test-safe behavior, and the connection
  between application rendering intent and a backend.
- `shared/vk_renderer` is a non-core shared Vulkan/SDL presentation backend.
  It owns the SDL window surface, swapchain lifecycle, graphics command
  submission, synchronization, buffers, textures, meshes, lines, and capture.
  Its `1.3.2` device lifecycle delegates instance/device and graphics/present
  queue ownership to `vk_runtime` while preserving its public device handles as
  compatibility mirrors.
- Each program normally builds from its vendored
  `third_party/codework_shared` snapshot. The canonical shared version and the
  version actually compiled by a program can therefore differ.

`vk_renderer` is not itself a general GPU runtime:

- device creation requires an SDL window and presentation surface;
- device selection requires graphics, present, and swapchain support;
- compute and transfer queue roles are not modeled as first-class contracts;
- `vk_renderer` does not provide headless initialization or a deterministic
  capability report; those now belong to the separate `vk_runtime` foundation;
- it consumes the shared runtime lifecycle but does not expose the runtime's
  compute-session API as renderer drawing policy;
- its presentation shaders still use the ambient renderer build lane rather
  than the new runtime shader manifest contract;
- the automated hidden-window renderer proof now covers validation-enabled
  rendering, nontrivial PPM readback, capture, real extent change, and
  out-of-date swapchain recovery.

The canonical source versions observed on 2026-08-09 are:

- `vk_runtime`: `0.6.0`
- `vk_renderer`: `1.3.2`
- `kit_render`: `0.14.3`

These are source-package versions. They are not Vulkan loader, Vulkan API,
MoltenVK, GPU driver, shader compiler, or device firmware versions.

## Program Adoption Snapshot

The table records the active UI/presentation path and the checked-in
`vk_renderer` snapshot. Presence of a vendored copy does not prove that the
program uses it.

| Program | Current presentation path | Vendored `vk_renderer` | Current interpretation |
| --- | --- | ---: | --- |
| Ball Bounce | Vulkan preferred for menu, seeded-room family, and seeded-pair family; SDL fallback/oracle | live shared `1.3.2` | protected direct-source `vk_runtime 0.6.0` presentation adoption with exact-source binding, validation/readback/resize/capture/2x-Retina proof, CPU-depth recreation, and explicit SDL fallback; other hosts and compute remain unadopted |
| BehaviorSim | Vulkan default; SDL/fisiCs oracle | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with exact-source, validation/readback/resize/Retina/restart, package, Traffic, and Population proof; no compute adoption |
| Connected Mechanics Sim | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with source/package validation, readback, resize/recovery, capture, and material-frame proof; no compute adoption |
| DAW | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption verified with validation/readback/resize/Retina/restart proof; not released and no compute adoption |
| DataLab | Vulkan default; SDL fallback/oracle | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with exact-source validation/readback/resize/capture/restart, package, and real-host proof; no compute adoption |
| Drawing Program | Vulkan default; SDL/fisiCs oracle | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with exact-source, validation/readback/resize/Retina/restart, real-frame, and package proof; no compute adoption |
| Dungeon | Vulkan default; SDL dummy oracle | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with canonical-source, validation/readback/resize/Retina/restart, package, and live indexed-tileset application proof |
| Gravity Orbit Sim | Vulkan default; fisiCs SDL oracle | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with validation/readback/resize/Retina/restart source and package proof; no compute adoption |
| GrowthSim | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with validation/readback/capture/real-resize/Retina/restart and real Mold-frame proof; fisiCs retains the SDL oracle, no compute adoption |
| IDE | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with exact-source, validation/readback/resize/Retina/restart and package proof |
| LineDrawing | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with validation/readback/capture/resize/Retina/restart proof; not released and no compute adoption |
| MapForge | Vulkan preferred, SDL fallback | `1.3.1` | managed `vk_runtime 0.6.0` presentation adoption verified; SDL fallback remains app-owned |
| Memory Console | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with validation/readback/resize/Retina/restart proof |
| PhysicsSim | Vulkan default | `1.3.1` | committed managed `vk_runtime 0.6.0` adoption verified through its shared-device singleton; not released |
| RayTracing | Vulkan default | `1.1.2` | active presentation host; ray compute remains CPU-owned |
| Video Editor | Vulkan default; direct SDL fallback/oracle | `1.3.1` | committed managed `vk_runtime 0.6.0` presentation adoption with checksum-bound validation/readback/resize/Retina/restart, package, and installed-app proof; GPU video composition remains future work |
| Workspace Sandbox | Vulkan default | `1.3.2` | committed managed `vk_runtime 0.6.0` presentation adoption with validation/readback/resize/capture/package proof and packaged shader-root resolution; no compute adoption |

This snapshot is deliberately date-stamped. Before any rollout, re-read the
program build files and vendored `VERSION` files rather than treating this
table as a permanent compatibility claim.

## Target Layering

The selected target boundary is additive:

```text
program rendering intent
        |
        v
shared/kit_render                 backend-neutral policy
        |
        v
shared/vk_renderer                surfaces, swapchains, graphics presentation
        |
        v
shared/vk_runtime   [S4 local]    lifecycle, resident compute, and GPU timing
        ^
        |
shared/vk_compute   [conditional] higher-level reusable kernel policy
```

`shared/vk_runtime` now exists as a non-core shared module. S1 implements:

- Vulkan loader/API negotiation and validation-layer policy;
- physical-device enumeration, stable device identity, feature and extension
  discovery, and selection diagnostics;
- explicit graphics, compute, and transfer queue roles;
- logical-device ownership independent of a window;
- memory-heap discovery;
- debug messages and deterministic error translation;
- headless operation and a machine-readable capability report;
- runtime, driver, device, compiler, and enabled-feature evidence.

S2 adds host-visible coherent storage buffers, mapping, upload, descriptor
sets, compute pipelines, command pools/buffers, submission, bounded fence
wait, readback, reverse teardown, and measured one-shot dispatch evidence.

S3 adds explicit host-staging and device-local buffer roles, persistent
descriptor/pipeline state, reusable command-pool/buffer/fence ownership,
multiple dependent dispatches in one submission, a compute-write to
compute-read/write barrier between dispatches, final-only readback, bounded
in-flight timeout recovery, and deterministic resource accounting. Transfer
and compute commands deliberately use the selected compute queue family, so
the current exclusive buffers have one queue-family owner and require no
ownership transfer.

S4 adds one persistent timestamp-query pair per supported compute session,
wrap-safe conversion through `timestampValidBits` and `timestampPeriod`,
automatic GPU timing for every session upload/dispatch/readback, and a
seven-size resident workload sweep. The timing report separates CPU reference,
host copies, submit/fence waits, GPU upload/execution/download, transfer GPU,
and total wall time. Images, semaphores, cross-queue transfer, suballocation,
and presentation remain later work.

The `0.5.0` portability continuation keeps `SHADER_MODE=compile` as the
default and adds an explicit `SHADER_MODE=prebuilt` consumer. Exported bundles
bind the current GLSL source, SPIR-V, and per-kernel manifest by SHA-256, carry
deterministic bundle metadata, reject unsafe artifact paths and tampering, and
survive `make clean`. This is transport of already compiled Vulkan shader
code, not a runtime fallback or a claim that unverified binaries are safe.

`shared/vk_renderer` remains the rendering/presentation owner and now consumes
that runtime through its staged instance/surface/device lifecycle. Existing
public entry points and Vulkan handle fields remain available as compatibility
wrappers/mirrors so programs can migrate incrementally. Consumers must add and
link the sibling `vk_runtime`. Committed managed presentation adoption is now
proven in Workspace Sandbox (`vk_renderer 1.3.2`) and in MapForge, Memory
Console, PhysicsSim, LineDrawing, DAW, Gravity Orbit Sim, GrowthSim, IDE,
Dungeon, Video Editor/Capture, BehaviorSim, Drawing Program, DataLab, and
Connected Mechanics Sim (`vk_renderer 1.3.1`). PhysicsSim specifically
exercises its existing shared-device singleton; the other hosts retain thin
app-local compatibility backends or established renderer seams.
RayTracing remains an active native `vk_renderer 1.1.2`
presentation consumer without the managed `vk_runtime` lifecycle rebase. These
are presentation and lifecycle adoption claims only;
they do not imply app compute-kernel use or a release/publish state.

A later `shared/vk_compute` may own higher-level reusable kernel/dispatch
policy if multiple adopters prove that boundary. It should not define
application algorithms. A backend-neutral `core_compute` abstraction is
explicitly deferred until at least two or three real workloads expose stable
semantics.

## Platform Profiles

The runtime must report facts instead of choosing behavior from platform names.
Initial validation profiles are:

1. Apple Silicon macOS: Vulkan loader to MoltenVK to Metal.
2. Intel macOS, where supported: Vulkan loader to MoltenVK to Metal.
3. Linux PC: native Vulkan loader and installed GPU driver.
4. Other future hosts: accepted only through the same capability contract.

Each probe record must distinguish:

- CodeWork module and probe-schema versions;
- requested and negotiated Vulkan API versions;
- loader implementation/version;
- MoltenVK implementation/version when present;
- physical-device name, stable identifiers when available, vendor/device IDs,
  device type, and driver name/version;
- queue families and selected queue roles;
- memory heaps, budgets when available, and host/device visibility;
- required and optional extensions/features;
- validation-layer availability and messages;
- shader compiler name/version, source digest, flags, entry point, and SPIR-V
  digest;
- operating-system and architecture identity.

Runtime reports are diagnostic evidence, not a promise that every device will
enable every accelerator.

The first live Apple Silicon profile is now recorded at
`shared/vk_runtime/docs/profiles/2026-07-26_apple_m2_moltenvk.md`.

The S2 Apple M2 proof binds both shader manifests and all 257 results per
kernel in `codework_gpu_compute_report_v1`. Required validation completed with
zero warnings/errors. Integer parity is exact; the finite float policy uses
absolute and relative tolerances of `1e-6`.

The S3 Apple M2 proof records
`codework_gpu_residency_report_v1`: four complete session lifecycles, thirteen
resident chains, twenty-six dispatches, thirteen explicit inter-dispatch
barriers, thirteen final readbacks, reusable staging/device/descriptor/command
state, deterministic zero-timeout recovery, exact 1,025-value integer parity,
zero live resources at each teardown, and required validation with zero
warnings/errors.

The S4 Apple M2 proof records `codework_gpu_timing_report_v1`: 64 valid
timestamp bits at a 1 ns period, 189 timestamped submissions, seven workload
sizes from 256 through 1,048,576 values, exact parity at every size, two
warmups and seven median-aggregated measured samples per size, and required
validation with zero warnings/errors. The validation-required sample observed
no execution-only or end-to-end crossover within that ladder; crossover
absence is retained as truthful platform-specific evidence rather than
converted into a speedup claim. The profile is recorded at
`shared/vk_runtime/docs/profiles/2026-07-26_apple_m2_timing.md`.

## Functional Proof Ladder

GPU availability is not proved by a successful compile or an opened window.
The shared lane must advance in small, independently testable gates:

1. **Toolchain identity** — resolve the loader, headers, shader compiler, and
   generated SPIR-V with recorded versions and hashes.
2. **Discovery** — enumerate devices and queue families without creating a
   window.
3. **Capability determinism** — emit stable, schema-versioned JSON with
   volatile fields either excluded or explicitly classified.
4. **Lifecycle correctness** — create and destroy the runtime with validation
   enabled and no validation errors.
5. **Exact compute** — transform an integer buffer on the GPU and compare every
   value with the CPU oracle.
6. **Numeric compute** — run a floating-point kernel with declared tolerance
   and an explicit exceptional-value policy. S2 proves finite inputs; NaN,
   infinity, signed-zero, and subnormal fixtures remain later numeric
   hardening.
7. **Residency and synchronization** — keep data device-resident across
   multiple dispatches and prove barriers and readback.
8. **Timing** — report upload, GPU execution, download, and total wall time
   separately across increasing workload sizes.
9. **Failure fixtures** — prove deterministic diagnostics for missing
   features, unsuitable devices, invalid shaders, allocation failure, timeout,
   and device loss where safely reproducible.
10. **Application parity** — run one canonical workload through CPU and GPU
    implementations and compare results before performance claims.

The first application workload should be chosen from profiling, regular data
shape, measurable CPU cost, and a strong CPU oracle. A GrowthSim field
transform is an early candidate, not a pre-approved migration. A complete GPU
ray tracer is intentionally not the first workload; ray/AABB, ray/triangle,
and BVH stages can follow after the runtime is proved.

## Adoption Rules

- The CPU implementation remains the semantic oracle and fallback.
- Presentation-backend adoption and compute-backend adoption are separate
  decisions.
- Programs submit canonical inputs; a GPU backend must not invent a second
  incompatible scene or simulation format.
- No program may select a GPU path only because `vk_renderer` is vendored.
- GPU paths must have explicit capability requirements and a deterministic
  unsupported/fallback result.
- Transfer, dispatch, and total wall time must be measured before claiming a
  speedup.
- Work should remain on the device across useful stages when possible; repeated
  upload/readback can erase compute gains.
- Shared changes originate in `shared/`, receive their own version and tests,
  and roll into programs through the shared-subtree workflow.
- Program rollout is incremental. One verified adopter is preferred to a
  workspace-wide renderer migration.

## Version and Compatibility Contract

When `vk_runtime` and any later `vk_compute` module are created, each receives
its own SemVer `VERSION`, changelog/current-state documentation, and test
surface. Compatibility must be recorded across distinct identities:

| Identity | Why it is separate |
| --- | --- |
| CodeWork module version | API/ABI and behavior owned by CodeWork |
| capability schema version | machine-readable report compatibility |
| Vulkan requested/negotiated API | API contract with the implementation |
| loader/runtime version | installed Vulkan implementation truth |
| MoltenVK version | macOS translation implementation truth |
| driver/device identity | host-specific execution behavior |
| shader toolchain and SPIR-V digest | reproducible kernel identity |
| program vendored version | code actually compiled by an adopter |

No single “Vulkan version” field is sufficient.

## Current Stop Boundary

The 2026-08-05 exact Linux PC run materially supersedes the stale pre-recovery
blocker narrative retained below. Immutable item
`20260805T232713Z--vk-runtime-s4-linux-native-20260805a` bound 49 source files
at manifest SHA-256
`e5e2b422b4fade537f30cd09398af33411dfb11cddd271dd0cb6b2fd82eb04ff`
and payload-tree SHA-256
`73f7abc1bf39733d6434fd25ff2b9f118fb9ca9eaa7632f62ab5e2461ecb45ee`.
On the physical RTX 3060 with NVIDIA `580.159.03`, capability, contracts,
sanitizers, prebuilt-shader checks, deterministic compute parity, persistent
residency, timestamp timing, and repeat comparisons passed. After the exact
`vulkan-validationlayers` prerequisite was installed and independently read
back, strict run `host-s4-hardware-proof-20260805c` passed all 12 fixed gates.
Capability, compute, residency, and timing validation reports each recorded
zero warnings and zero errors. The independently recomputed result SHA-256 is
`dc791c5f3f7bff314054bac11b460b3943c045224a2096da93f474c565f87f95`.
This closes the canonical physical RTX/Linux S4 boundary.

Separately, the committed S5 source is now `vk_runtime 0.6.0` and
`vk_renderer 1.3.2`. Clean runtime build/tests/sanitizers and all four
validation-required Apple M2 live lanes pass. The renderer hidden-window proof
passes with validation enabled and zero errors, compatibility-handle parity,
two nontrivial captures, an actual resize, and injected out-of-date recovery.
The managed application rollout listed above has since landed program by
program; this does not imply compute adoption, release, or publication.

The retained profile is
`shared/vk_runtime/docs/profiles/2026-08-05_linux_rtx3060_s4_hardware.md`.
Execution-only crossover first appeared at 16,384 values; end-to-end crossover
was not observed through 1,048,576 values. No application speedup is claimed.
The July diagnostics below are preserved as historical recovery evidence and
are superseded for current host readiness by this August closure.

S1 capability/lifecycle, S2 deterministic one-shot compute, S3 resident buffer
compute, and the Apple M2/MoltenVK portion of S4 timing are implemented and
locally proved. The exact `0.4.0` Linux source bundle
`20260726T220642Z--vk-runtime-s4-linux-native-20260726b` was uploaded and run
through the bounded Linux PC lane. Linux compilation, contract tests,
ptrace-adjusted ASan/UBSan, deterministic S1, and compilation of the S2-S4
test executables passed. That evidence selected Mesa llvmpipe as a CPU Vulkan
device; S2-S4 execution was blocked because no Vulkan shader compiler was
installed, and validation-required variants were blocked because the Khronos
layer was absent.

`vk_runtime 0.5.0` closes the shader-compiler portability gap with
deterministic, source-bound precompiled artifacts. Authorized Linux item
`20260727T003427Z--vk-runtime-s4-linux-native-20260726c` reproduced its exact
manifest and passed build, contracts, adjusted ASan/UBSan, prebuilt validation,
deterministic S1, compiler-free S2 compute, S3 residency, and S4 timing on
llvmpipe CPU Vulkan without any shader toolchain installed. Exact parity held;
timestamp accounting completed; neither execution-only nor end-to-end
crossover was observed. Khronos validation remained blocked by a missing host
prerequisite, and llvmpipe is software portability evidence rather than
hardware-GPU proof.

That run also exposed a stale report-identity literal: its payload `VERSION`
was `0.5.0`, but reports emitted `module_version: 0.4.0`. The runtime now
derives its library/report identity from the canonical `VERSION` file, and all
local compiled, prebuilt, and validation-required reports emit `0.5.0`. The
authorized corrected 48-file item
`20260727T024221Z--vk-runtime-s4-linux-native-20260726d` reproduced manifest
SHA-256
`435f274ecf0e586c7ffb59d67e3db637acba3614f5384d229f5fcdbb0676a1b7`
and reran the complete bounded Linux proof. Every generated S1-S4 report
emitted canonical `module_version: 0.5.0`; build, contracts, adjusted
ASan/UBSan, prebuilt validation, deterministic S1, compiler-free S2 compute,
S3 residency, and S4 timing passed again with exact parity. The host still
selected llvmpipe, the Khronos layer remained absent, and neither crossover
mode was observed. This closes canonical Linux software-portability identity,
but a hardware-backed Linux device and supported Intel macOS evidence are
still pending.

The follow-up read-only host inventory identified an NVIDIA GeForce RTX 3060
at PCI `07:00.0`, bound to NVIDIA driver `580.159.03`, with host sysfs entries
for `card1` and `renderD128`. The bounded Codex execution environment is
classified as `container-other` and exposes neither `/dev/dri` nor
`/dev/nvidia*`; its user also lacks `video` and `render` membership. The
installed NVIDIA ICD is therefore discoverable but unusable, while the
llvmpipe ICD remains functional. No environment-only invocation can turn that
into hardware Vulkan evidence. The Khronos validation layer is separately
confirmed not installed: its manifest, shared library, and package record are
all absent.

A fixed host-level inventory lane is implemented, contract-tested, and now
completed remotely. It installed only
`/srv/codework-inbox/bin/inspect_linux_pc_vulkan_host.py`, then uses the
read-only `linux_pc_vulkan_host_inventory` outer-runner profile to inspect
device nodes, ICDs, libraries, loader enumeration, and validation readiness.
The exact helper payload SHA-256 is
`1c78577591080d3069ff272166396e47c3a3eaec731db0b4a9ee5a11717c7740`.
Install thread `linux-pc-vulkan-host-helper-install-20260726a` and proof thread
`linux-pc-vk-runtime-s4-host-hardware-proof-20260726f` both completed. The
installed helper read back at the exact authorized digest, mode `0755`, with
successful `py_compile`.

The live outer runner is bare metal (`systemd-detect-virt: none`) and sees
`/dev/dri/card1`, `/dev/dri/renderD128`, and the expected `/dev/nvidia*`
nodes. Hardware Vulkan still fails: forced NVIDIA enumeration reports
`ERROR_INCOMPATIBLE_DRIVER` because `/usr/lib64/libGLX_nvidia.so.0` does not
provide a usable `vkCreateInstance`, while normal enumeration returns only
llvmpipe. `nvidia-smi` independently reports insufficient permissions for the
runner identity, which is not in the device-node owning groups. The fixed
classification is therefore `hardware_blocker=nvidia_icd_failed`,
`hardware_vulkan_visible=false`, and `hardware_s4_ready=false`.

The bounded follow-on hardware execution lane is also source-ready and locally
contract-tested. It improves the earlier transport check by binding all 48
manifest-listed payload files to canonical tree SHA-256
`0a5d02a5872b806f0ba631b4dfa7e8557f1d7b9d2fff4b36ee86547950112f78`,
then copying only those verified files into a fresh evidence root. Its fixed
helper selects the NVIDIA ICD, runs only the predeclared compiler-free S1-S4
and validation gates, rejects ambient compiler/Vulkan overrides, and preserves
the canonical payload plus retained llvmpipe evidence. Helper-install thread
`linux-pc-vk-runtime-s4-host-runner-install-20260726g` and execution thread
`linux-pc-vk-runtime-s4-host-execution-proof-20260726g` remain
`mac_prepared`; execution is blocked until a separately authorized NVIDIA
driver/access repair and read-only rerun prove successful hardware inventory.

The validation prerequisite now has its own exact privileged profile rather
than generic package authority. It can execute only
`sudo -n /usr/bin/zypper --non-interactive --no-refresh install --no-recommends vulkan-validationlayers`;
it cannot refresh/change repositories, select another package, update/remove
packages, override the solver, accept unsigned packages, control services, or
run arbitrary sudo. Install thread
`linux-pc-vk-runtime-s4-validation-install-20260726h` and independent read-only
thread `linux-pc-vk-runtime-s4-validation-readback-20260726h` remain
`mac_prepared`. Validation installation is not the immediate next operation:
the NVIDIA ICD/device-access blocker must be repaired and read back first. The
combined immutable hardware S4 run follows only after both hardware and
validation readbacks prove readiness.

Retained desktop-session proof provides a narrower interpretation of the
headless failure. The same outer-runner user previously succeeded with
`nvidia-smi` and enumerated the NVIDIA GeForce RTX 3060 through Vulkan when
the active X11/session environment was present. CodeWork now has a source-ready
read-only diagnosis that compares clean headless and discovered same-user
desktop-session NVIDIA-only probes. It cannot change drivers, packages,
groups, ACLs, services, configuration, or files.

The exact helper-install and read-only diagnosis have now completed. The
installed helper reproduced SHA-256
`609c57aeb45e0628365107c041e085803c355ef046006783118f9dacc08f756c`,
mode `0755`, and successful `py_compile`. The diagnosis returned
`desktop_session_ready=false`,
`diagnosis=desktop_session_unavailable`,
`environment_bridge_sufficient=false`, and `headless_ready=false`.

The runner `calebsv16` is not a member of `video` or `render` and cannot
read or write the mode-`0660` DRM/NVIDIA control and render nodes owned by
those groups. NVIDIA userspace is present and dependency-complete, but
`nvidia-smi` reports insufficient permissions and NVIDIA-only `vulkaninfo`
cannot create an instance. Therefore the prepared session bridge,
inventory-rerun, and S4-runner subjects are ineligible and remain unuploaded
and unexecuted. The next system-mutation boundary is a separately designed,
exact, idempotent supplementary-group repair followed by a fresh runner/login
context and the same read-only diagnosis. No group or session mutation has
occurred.

CodeWork now has a fixed exact repair profile for
`sudo -n /usr/sbin/usermod -aG video,render calebsv16`. Its first live
execution stopped before mutation because non-interactive sudo requires a
password on this host. The password must not be transported through the
report-inbox lane. The operator must run the same command interactively on the
Linux PC, after which the prepared read-only diagnosis can run through a new
SSH login whose supplementary groups are freshly initialized. No worker
service or desktop-session restart is required for that diagnostic readback.

The operator completed that command. A terminal subprocess-free fresh-login
proof now reports both `video` and `render` membership and read/write access to
all five required DRM/NVIDIA nodes. The group/device-access repair is
therefore complete.

The broader NVIDIA subprocess diagnosis remains open: two post-repair threads
failed to finalize around the NVIDIA probe path, including after process-group
timeout hardening. They are not Vulkan-readiness evidence. A runner-safe
NVML/Vulkan enumeration proof must complete before the hardware inventory,
validation, or S4 lanes resume.

A runner-safe proof now completes reliably and cleans its temporary workspace.
It establishes that `nvidia-smi -L` can print the RTX 3060 but does not exit
within its fixed timeout, while forced-NVIDIA `vulkaninfo --summary` reaches
device creation and fails `ERROR_DEVICE_LOST`. Therefore access is repaired,
but NVIDIA runtime health is not. The conditional inventory rerun did not run;
runtime-health diagnosis/repair must precede inventory, validation, and S4.
The S4 runner additionally requires a fixed NVIDIA-only enumeration preflight
before creating its immutable evidence directory.

A separate read-only runtime-health proof now completes. It confirms matching
NVIDIA `580.159.03` kernel, firmware, and userspace identity; active NVIDIA PCI
binding and runtime power; active `nvidia-persistenced`; and continued
read/write device-node access. It also finds four same-user `nvidia-smi`
processes blocked in uninterruptible `D` state. A new query prints the RTX
3060 identity, driver, P8 state, and PCI address before blocking, while Vulkan
still fails `vkCreateDevice` with `ERROR_DEVICE_LOST`. This strongly narrows
the live blocker to a wedged NVIDIA kernel/driver-GPU interaction.

The later privileged capture resolves that journal uncertainty. There is no
NVIDIA Xid in the retained current-boot evidence: the apparent `XID 641` match
is the Realtek NIC identifier, not an NVIDIA fault. There is likewise no AER
error; the retained PCIe lines are normal enumeration and AER-enable records.
The actual NVIDIA fault is stronger and earlier: during boot the open kernel
module repeatedly asserted that its watchdog GPFIFO was full, then reported
`RC watchdog: GPU is probably locked!`. No recovery action has occurred.

A no-reboot repair-research proof now also confirms that running kernel
`7.0.9-1-default`, its exact signed
`580.159.03_k7.0.9_1` NVIDIA KMP, the loaded `580.159.03` module, and core
`580.159.03` userspace packages are consistent. RPM database verification
passes. This does not support a simple installed kernel/KMP/core-userspace
version mismatch as the leading explanation.

Four NVIDIA clients remain blocked in `D` state specifically in
`nvidia_uvm` teardown at `uvm_va_space_destroy`, while three CodeWork worker
loops and two ray-tracing render workers are live. Privileged Btrfs device
statistics are all zero and the last scrub completed with no errors, so the
helper's broad text classifier must not be interpreted as a Btrfs/I/O fault.
RPM DB verification passes; package-file verification differs only at the
modified configuration file `/usr/lib/modprobe.d/50-nvidia.conf`, whose
contents and packaged baseline have not yet been compared. Snapper is simply
not configured for `root`. The current safe boundary remains observation
only: no further NVML/Vulkan probes and no live repair while workers run.
Reboot is explicitly prohibited.

A separate privileged evidence collector was installed with its exact digest
and executed once by the operator with interactive sudo. It requires EUID
`0`, accepts only `capture`, and uses
fixed non-shell reads for current-boot journals, RPM DB and installed-file
verification, relevant zypp history, Btrfs device/usage/scrub status, Snapper
inventory, module/PCI state, and NVIDIA/CodeWork process wait-channel/kernel
stack evidence. It invokes no NVIDIA client API, package transaction, scrub
operation, snapshot mutation, service/module action, reset, or reboot.

The helper SHA-256 is
`8fd0d1450b6a514bccde5d50a4d3516650231e3136c8b1419e9af9af736c5d87`.
Install thread
`linux-pc-nvidia-privileged-evidence-helper-install-20260728a` verified the
exact helper, mode, and Python syntax. Automated capture thread
`linux-pc-nvidia-privileged-evidence-capture-20260728a` stopped without retry
when `sudo -n` required a password. The operator then ran the documented
one-time interactive invocation. Canonical extracted evidence SHA-256 is
`7d517168b6a71e84c8d58a52c83282a5bb456cc3b0b91946f42d7af315fef296`;
it reports EUID `0`, helper-digest parity, and scratch cleanup. No sudo policy,
repair, inventory, validation, or S4 action occurred.

The following recovery text is historical evidence from the pre-recovery
snapshot and is superseded by the 2026-08-05 reconciliation above: the Linux
PC desktop recovery is closed, and this Vulkan lane must not reopen host repair
without new evidence. There is no credible non-disruptive live recovery in
that historical snapshot.
The stuck clients are uninterruptible kernel waits, restarting persistence
does not address the UVM teardown, and unloading/resetting the display driver
would be disruptive and is not authorized. Preserve the active jobs. After
they finish, first compare the modified NVIDIA modprobe configuration with its
packaged baseline and capture a final read-only process/job state. A controlled
reboot is the likely minimum operation that can clear this boot-scoped GPU
wedge, but it remains a future separately authorized maintenance action, not
part of the current lane.

The prior local Intel handoff was also audited and found to be stale
`vk_runtime 0.4.0` source without the compiler-free artifacts. It remains
historical and must not be uploaded as current S4 evidence. A corrected
local-only `0.5.0` compiler-free archive,
`vk-runtime-s4-intel-macos-20260726b.tar.gz`, is prepared at SHA-256
`0a9181b25f7fe13309a9294b36022f3727347e063e43cf3eaeebb29747189b85`.
Neither documented Intel alias returned host identity, so this archive was not
uploaded or run.

The compatibility-preserving renderer lifecycle rebase and bounded automated
presentation/readback/resize/capture proof are implemented locally. RTX S4
validation and independent retention are now complete. The exact stop boundary
is before selecting or updating a representative program: application subtree
rollout, integration fixes, desktop packaging, commits, and publication remain
separate later work.

Private execution plan:
`docs/private_program_docs/shared/active/2026-07-26_shared_gpu_vulkan_runtime_foundation_plan.md`
