# core_viewport2d

Shared 2D viewport/camera math contract for screen-to-content transforms.

Purpose:
- Keep cursor-pivot zoom, drag pan, and fit-to-window behavior consistent across apps.
- Provide a small pure-math API for 2D content inspection without owning SDL/input policy.

Current status:
- v0.1.0 scaffold implemented.
- First proving host is now live in DataLab sketch/image inspection.
- Future convergence candidates include DrawingProgram canvas view and selected MapForge camera semantics where the generic 2D math cleanly overlaps.
