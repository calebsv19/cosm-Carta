# Linux RTX 3060 Host-Visibility Inventory

Observed: 2026-07-27
Host: openSUSE Tumbleweed `20260524`, `x86_64`
Scope: read-only host and Vulkan-loader inventory
Runtime execution: not performed

## Purpose

This record explains why the canonical Linux S4 proof enumerates only
llvmpipe. It is host-readiness evidence, not a hardware-backed Vulkan profile.

The bounded report-inbox request was:

`linux-pc-vk-runtime-s4-hardware-validation-inventory-20260726e`

Its retained terminal reply is:

`_private_workspace_artifacts/codework_report_inbox/linux-pc-vk-runtime-s4-hardware-validation-inventory-20260726e/vps/reply_body.md`

## Physical GPU and kernel state

- PCI device: NVIDIA GA106 GeForce RTX 3060 `[10de:2503]`;
- PCI address: `07:00.0`;
- kernel driver: `nvidia`;
- loaded modules: `nvidia`, `nvidia_drm`, `nvidia_modeset`, `nvidia_uvm`;
- driver version: NVIDIA open kernel module `580.159.03`;
- host DRM objects visible through sysfs: `card1`, `renderD128`;
- connected output: HDMI, `1920x1080@60`.

This proves physical GPU and kernel-driver presence. It does not prove that
Vulkan can enumerate the hardware.

## Execution-environment boundary

The bounded Codex execution environment reported:

- `systemd-detect-virt`: `container-other`;
- `/dev/dri`: absent;
- `/dev/nvidia*`: absent;
- current user not a member of `video` or `render`.

Sysfs and procfs expose the host GPU identity, but the device nodes needed by
the installed NVIDIA userspace driver are not exposed to this execution
environment.

## Vulkan loader and ICD state

Installed ICD manifests:

- `/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`
  -> `/usr/lib64/libvulkan_lvp.so`;
- `/usr/share/vulkan/icd.d/nvidia_icd.x86_64.json`
  -> `/usr/lib64/libGLX_nvidia.so.0`.

Both referenced libraries exist and the NVIDIA library dependencies resolve.
Normal `vulkaninfo --summary` enumerates only llvmpipe as
`PHYSICAL_DEVICE_TYPE_CPU`. Selecting the NVIDIA manifest alone returns
`ERROR_INCOMPATIBLE_DRIVER`; the loader cannot obtain a usable instance path
from the NVIDIA ICD in this device-node-isolated environment.

No read-only process environment override can expose the hardware device while
the required device nodes remain absent.

## Validation state

`VK_LAYER_KHRONOS_validation` is not installed:

- no layer manifest;
- no `libVkLayer_khronos_validation.so`;
- no matching installed package record;
- no validation layer in `vulkaninfo`.

This is distinct from an installed-but-undiscoverable or unloadable layer.

## Live outer-runner result

The fixed host-level lane was installed and executed through exact helper
`/srv/codework-inbox/bin/inspect_linux_pc_vulkan_host.py` through the read-only
outer-runner profile `linux_pc_vulkan_host_inventory`. Helper install thread
`linux-pc-vulkan-host-helper-install-20260726a` and proof thread
`linux-pc-vk-runtime-s4-host-hardware-proof-20260726f` both completed
successfully. The installed helper read back at mode `0755`, passed
`py_compile`, and retained helper payload SHA-256
`1c78577591080d3069ff272166396e47c3a3eaec731db0b4a9ee5a11717c7740`.

Unlike the earlier bounded Codex environment, this outer runner reported
`systemd-detect-virt: none` and saw all expected device nodes:
`/dev/dri/card1`, `/dev/dri/renderD128`, `/dev/nvidia0`,
`/dev/nvidiactl`, `/dev/nvidia-modeset`, and `/dev/nvidia-uvm*`.
The physical GPU and `nvidia` kernel binding are therefore visible from this
lane.

Hardware Vulkan is still not usable. The fixed classification was:

- `device_nodes_present: true`;
- `hardware_vulkan_visible: false`;
- `hardware_blocker: nvidia_icd_failed`;
- `hardware_s4_ready: false`;
- `validation_blocker: validation_layer_not_installed`.

Forced NVIDIA `vulkaninfo --summary` failed with
`ERROR_INCOMPATIBLE_DRIVER` because the loader could not obtain
`vkCreateInstance` through `/usr/lib64/libGLX_nvidia.so.0`. Normal enumeration
reported only llvmpipe. `nvidia-smi` separately failed with
`Failed to initialize NVML: Insufficient Permissions`; the runner user is not
in the device-node owning groups. These are live host driver/access findings,
not an absence of physical hardware or device nodes.

## Remaining proof boundary

Hardware-backed Linux S4 now requires a separately bounded repair/readback
lane for NVIDIA ICD usability and runner device access. Validation installation
remains a distinct later authority boundary. The prepared hardware runner must
not execute until a read-only rerun classifies both
`hardware_vulkan_visible=true` and `hardware_s4_ready=true`.

That subsequent run is now implemented locally as fixed profile
`linux_pc_vk_runtime_s4_host_proof`. It binds corrected item `20260726d`,
root-manifest SHA-256
`435f274ecf0e586c7ffb59d67e3db637acba3614f5384d229f5fcdbb0676a1b7`,
the byte identity of all 48 manifest-listed files through payload-tree
SHA-256
`0a5d02a5872b806f0ba631b4dfa7e8557f1d7b9d2fff4b36ee86547950112f78`,
the NVIDIA ICD, a fresh evidence root, and a fixed compiler-free gate list.
It preserves the canonical payload and retained llvmpipe evidence. Exact
runner-install thread
`linux-pc-vk-runtime-s4-host-runner-install-20260726g` and execution thread
`linux-pc-vk-runtime-s4-host-execution-proof-20260726g` remain
`mac_prepared`, unuploaded, and unexecuted.

Because that evidence root is immutable, the revised order is NVIDIA
driver/access repair, read-only hardware inventory, separately authorized
exact validation-package installation, independent validation readback, then
one combined hardware-plus-validation S4 execution.
Fixed install thread
`linux-pc-vk-runtime-s4-validation-install-20260726h` permits only
`vulkan-validationlayers` through non-interactive `zypper --no-refresh`;
read-only thread
`linux-pc-vk-runtime-s4-validation-readback-20260726h` must prove its package,
manifest, shared library, and loader visibility. Both remain `mac_prepared`.

Validation-clean Linux S4 separately requires authority to install the
appropriate Khronos validation-layer package and read back its manifest,
library, and loader visibility. Neither change was performed by this
inventory.
