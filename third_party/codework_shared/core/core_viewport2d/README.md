# core_viewport2d

Shared 2D viewport/camera math contract for screen-to-content transforms.

Purpose:
- Keep cursor-pivot zoom, drag pan, fit-to-window, and optional 2D rotation behavior consistent across apps.
- Provide a small pure-math API for 2D content inspection without owning SDL/input policy.

Current status:
- v0.2.0 additive rotation update implemented.
- First proving host is now live in DataLab sketch/image inspection.
- Future convergence candidates include DrawingProgram canvas view and selected MapForge camera semantics where the generic 2D math cleanly overlaps.

Notes:
- `rotation_rad` is stored in radians on the viewport state.
- Positive rotation rotates content `+X` toward screen `+Y`.
- `screen_to_content`, `content_to_screen`, and anchor-preserving zoom now honor rotation.
- `reset_to_fit` intentionally resets the viewport to an unrotated fit baseline.
