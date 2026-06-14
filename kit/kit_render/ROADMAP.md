# kit_render Roadmap

Current milestone:
- null backend recording contract is live and test-backed
- Vulkan backend attachment path is live behind the shared `vk_renderer` bridge
- shared text policy resolution and external text helper runtime are both live
- the module boundary is now truth-locked around frame recording, backend attachment, and shared text-runtime ownership
- external text bridge hosts now have an explicit shared font-cache reset seam for SDL_ttf lifecycle boundaries, reducing stale-handle risk during menu/runtime teardown and restart flows

Next implementation steps:
- improve transform-stack ergonomics beyond per-command transforms
- improve rounded-rect fidelity beyond plain rect fallback
- continue glyph/string cache work for dense UI text workloads
- keep Vulkan text/runtime parity tuning additive and validation-backed
- decide whether any repeated external-text cache helpers should be narrowed further without moving app policy into `kit_render`

Deferred boundaries:
- widget logic, hit testing, and layout systems remain outside `kit_render`
- renderer-host lifetime policy remains explicit attach/adopt behavior only
- app-local wrapped text layout policy, cursor policy, and selection behavior remain outside this module
- optional plain-SDL UI bridges such as `kit_ui_sdl` remain outside this module;
  they can reuse shared theme/font vocabulary and `KitRenderColor` structs
  without moving host SDL drawing into `kit_render`
- no scene graph, pane graph, or persistence ownership belongs here
