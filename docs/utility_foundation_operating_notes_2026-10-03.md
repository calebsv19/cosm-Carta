# Carta utility foundation: operating notes and evidence

Completed 2026-10-03 local / 2026-10-04 UTC in the persistent `codex/map-forge-main-edit` lane. Source remains uncommitted at base `bb693e9`, VERSION `0.3.0`. Canonical source and the installed Desktop app were preserved. The generated development bundle is `build/targets/macOS-arm64/dist/dev/main-edit/Carta Main Edit.app` in that lane.

## What is ready

The Map, Dog Walking, Hiking & History and Data buttons select a workspace within the existing app. Dog Walking filters places to dog-walking or pet-sitting membership; Hiking & History filters hiking or visited places. Map shows all places, and Data exposes the existing importer. These are foundation views: scheduling, visit ordering, GPX tracks, hike completion records and DEM terrain are subsequent work.

Navigation preserves a pending draft, selection and map cursor; clicking the navigation row cannot create a map endpoint. The selected workspace persists in runtime `app_state.json`. The pin metadata panel toggles five independent memberships. A place can belong to several collections. New places inherit the active workspace membership. Reordering is available in the complete Map list; filtered views do not rewrite the underlying order.

## Routing contract

Rendering and hit testing keep Web Mercator coordinates. Edge lengths, endpoint distances and A* bounds use spherical ground meters, with Earth radius 6,378,137 m. This is a consistent spherical approximation, rather than an ellipsoidal survey measurement. At Seattle latitude it removes the large Mercator scale inflation in the old distance/ETA values.

One app-local cost policy in `src/route/cost.c` controls graph search, endpoint selection and path totals. Walking uses 1.4 m/s (5.04 km/h); car mode uses static graph speed. Travel times do not include traffic, stops, client appointments or personal pace. Paths retain the exact directed edge and mode for each leg, so parallel edges and mixed walking/car legs are measured using the solver's choices. Directed destination-side transfer search uses incoming adjacency. Partial endpoint legs choose the cheapest permitted parallel edge; foot-only car endpoint legs use walking time, and a forbidden segment cannot fall back to an unmeasured node jump. A start and destination snapped to the same node is a valid zero-edge route; endpoint partial distances are still included.

| UI objective | Optimization cost |
| --- | --- |
| Shortest Distance | Ground length |
| Lowest Time | Length / mode-specific speed |
| Low Climb Proxy | Length + 8 × positive tagged grade × length + 0.5 × surface penalty × length |
| Prefer Fast Roads | Car time weighted 0.6 above the 30 mph threshold, 1.8 otherwise; walking uses walking time |

Low Climb Proxy uses sparse OSM incline tags and surface hints. Missing incline is unavailable information, and this is not DEM-backed ascent. Prefer Fast Roads is a weighted preference. Both labels now describe their actual behavior.

Graph format is native-array `MFG1` with a uint32 version, node count and edge count. All versions contain node x/y doubles, CSR starts, destinations, lengths, speeds and road-class bytes. Version 2 adds speed-limit, grade and surface-penalty floats. Version 3 declares spherical ground lengths. Version 4 additionally stores access bytes (bit 1 car, bit 2 foot). New builds emit v4. V1/v2 lengths are normalized in memory on load, leaving old graph files untouched; v3/v4 lengths are validated against node geography. Payload sizes, CSR bounds and numeric/access values are checked before use and before graph publication.

V4 resolves `motorcar` > `motor_vehicle` > `vehicle` > `access` for cars, and `foot` > `access` for walking. Explicit yes/permissive/designated values can override a general restriction. No/private/use_sidepath values block that mode. Construction/proposed ways are excluded. Motorway/trunk roads default to no walking, while footway/path/steps/pedestrian default to no car access. Car one-way and roundabout rules are separate from walking; walking remains bidirectional unless `oneway:foot` says otherwise.

This is a practical offline graph, not a navigation safety certification. Turn restrictions, barriers, conditional restrictions and dog suitability are not modeled. Legacy graphs retain their older coarse access and direction information. Rebuild a source as v4 to obtain the new access behavior. Interactive partial endpoints are evaluated through their adjoining graph nodes; two points inside the same segment can still detour through those nodes rather than take a direct virtual-node shortcut. Out-of-pack pins and pins over 200 ground meters from the routable network are refused; being inside the rectangle alone does not prove a connected route exists.

## Durable places and recovery

Personal records now live independently of replaceable packs:

- With `MAPFORGE_RUNTIME_DIR`: `<runtime>/pins/places.local.json`.
- Development fallback: `data/pins/private/places.local.json`.

