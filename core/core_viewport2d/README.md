# core_viewport2d

Shared 2D viewport/camera math contract for screen-to-content transforms.

Purpose:
- Keep cursor-pivot zoom, drag pan, fit-to-window, and optional 2D rotation behavior consistent across apps.
- Provide a small pure-math API for 2D content inspection without owning SDL/input policy.

Current status:
- v0.2.2 truth-lock and edge-policy coverage in place, including Linux-safe internal pi constants so strict C11 builds no longer depend on libc exposing `M_PI`.
- Source-level proving hosts are now live in DataLab sketch/image inspection, DrawingProgram canvas/document viewport bridges, MapForge camera target/rotation bridging, and GravityOrbitSim world camera math.

Notes:
- `rotation_rad` is stored in radians on the viewport state.
- Positive rotation rotates content `+X` toward screen `+Y`.
- `screen_to_content`, `content_to_screen`, and anchor-preserving zoom now honor rotation.
- `reset_to_fit` intentionally resets the viewport to an unrotated fit baseline.

Current contract notes:
- The shared surface owns only pure viewport math; input gestures, projection/world-unit conversion, target smoothing, persistence, sampling, and rendering remain host-owned.
- `core_viewport2d_clamp_zoom(...)` falls back to internal default bounds when the viewport pointer is null or when the viewport bounds are invalid.
- The transform helpers normalize finite rotation internally before applying math; hosts may still store any finite radians value on the struct.
- Invalid input paths return an error without mutating viewport state, and transform failures leave caller output values unchanged.
