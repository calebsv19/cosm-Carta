# Phase 0 Skeleton Plan (Working Checklist)

Status key: [ ] pending, [x] completed

1) [x] Confirm minimal project layout and goals
- Create base directories: include/, src/, assets/, config/, tools/, data/regions/
- Establish module mirrors: app, render, core, camera, ui
- Binary name: mapforge

2) [x] Build system skeleton (Makefile)
- Targets: make/app, run, clean
- C99 flags: -Wall -Wextra -Wpedantic
- SDL2 linkage via sdl2-config with fallback
- Output: build/mapforge

3) [x] Core app loop skeleton
- SDL init, window + renderer creation
- Main loop: input, dt, camera update, render, present
- Clean shutdown

4) [x] Renderer abstraction + SDL backend
- Renderer API header
- SDL backend implementation
- Clear/begin/end frame stubs

5) [x] Camera stub
- Store x/y/zoom
- Init + update + input stub functions

6) [x] Debug overlay stub
- Track FPS/dt
- Render stub (no text yet)

7) [x] Config + assets placeholders
- Minimal config/app.config.json
- Keep assets/ empty

8) [x] Entry point
- src/main.c calling app_run

9) [x] Acceptance check
- make builds without warnings (completed)
- make run opens window and exits cleanly (completed)

10) [x] Follow-up hooks for Phase 1
- TODO markers for tiles/loader/draw calls
