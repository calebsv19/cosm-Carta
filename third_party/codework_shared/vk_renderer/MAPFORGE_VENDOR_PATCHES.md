# MapForge vendored renderer patches

2026-10-04: Consume the canonical shared renderer's existing per-swapchain-image present semaphore fix. The command header/source are exact copies of the canonical files; resize recreates those semaphores after the existing device-idle barrier. Avoid importing unrelated canonical renderer changes into this app snapshot. This is an app-local backport to the vendored 1.3.2 baseline; canonical shared sources and the upstream VERSION identity are unchanged. Native validation exercises acquisition, repeated resize and exit. Source hashes are recorded in the S7 proof receipt.

## S8 — 2026-10-04

Bounded feature backport from canonical vk_renderer 1.6.0: public `vk_renderer_draw_texture_corners`, identical `src/vk_renderer_texture_corners.c`, and canonical textured vertex emitter. Vendor VERSION retains its 1.3.2 base identity. No wholesale 1.6 adoption. Terrain projection/cache/coverage stays app-local.
