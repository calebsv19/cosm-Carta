# UI window and fullscreen lifecycle contract

The optional `kit_ui 0.18.0` SDL adapter observes the host window; it does not own
its event loop, domain state, renderer or fullscreen policy. `vk_renderer 1.6.0`
provides corrected frame synchronization and bounded out-of-date recovery.

## Observation and coordinates

Call `kit_ui_window_refresh_sdl` before routing/rendering and after draining native
events. Track logical window size and actual drawable size independently. A
geometry, fullscreen/maximize, display, visibility or focus transition increments
an observation generation. Geometry is queried from SDL, not trusted from an old
queued resize event. Polling observations catches drawable/DPI changes even when
logical dimensions remain unchanged. F11 uses optional desktop-fullscreen toggling;
native macOS fullscreen/window controls remain supported.

Use logical coordinates for the Vulkan command UI in Orchestra/Echo. DataLab uses
its actual SDL render extent and maps native window positions to that extent.
Apply scale once; out-of-bounds captured positions must remain out of bounds.
The SDL native text adapter maps render-space caret rectangles back to window
coordinates. Hosts retain text buffers and eligibility; resize does not save or
cancel a domain edit automatically.

## Invalidation and suspension

Before control consumers, invalidate pointer/key press ownership, pane captures
and active splitter drafts on resize, move/display change, maximize/restore,
hide/minimize, or focus loss. Restore saved domain payload/revision state through
the pane edit contract. Newly painted surfaces rebuild hit geometry. A stale
release must never activate a control or persist a canceled drag.

Hidden, minimized or zero-sized windows defer submissions and swapchain
recreation. Keep polling/ticking the host lifecycle at a bounded idle rate so
restore/show can resume. Cocoa minimize animation may render while the native
window is still presentable; suspension starts when native flags settle.

## Vulkan recovery

The renderer borrows its original SDL window through shutdown; the host retains
that window's lifetime. An acquire returning out-of-date leaves its frame fence
signaled, recreates the swapchain and retries acquisition once. A suboptimal
acquire has an image/semaphore and therefore records/submits/presents normally.
Reset the fence only directly before queue submission. Fatal device/recording
errors propagate; ordinary resize/fullscreen never relies on an unsignaled fence
from an unsubmitted frame. End-frame out-of-date/suboptimal results recreate
presentation resources after the submitted work. Invalid drawable dimensions
return `VK_NOT_READY` before destroying existing presentation resources.

Textures, domain content and UI state survive swapchain recovery. Hosts compare
actual drawable extents, rebuild only presentation-dependent resources and
schedule a subsequent frame. Existing raw command functions still return native
Vulkan results; high-level begin/end provide bounded surface recovery.

## DataLab reference canvas

The existing 4096x4096 compatibility canvas stays bounded. Larger drawables use
an explicitly bounded render extent mapped to the full presentation extent,
with the same extent used for software painting, hit tests and native-image
layout. This avoids rejecting fullscreen on larger displays and avoids growing
the software canvas allocation solely for window size. The actual drawable
metrics remain separately observable; this is presentation scaling, not a claim
that the SDL canvas renders every large-display pixel natively.

## Qualification and migration

The opt-in `CODEWORK_WINDOW_LIFECYCLE_PROOF=<output-directory>` driver qualifies
real app loops through resize, fullscreen enter/exit, hide/show and minimize/
restore, checking continued frame progress and no submission while suspended.
It captures real rendered frames; it is inactive during normal operation.
Use unit fault injection for acquire-out-of-date/suboptimal synchronization and
1x/2x mapping, then native driver/capture checks in the selected programs. Platform
claims require execution on that platform. macOS native proof is not Linux or
Windows acceptance, nor human IME candidate interaction acceptance.

Import a committed shared pin through the managed subtree lane, rebuild all
consumers of the public Vulkan context structure, integrate one host at a time,
and keep production/canonical packages separate from Main Edit comparison apps.

## Fullscreen/window lifecycle acceptance — 2026-10-05

Accepted shared source `854b51ff57c756b4565021fe759c27a459efbfd3` supplies `kit_ui 0.18.0` optional SDL window
observation, coordinate mapping, F11 desktop-fullscreen and bounded native proof;
`vk_renderer 1.6.0` resets fences only before submission, consumes suboptimal
acquired images and performs bounded out-of-date recovery. Hosts own their loops,
window/resource lifetimes, layouts, domain state, edit restoration and persistence.
Logical size, actual drawable size and an explicitly bounded render extent are
separate. Resize/move/display/maximize/restore/hide/minimize/focus loss cancel stale
pane/control ownership before consumers. Non-presentable windows defer submissions.

Orchestra and Echo use logical command UI coordinates. DataLab retains SDL drawing
and Vulkan canvas presentation, including a bounded 4096x4096 canvas; large
presentation extents scale that canvas with matched paint/input geometry. Both
DataLab viewer and startup picker adopt the lifecycle; the plain SDL viewer path
also passes. Native macOS actual loops pass all eight stages (initial, resize,
fullscreen, windowed, hidden, shown, minimized, restored), six captures each and
continued rendering, with zero Vulkan validation warnings/errors. DataLab also
passes a 5000x1440 drawable. Unit fault injection proves acquire-out-of-date and
suboptimal fence behavior; production-linked pane tests prove canceled payload
and revision restoration for all ten invalidating event kinds.

Scope is the proving trio retained Main Edit lanes. Canonical/stable programs and
app VERSION remain unchanged. macOS proof does not qualify Linux/Windows,
exclusive fullscreen, external-monitor migration, device-loss recovery, human IME
workflow or every program. The native proof drives SDL desktop fullscreen; it does
not automate clicking macOS's green window control. Next: apply the established
migration recipe to one selected program, then qualify its actual UI/native paths.
Mixed field/button traversal and OS IME sessions remain bounded follow-on work;
generalized docking/provider insertion/persistence needs its own contract.
