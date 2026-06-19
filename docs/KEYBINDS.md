# MapForge Keybinds

This reference groups all active controls by category for quick lookup.

## Navigation
- WASD / Arrow Keys: Pan camera
- Mouse Wheel: Zoom in/out at cursor
- Left-Click + Drag (empty space): Pan camera
- `=` / `+`: Zoom in at viewport center
- `-`: Zoom out at viewport center
- Cmd/Ctrl + `+`: Increase UI text size
- Cmd/Ctrl + `-`: Decrease UI text size
- Cmd/Ctrl + `0`: Reset UI text size
- Mouse Wheel over header layer strip (right side): Horizontal-scroll layer chips

## Routing: Placement & Editing
- Shift + Left Click: Place start (green) node
- Right Click: Place goal (red) node
- Left Click + Drag (start marker): Move start node
- Left Click + Drag (goal marker): Move goal node
- Right Click + Drag (goal marker): Move goal node
- Middle Click: Clear route
- Enter: Recompute route (if start + goal set)

## Routing: Playback
- Space: Play / pause playback
- . : Step forward 5 seconds
- , : Step back 5 seconds
- Shift + `=` / `+`: Speed up (1x -> 2x -> 4x -> 6x -> 8x -> 12x -> 16x)
- Shift + `-`: Speed down

## Routing: Preview
- F8: Toggle route follow-preview
- F9: Toggle heading-up / north-up preview mode
- ; : Rotate heading left and leave follow-preview
- ' : Rotate heading right and leave follow-preview
- 0: Reset heading toward north-up and leave follow-preview

## Routing: Profile & Mode
- F4: Cycle objective (shortest -> lowest time -> lowest elevation gain -> time above speed threshold)
- Header toggle (top-left): Switch car / walk

## Debug & Utility
- F1: Toggle debug overlay
- F2: Toggle single-line road render
- F3: Cycle regions
- F5: Toggle landuse polygons
- F6: Toggle building fill (enabled by default)
- F7: Toggle polygon outline-only (parks/water/landuse)
- Cmd + C: Copy current layer/zoom HUD debug snapshot to clipboard

## Data Root + Ingest Panel
- O: Toggle ingest panel collapse/expand (does not fully hide panel; collapsed state keeps the small clickable handle visible)
- Tab: Switch ingest list tab (`OSM SOURCES` / `ACTIVE REGIONS`)
- Up/Down: Move ingest selection in current tab
- E: Toggle input-root edit mode (path typing)
- B: Open native macOS folder chooser for input root
- Enter:
  - in edit mode: apply current input-root path
  - source tab: import selected OSM source (`.osm`/`.osm.xml`/`.pbf`) and open derived region
  - active tab: open selected imported region
- Double-click (active tab row): open selected imported region
- A: Import all discovered OSM source files from current input root

Notes:
- Keyboard-first ingest is the current contract.
- Mouse double-click activation is currently implemented only for opening an imported region from `ACTIVE REGIONS`.
- When the ingest panel is collapsed, normal route `Enter` recompute behavior is active.
