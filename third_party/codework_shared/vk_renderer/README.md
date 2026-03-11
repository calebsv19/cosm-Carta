# SDL Vulkan Renderer Layer

A modular Vulkan rendering backend designed to slot into existing SDL-based
projects as a drop-in replacement for `SDL_Renderer` APIs. The layer exposes a
minimal C API for initialisation, drawing primitives, texture uploads, and
frame submission while keeping SDL responsible for window creation, input, and
lifecycle management.

## Highlights

- Pure C implementation with small, composable translation units.
- Works with your existing SDL window (`SDL_Vulkan_CreateSurface` under the
  hood).
- Helper headers (`vk_renderer_compat_sdl.h`, `vk_renderer_sdl.h`) to remap
  common `SDL_Render*` calls to Vulkan equivalents when you are ready to switch.
- Separate modules for context/device management, command buffers, pipelines,
  textures, and memory helpers.
- Offline GLSL shader sources plus a helper script for compiling to SPIR-V.
- Simple SDL example (`make example`) that demonstrates the render loop.
- Explicit frame capture API via `vk_renderer_request_capture(...)` (no automatic frame dumps in normal runtime).

## Versioning

- Module version source of truth: `shared/vk_renderer/VERSION`
- Current version: `1.0.0`
- `1.0.0` behavior note: debug capture is opt-in only and runs only when `vk_renderer_request_capture(...)` is called.

## Layout

```
include/
  vk_renderer.h              // Public API surface
  vk_renderer_config.h       // Configuration struct & defaults
  vk_renderer_context.h      // Instance/device/swapchain helpers
  vk_renderer_memory.h       // Buffer & image allocation helpers
  vk_renderer_pipeline.h     // Graphics pipeline creation helpers
  vk_renderer_commands.h     // Command buffer lifecycle helpers
  vk_renderer_textures.h     // Texture upload and tracking
  vk_renderer_compat_sdl.h   // Optional SDL macro remaps
  vk_renderer_sdl.h          // Convenience header for SDL compat mode
src/
  ... matching C implementations ...
shaders/
  *.vert / *.frag            // GLSL shader sources (+ .spv outputs)
scripts/
  compile_shaders.sh         // Convenience wrapper around glslc
examples/
  sdl_vulkan_example.c       // Minimal SDL render loop using the layer
```

## Building the Static Library

Requirements:

- Vulkan SDK (1.2 or newer recommended)
- SDL2 development headers
- `glslc` from the Vulkan SDK for shader compilation

```bash
cd src/engine/Render/vk_renderer_ref
./scripts/compile_shaders.sh   # produces .spv files next to the GLSL sources
make                           # builds libvkrenderer.a
```

To build and run the demo (requires `sdl2-config` on PATH):

```bash
make example
./build/example/sdl_vulkan_example
```

## Location in This Repo

The library lives at `src/engine/Render/vk_renderer_ref`. You can copy that
directory into another SDL project as-is (rename the folder if you want).
When you do, make sure your build includes both `include/` and `src/`, and keep
`shaders/` alongside them so the renderer can find the SPIR-V files.

## Integrating with an SDL Application

The renderer is designed to slip into an existing SDL codebase without
rewriting your main loop. SDL stays in charge of window creation, events, and
high-level timing; the Vulkan layer handles command recording and swapchain
presentation.

1. **Create / store the SDL window**  
   Use `SDL_CreateWindow` with `SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
   SDL_WINDOW_ALLOW_HIGHDPI`. Keep your existing event loop intact.

2. **Initialise the renderer**  
   ```c
   #include "vk_renderer.h"
   #include "vk_renderer_sdl.h"

   VkRenderer renderer;
   VkRendererConfig cfg;
   vk_renderer_config_set_defaults(&cfg);
   cfg.enable_validation = SDL_TRUE;   // toggle off in release builds if desired
   VkResult init = vk_renderer_init(&renderer, window, &cfg);
   SDL_assert(init == VK_SUCCESS);

   // replace SDL_Renderer* storage with VkRenderer*
   renderContext->renderer = &renderer;
   renderContext->window = window;
   ```

