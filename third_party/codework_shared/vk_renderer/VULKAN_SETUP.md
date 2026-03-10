# SDL + Vulkan Integration Notes

This document captures the wiring between the SDL host application and the
modular Vulkan renderer. Use it as a reference when dropping the layer into
new projects or revisiting the setup after a long pause.

## Responsibilities

| Component     | Owns / Provides                                                    |
|---------------|--------------------------------------------------------------------|
| SDL           | Window creation, event loop, high-DPI handling, timing.            |
| VkRenderer    | Vulkan instance/device/swapchain, command buffers, draw helpers.   |
| Compat header | Macro bridge that remaps `SDL_Render*` calls to Vulkan functions.  |

SDL continues to own the window and emits resize events; the Vulkan layer simply
records draw calls against the surface tied to that window.

## Bootstrapping Checklist

1. **Create the window**
   ```c
   SDL_Window* window = SDL_CreateWindow("App",
                                         SDL_WINDOWPOS_CENTERED,
                                         SDL_WINDOWPOS_CENTERED,
                                         width, height,
                                         SDL_WINDOW_VULKAN |
                                         SDL_WINDOW_RESIZABLE |
                                         SDL_WINDOW_ALLOW_HIGHDPI);
   ```

2. **Initialise Vulkan renderer**
   ```c
   VkRenderer* renderer = calloc(1, sizeof(*renderer));
   VkRendererConfig cfg;
   vk_renderer_config_set_defaults(&cfg);
   cfg.enable_validation = SDL_TRUE;    // disable for release if desired

   VkResult init = vk_renderer_init(renderer, window, &cfg);
   SDL_assert(init == VK_SUCCESS);
   setRenderContext(renderer, window, initialWidth, initialHeight);
   ```

3. **Enable compatibility macros (optional but recommended for migration)**
   ```c
   #include "vk_renderer_sdl.h"
   ```
   This typedefs `SDL_Renderer` to `VkRenderer` so existing drawing code compiles
   unchanged. If you later want to call the Vulkan functions directly, include
   only `vk_renderer.h` and update call-sites.

4. **Per-frame maintenance**
   - Fetch `SDL_GetWindowSize` and `SDL_Vulkan_GetDrawableSize`.
   - Call `vk_renderer_set_logical_size(renderer, winW, winH)` so logical units
     used by your UI match the window regardless of pixel density.
   - Begin the frame with `vk_renderer_begin_frame`. Handle both
     `VK_ERROR_OUT_OF_DATE_KHR` *and* `VK_SUBOPTIMAL_KHR` by recreating the
     swapchain (`vk_renderer_recreate_swapchain(renderer, window)`) and skipping
     the rest of the frame.
   - Record draw calls (compat macros or direct API).
   - End the frame with `vk_renderer_end_frame`, again watching for
     `OUT_OF_DATE`/`SUBOPTIMAL`.
   - Optional: if `drawable` size diverges from the swapchain extent, force a
     recreate to stay in sync with high-DPI monitors.

5. **Textures**
   Promote `SDL_Surface` data (e.g. from SDL_ttf) via
   `vk_renderer_upload_sdl_surface`. The helper internally stages the pixels and
   queues destruction when they are no longer needed.

6. **Shutdown**
   ```c
   vk_renderer_shutdown(renderer);
   free(renderer);
   SDL_DestroyWindow(window);
   SDL_Quit();
   ```

## Handling Resizes

- SDL sends `SDL_WINDOWEVENT_SIZE_CHANGED` / `SDL_WINDOWEVENT_RESIZED` events.
  Use these to update any UI layout state.
- When the window is resized, the next call to `vk_renderer_begin_frame` or
  `vk_renderer_end_frame` will often return `VK_ERROR_OUT_OF_DATE_KHR`. Some
  drivers (notably on macOS) prefer `VK_SUBOPTIMAL_KHR` instead. Treat *both*
  codes the same: wait for any in-flight work to finish (`vkDeviceWaitIdle`
  is called inside `vk_renderer_recreate_swapchain`) and build a fresh
  swapchain.
- After recreating the swapchain, immediately refresh the logical size with the
  new window dimensions.

## Debugging Aids

- Define `VULKAN_RENDER_DEBUG=1` to enable high-level logging.
- Define `VULKAN_RENDER_DEBUG_FRAMES=1` alongside the above to get per-frame
  traces of draw calls, logical sizing, and swapchain rebuilds.
- The renderer keeps track of draw call counts; if you see zero draws reported,
  double-check that your UI code is still issuing the expected `SDL_Render*`
  calls or direct `vk_renderer_*` primitives.
- Mitigate black screens by verifying:
  1. `vk_renderer_begin_frame` returns `VK_SUCCESS`.
  2. Logical size is non-zero (set every frame).
  3. Shaders are compiled to SPIR-V (run `scripts/compile_shaders.sh` after GLSL tweaks).

## Porting to Other SDL Projects

- Wrap existing SDL render contexts inside a lightweight struct that stores the
  `SDL_Window*` and `VkRenderer*`.
- Enable the compat header to keep the rest of the codebase unchanged.
- Replace any direct calls to `SDL_RenderPresent` or `SDL_RenderClear` with the
  begin/end frame pair documented above.
- Propagate resize notifications so layout systems recalculate using
  `SDL_GetWindowSize`; the renderer will take care of the swapchain.

## This Repo's Wiring

- The library lives at `src/engine/Render/vk_renderer_ref` with headers in
  `include/` and implementation in `src/`.
- `engine/Render/renderer_backend.h` pulls in `vk_renderer_sdl.h` and asserts
  the SDL compat macros are active.
- `app/GlobalInfo/system_control.c` initializes the renderer and wires it into
  the global render context.

Following this checklist ensures the Vulkan backend remains a drop-in upgrade
that respects SDL’s window lifecycle while providing explicit control over the
graphics pipeline.