The workspace loader migrates legacy `*.pins.local.json` files from the runtime pin directory and development private directory. Original files remain. Stable IDs, full source JSON and unknown extensions are retained. Identical duplicate IDs may merge; conflicting IDs refuse migration instead of choosing one record. No actual private place files existed in either inspected development tree during this run; migration/recovery was proved using disposable fixtures.

Schema v1 still loads; saves use v2. Built-in membership IDs are `general`, `dog_walking`, `pet_sitting`, `hiking`, `visited_places`. Unknown collection IDs and root/pin JSON extensions survive a save. `map_region` is retained as compatibility metadata, rather than ownership of a place by a downloaded pack.

Coordinates must be finite and supported by the Mercator renderer (latitude ±85.05112878, longitude ±180). Required strings, fixed-capacity text, embedded NUL, strict JSON/UTF-8 and duplicate IDs are validated. Oversized notes are refused rather than silently truncated.

Saving uses the existing atomic `core_io` writer. A validated current primary is backed up to `<path>.bak` before replacement. A corrupt primary can load the valid backup; repair occurs only on an explicit Save. The corrupt file never replaces the good backup. A newer primary schema blocks fallback to an older backup. An exact-byte source signature refuses a save after another writer changes the file. This is a stale-editor guard; it does not implement interprocess locking across the final read/rename window.

If loading refuses a file, retain both primary and backup before investigating. Do not delete records to dismiss the error. If another writer changed the primary, reload before editing; there is no automatic merge. Unsaved UI changes remain unsaved when writing fails.

## North Seattle service-area pack

Canonical data path: `/Users/calebsv/Desktop/CodeWork/map_forge/data/regions/north_seattle_service_area`. The Main Edit launch uses that existing external data root. Region selector identity: `north_seattle_service_area`.

Bounds are west -122.430, south 47.605, east -122.220, north 47.828. This covers Ballard/UW, a little south of the Cut, Shoreline and central Lynnwood. It does not cover the whole Eastside or all of Lynnwood. The northern bound fits the available parent extract; requesting 47.86 would claim unsupported coverage.