3. **Update your frame loop**  
   Each frame, query the SDL window size, optionally fetch the drawable size,
   and pass the window-space dimensions back to the renderer so logical units
   match your UI layout:
   ```c
   int winW, winH, drawableW, drawableH;
   SDL_GetWindowSize(window, &winW, &winH);
   SDL_Vulkan_GetDrawableSize(window, &drawableW, &drawableH);
   vk_renderer_set_logical_size(renderContext->renderer,
                                (float)winW, (float)winH);

   VkCommandBuffer cmd;
   VkFramebuffer fb;
   VkExtent2D extent;
   VkResult frame = vk_renderer_begin_frame(renderContext->renderer,
                                            &cmd, &fb, &extent);
   if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
       vk_renderer_recreate_swapchain(renderContext->renderer, window);
       return; // or continue; inside your loop
   }
   SDL_assert(frame == VK_SUCCESS);
   ```

   Keep your existing immediate-mode drawing intact. With the compat header
   enabled, functions such as `SDL_SetRenderDrawColor`, `SDL_RenderDrawLine`,
   and `SDL_RenderFillRect` now record Vulkan draw calls.

   Finish the frame with:
   ```c
   VkResult end = vk_renderer_end_frame(renderContext->renderer, cmd);
   if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
       vk_renderer_recreate_swapchain(renderContext->renderer, window);
   } else {
       SDL_assert(end == VK_SUCCESS);
   }
   ```
   Optionally compare `drawableW/drawableH` against
   `renderer.context.swapchain.extent` to detect high-DPI mismatches and
   trigger a recreate.

4. **Texture uploads**  
   Call `vk_renderer_upload_sdl_surface` to promote SDL-generated surfaces
   (e.g. SDL_ttf glyphs) into GPU textures, then draw them with
   `vk_renderer_draw_texture`.

5. **Window resize handling**  
   The renderer now bubbles up both `VK_ERROR_OUT_OF_DATE_KHR` and
   `VK_SUBOPTIMAL_KHR`. Treat either code as a signal that SDL resized the
   window or the swapchain no longer matches the drawable size, and immediately
   call `vk_renderer_recreate_swapchain`.

6. **Shutdown**  
   Invoke `vk_renderer_shutdown` before destroying the SDL window or quitting
   SDL.

## Texture Workflow

```c
SDL_Surface* surface = /* generated by SDL_ttf */;
VkRendererTexture gpu_texture;
if (vk_renderer_upload_sdl_surface(&renderer, surface, &gpu_texture) == VK_SUCCESS) {
    SDL_Rect dst = {100, 50, surface->w, surface->h};
    vk_renderer_draw_texture(&renderer, &gpu_texture, NULL, &dst);
}
// Later
vk_renderer_texture_destroy(&renderer, &gpu_texture);
```

## Configuration Hooks

`VkRendererConfig` contains tunables that you can persist per platform:

- `enable_validation` – toggles `VK_LAYER_KHRONOS_validation`.
- `prefer_discrete_gpu` – choose discrete GPUs first when multiple adapters
  exist.
- `msaa_samples` – pipeline MSAA sample count (defaults to `VK_SAMPLE_COUNT_1_BIT`).
- `frames_in_flight` – number of command buffers/fences to rotate through.
- `clear_color` – RGBA float array used at the start of every frame.
- `extra_*_extensions` – append additional Vulkan instance/device extensions
  if you need platform-specific functionality.

## Bridging With Existing SDL Render Calls

Once you are confident the Vulkan layer is initialising correctly, swap the
renderer pointer stored in your `RenderContext` to `VkRenderer*`. Then:

```c
#include "vk_renderer_sdl.h"
```

Your existing code that calls `SDL_RenderDrawLine`, `SDL_RenderFillRect`, etc.
will automatically forward to the Vulkan-backed implementations.

## Shader Path Note

If the renderer cannot find SPIR-V shaders at runtime, define
`VK_RENDERER_SHADER_ROOT` to the folder that contains `shaders/`. When you build
the library in this repo the fallback path resolves correctly, but external
projects may need the explicit define.

## Next Steps / TODO Hooks

- Depth buffers and 3D support (current render pass is colour-only).
- Pipeline variants (wide lines, dashed lines, MSAA toggles).
- Descriptor allocation strategy for long-lived textures (simple pool for now).
- GPU memory allocator integration (VMA) if you decide to move beyond the
  simple helpers provided here.

Feel free to extend the layer with your own draw helpers (`vk_renderer_draw_circle`,
`vk_renderer_fill_polygon`, etc.) using the provided vertex batching approach.
