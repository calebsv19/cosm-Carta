# Carta text rendering and style transactions

Carta prepares a replacement font bank before changing the active style. A missing
font or partial load failure preserves all working fonts and the semantic preset
and zoom. Both shared presets and text zoom steps -4 through +5 use this owner.
Disabling MAPFORGE_USE_SHARED_FONT retains the fixed legacy 10-point fallback.

The Font/Theme authoring overlay pins its entry font bank. Cancel, toggle-off and
shutdown restore that bank without reopening fonts. Apply writes preset, theme
and text zoom into the existing atomic app_state.json owner before accepting the
draft. A failed replacement keeps the draft open and the previous file intact.
Camera and unrelated JSON properties retain their existing owner and meaning.

Measurement and painting use the same native raster density. Vulkan uses the
committed KitRender 0.16.0 native-pixel API; SDL paints integer device-pixel quads
while preserving caller scale, viewport and clipping. Missing CJK/emoji glyphs
use the shared native-font adapter on macOS. The app owns a bounded fallback run
cache with deferred Vulkan texture retirement; map projection and camera rules
remain with the existing map owner.

The scoped KitRender source pin is tools/text-render-source-pin.json. It is
independent of the older whole-subtree pin: other shared modules were not imported.
Run make vulkan-rollout-contract to verify exact module bytes. Qualification uses
fresh contained outputs, make app, make test-text-settings, native Vulkan and SDL
pixel oracles, the existing regression/headless gates and the Main Edit package
self-test. No clean target is needed. The native oracles require their font and
fresh output-directory arguments; Vulkan additionally needs a native display and
a local absolute-path validation-layer manifest.

The persistent Main Edit lane owns development qualification. Public release,
installed applications and release records remain separate. Native Linux
presentation and human multistage IME acceptance have not been established by
the macOS replay. Compact map HUD overflow observed in replay remains outside
this text ownership slice.

Continuity qualification starts every scenario from the checked-in configuration
in its own synthetic runtime and home. This prevents a profile's saved final
camera from changing the next profile's starting view. Each successful run keeps
its exact trace for D2/D3 consumers; failed runs preserve their isolated evidence.
The gate requires per-frame fallback samples so periodic telemetry cannot miss
short viewport-transition peaks. Missing samples, regions and numerical
continuity breaches still fail. Thresholds and
production map/camera behavior are unchanged.