Parent source: [BBBike Seattle PBF](https://download.bbbike.org/osm/bbbike/Seattle/Seattle.osm.pbf), HTTP last-modified 2026-09-26, OSM replication timestamp 2026-09-25T23:00:00Z. [OpenStreetMap contributors and ODbL](https://www.openstreetmap.org/copyright) remain the data attribution/license. Retained bounded PBF and SHA-256 provenance are in `data/sources/north_seattle_service_area/` in canonical.

The crop uses `osmium extract --strategy=simple --set-bounds` and the explicit rectangle. A complete-way crop was rejected because a long ferry way pulled geometry far outside the service area. The selected crop contains bounded nodes; missing references split rendering chains and routing segment pairs rather than connecting across gaps. Edge-of-crop polygons may remain partial, so the boundary is not a complete polygon reconstruction guarantee.

Zoom range 10–15, 880 covered tile coordinates and 5,280 indexed rows. Layers: arterial/local roads, water, parks, landuse and buildings. Coarse/mid/fine road/building bands remain available. Contours are disabled because no DEM/terrain proof was built. `tiles.mbtiles` is an indexed SQLite store containing MapForge MFT payloads, not an interoperable raster MBTiles map.

| Retained item / observed resource | Exact amount | Approximate amount |
| --- | ---: | ---: |
| Tile archive | 101,765,120 bytes | 97.05 MiB |
| V4 routing graph | 37,124,226 bytes | 35.40 MiB |
| Runtime pack, including two metadata files | 138,903,169 bytes | 132.47 MiB |
| Bounded source PBF | 25,371,155 bytes | 24.20 MiB |
| Temporary expanded XML | 670,925,503 bytes | 639.84 MiB |
| Region builder maximum RSS | 389,775,360 bytes | 371.72 MiB |
| Graph builder maximum RSS | 348,340,224 bytes | 332.20 MiB |
| Walking proof maximum RSS | 124,813,312 bytes | 119.03 MiB |

The runtime pack has four files and no loose `tiles/` tree. Region generation took 6.73 s; graph generation 4.97 s in the observed runs. RSS and timing are observations for this machine. Peak filesystem use was not sampled. Source + expanded XML + final pack sum to about 796.5 MiB, before SQLite journals, retained snapshots and unrelated activity. Leave at least 1 GiB free working space, plus space for retained prior snapshots. The XML conversion remains the largest transient storage cost and should be replaced by streaming PBF ingest in later work.

`--archive-only` serializes each MFT payload directly into staged SQLite. It never writes the duplicate loose tree. `--max-archive-mib` caps payload insertion and final SQLite size; it does not cap graph, source, XML or retained snapshots. Budget/write failure aborts the transaction and refuses publication. Normal publication retains the prior snapshot using existing retention controls. The UI importer defaults to archive-only, zooms 10–15 and a 128 MiB archive cap, and avoids `--replace`.

## Reproduce a bounded build

From the Main Edit directory, use the established external target helper for this worktree layout:

```sh
make TARGET_CONTRACT_HELPER=/Users/calebsv/Desktop/CodeWork/bin/desktop_release_target_contract.sh
```

After downloading a parent PBF to a disposable path, crop it before building. Do not use the parent file as the retained service-area source:

```sh
osmium extract --strategy=simple --set-bounds \
  --bbox=-122.43,47.605,-122.22,47.828 \
  /absolute/path/Seattle.osm.pbf -o /absolute/path/source.osm.pbf
```

Inspect source bounds and free disk space before importing. Retain the cropped PBF, its source timestamp and hash. The two build tools accept the same bounded source:

```sh
build/targets/macOS-arm64/tools/mapforge_region \
  --region north_seattle_service_area \
  --osm /absolute/path/source.osm.pbf \
  --out /absolute/path/regions/north_seattle_service_area \
  --min-z 10 --max-z 15 --pad-bounds --archive-only --max-archive-mib 128
build/targets/macOS-arm64/tools/mapforge_graph \
  --region north_seattle_service_area \
  --osm /absolute/path/source.osm.pbf \
  --out /absolute/path/regions/north_seattle_service_area
```

A first build needs no replacement flags. Refreshes retain a previous region/graph snapshot under existing builder behavior; review retention rather than deleting old packs. Graph and tile publication are separate operations, so complete and validate both before selecting a refreshed pack. Strict readback:

```sh
MAPFORGE_REGIONS_DIR=/absolute/path/regions \
  build/targets/macOS-arm64/tools/mapforge_region_validate \
  --region north_seattle_service_area --strict --strict-contract
```

Launch the local bundle with an explicit region root until a separately authorized install/adoption step copies packs into the packaged runtime namespace:

```sh
MAPFORGE_REGIONS_DIR=/Users/calebsv/Desktop/CodeWork/map_forge/data/regions \
MAPFORGE_START_REGION=north_seattle_service_area \
  'build/targets/macOS-arm64/dist/dev/main-edit/Carta Main Edit.app/Contents/MacOS/mapforge-launcher'
```

The launcher keeps the Main Edit runtime namespace. Do not point experimental fixture runs at a personal runtime; this audit used `/private/tmp/carta-native-runtime` and synthetic public places.

## Evidence and acceptance

Full `make test`, `make`, Main Edit package contracts, headless smoke and visual artifact generation passed. The final affected tests repeat routing, pins, workspaces, archive emission and headless pin/job behavior after the last edits. The local Main Edit package passes embedded source/binary identity, launcher namespace, code-signature and native self-tests. No release/version, Registry promotion or Desktop refresh was performed.

The route-cost test compares 640 generated mode/objective cases against an independent Dijkstra oracle, exercises parallel edges, mixed legs, legacy normalization and malformed graph refusal. Pin fixtures exercise non-destructive migration, memberships/extensions, canceled writes, backup recovery, invalid input, stale-editor refusal and newer-schema preservation. Workspace fixtures exercise filtered indexing, draft retention and consumed input. Direct-vs-dual archive fixtures compare every payload and inspect actual v4 access/direction bytes. A dense fixture exceeds a 1 MiB budget; every active pack file must remain byte-identical after refusal.

Offline proof on the actual new pack used synthetic public locations:

| Route | Result |
| --- | --- |
| Ballard → UW, walking | 6.72 km, 80.0 min, 380 nodes |
| Ballard → UW, car | 6.92 km, 11.1 min, 388 nodes |
| Shoreline → central Lynnwood, walking | 8.13 km, 96.8 min, 407 nodes |
| Ballard → outside coverage | Refused with `pin_outside_routable_coverage` |

Native Vulkan startup loaded the archive-only pack without a loose tree. Navigation/filtering and draft preservation were inspected; workspace choice persisted across startup. Active-button contrast was corrected for the light theme and rechecked. Native overview throughput was roughly 8–10 FPS in the observed UI; this slice does not prove renderer continuity or resolve zoom flashing. Logs, source provenance and synthetic route outputs are retained under Main Edit `build/utility-foundation-proof/`.

## Bounded follow-ups

Proceed with stage 4 route plans/stop ordering and dog-walk scheduling on top of stable place IDs and corrected costs. Stage 5 should measure zoom flashing and preserve displayed geometry through tile/zoom transitions; the current coarse archive also repeats substantial geometry across zooms. Stages 6–7 can add GPX imports, visit/hike records and DEM-backed terrain without requiring a single huge regional download. Personal records should remain independent of replaceable data packs.
