# Workspace Authoring Host Adoption Guide

Public guidance for wiring `shared/kit/kit_workspace_authoring` into additional host programs.

## Purpose

This guide captures the stable host-side contract proven by `workspace_sandbox`, piloted in `datalab`, and closed as a completed first-host attach in `drawing_program`, so future programs can:
1. adopt shared authoring behavior quickly,
2. avoid overlay-only integrations that fail to surface theme/font changes in normal app shells,
3. keep host ownership boundaries clear while reusing shared input/runtime/UI glue.

## Required Host Contract

### 1) Authoring-first input routing
Host loops must route key/mouse input through shared authoring helpers before host-local handlers.

Required behavior:
1. `Alt+C+V` entry chord detection via shared helper.
2. While authoring active, consume authoring-first keys (`Tab`, `Enter`, `Esc`, reserved authoring keys).
3. Pointer events should route authoring-first when authoring overlay controls are active.

### 2) Host-owned state (not shared-owned)
`kit_workspace_authoring` does not persist host state. Host must own:
1. `text_zoom_step`,
2. theme preset id,
3. authoring entry baseline snapshots for apply/cancel semantics.

Minimum apply/cancel semantics:
1. `Apply` commits current draft (`text_zoom_step` + theme preset) as new baseline.
2. `Cancel` restores entry baseline and exits authoring mode.

### 3) Overlay + shell parity requirement
A host is not complete if only the authoring overlay reacts.

Required parity:
1. Authoring overlay controls react live (font/theme lane).
2. Non-authoring top-level shell surfaces (startup picker, reopen picker, or equivalent) reflect active theme preset.
3. Relaunch preserves theme preset + text zoom via host runtime prefs.

This parity requirement prevents hidden state where users apply a theme but see no change in normal runtime shells.

### 4) Shared render seam usage
In authoring-active mode, host submit path should switch to shared derive/submit seam:
1. `kit_workspace_authoring_ui_derive_frame(...)`
2. `kit_workspace_authoring_ui_submit_frame(...)`

In non-authoring mode, host render remains host-native.

## Recommended Integration Sequence

1. **Input attach**
   - add entry-chord + trigger/action routing in host loop(s),
   - preserve host fallback behavior when authoring inactive.
2. **Render attach**
   - add authoring takeover submit path through shared derive/submit seam.
3. **Font/theme reactivity**
   - wire interactive controls + baseline apply/cancel semantics.
4. **Shell parity**
   - theme top-level picker/shell chrome from active preset and persist across relaunch.
5. **Docs + closeout**
   - update host current-truth + future-intent docs and private execution lane status.

## DataLab-backed Reference Pattern

Use `datalab` as a concrete integration reference:
1. Host/runtime wiring:
   - `/Users/calebsv/Desktop/CodeWork/datalab/src/render/render_view_authoring_adapter.c`
   - `/Users/calebsv/Desktop/CodeWork/datalab/src/render/render_view_profiles_loops.c`
2. Overlay takeover:
   - `/Users/calebsv/Desktop/CodeWork/datalab/src/render/render_view_authoring_overlay.c`
3. Picker shell parity + theme persistence:
   - `/Users/calebsv/Desktop/CodeWork/datalab/src/render/render_view_picker.c`
   - `/Users/calebsv/Desktop/CodeWork/datalab/src/app/datalab_runtime_prefs.c`
   - `/Users/calebsv/Desktop/CodeWork/datalab/src/app/datalab_app_main.c`
4. Host state fields:
   - `/Users/calebsv/Desktop/CodeWork/datalab/include/app/app_state.h`

## Additional Closed Host Reference

Use `drawing_program` as a second concrete reference for a completed first-host attach that keeps drawing-specific render/input behavior app-local while adopting the shared authoring contract:
1. Host adapter + runtime ownership:
   - `/Users/calebsv/Desktop/CodeWork/drawing_program/src/runtime/adapters/drawing_program_authoring_host.c`
   - `/Users/calebsv/Desktop/CodeWork/drawing_program/src/app/drawing_program_app_visual_runtime_loop.c`
2. Authoring chrome + takeover overlay:
   - `/Users/calebsv/Desktop/CodeWork/drawing_program/src/render/frame/drawing_program_visual_authoring_chrome.c`
3. Accepted-only authoring persistence:
   - `/Users/calebsv/Desktop/CodeWork/drawing_program/src/io/session/drawing_program_snapshot.c`
   - `/Users/calebsv/Desktop/CodeWork/drawing_program/src/io/session/drawing_program_snapshot_ui_settings.c`

## Verification Checklist (per host)

1. Build/compile gate passes.
2. Host tests pass.
3. Packaged self-test/smoke gate passes (for desktop hosts).
4. Authoring acceptance:
   - enter/cycle/apply/cancel behavior matches contract,
   - live font/theme changes visible in overlay.
5. Shell parity acceptance:
   - startup/reopen picker reflects theme preset,
   - relaunch preserves theme + text zoom.

## Next Directions (post-adoption baseline)

Two independent tracks can proceed in parallel once this contract is met:
1. **Host adoption track**: attach additional programs to shared authoring contract.
2. **Capability expansion track**: expand authoring features (new controls/workflows) in shared kit while preserving host contract and parity rules.
