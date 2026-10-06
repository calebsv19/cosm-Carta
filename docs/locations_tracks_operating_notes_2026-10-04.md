# Saved locations and hiking-track preparation — 2026-10-04

## User priority and scope

The user wants saved geographic places and actual hiking history. S6 timed scheduling is deferred. Addresses are not a prerequisite for technical verification; the existing place intake can already accept user-provided locations. This slice adds read-only saved-route discovery and a read-only GPX 1.1 source preview. It does not implement journal storage, native track overlays or automatic account synchronization.

Home placement was explicitly accepted by the user. Broader interactive rendering/workflow acceptance remains separate. The personal database is unchanged by these checks; all service/route fixtures use disposable namespaces. Existing Main Edit S1–S5 source is preserved, VERSION remains 0.3.0, and no commit/release/Desktop replacement is performed.

## Adding real places

Provide a display name, full address including unit when applicable, and optional pet name/species and categories (dog walk, pet sitting, both, or general). No appointment time, date or service duration is needed. The address point is the home marker; the routing query independently selects a permitted nearby road. Different services can share one place identity.

Intake uses the existing guarded preview/commit/readback service described in [S4 operating notes](s4_operating_notes_2026-10-04.md). Automatic online resolution currently covers Seattle municipal address points. Locations outside that provider's coverage can use explicitly sourced/supplied coordinates; do not silently claim equivalent online coverage for Lynnwood or Issaquah. Saved personal records do not require an installed detailed map to remain in the repository, but visible basemap detail/routing require a covering pack.

The native Map workspace shows all places. Dog Walking includes walking and sitting collections; Hiking & History selects hiking/visited collections. Ctrl+F searches local names, addresses, pets and service keys. “Service workspace” means categorizing/searching these places and their linked pets, not appointment scheduling. Native pet/service editing and recovery are now implemented in the pre-hiking review slice; see [current operating notes](pre_hiking_operating_notes_2026-10-04.md) for the delivery and activation boundary.

## Read-only saved-route discovery

The compiled CLI accepts:

```json
{"operation":"routes.list","offset":0,"limit":100}
```

Pass the JSON via `mapforge_places --request /absolute/private/request.json` with an absolute `MAPFORGE_RUNTIME_DIR`. Use the Helpers CLI in a freshly verified Main Edit bundle or the source tool at `build/targets/macOS-arm64/tools/mapforge_places`. While the updated native owner runs, it answers through its command inbox. This read operation does not activate a route, change selection or reject an unsaved draft as though it were a mutation.

Replies contain `routes`, `total`, `offset`, `limit` and nullable `next_offset`. Each row retains `saved_intent`, current endpoint names, missing-reference flags and `coordinates_changed`. Missing names are null; missing IDs/snapshots remain in the saved intent. Limit is an integer 1–100 and offset is an integer 0–1000; invalid types/bounds and malformed intent containers are refused. Route capacity remains 1,000. Reads preserve the authoritative document byte-for-byte.

`recomputed:false` and `graph_status:"not_checked"` deliberately distinguish a listing from a newly solved route. Saved metrics belong to the saved snapshot. This operation never reads the map graph, so an unavailable pack does not prevent inspection. Use `routes.get` to recompute and check graph/policy/coordinate staleness when the pack is available. A native saved-route list/rename/archive/restore UI is now implemented in the pre-hiking review slice; see [current operating notes](pre_hiking_operating_notes_2026-10-04.md). An older running owner still exposes its earlier controls until the updated bundle is activated.

## First GPX source preview

Run the source tool locally:

```sh
python3 tools/gpx_preview.py /absolute/path/activity.gpx --kind recorded
```

It prints JSON and returns nonzero on refusal. It performs no network requests, personal-store writes, source copies, map downloads or geometry simplification. The default origin is `unknown`; `recorded` and `planned` are explicit operator declarations, not inferred completion evidence. A track element alone is not proof of a completed hike.

The receipt includes SHA-256 of the exact source bytes, byte count, creator, tracks/segments/point counts, track bounds, per-segment ground distance, optional elevation/time coverage and warnings. Distance uses Carta's existing spherical radius 6,378,137 m and never bridges segments or snaps points to a road. Source elevation datum stays unspecified. Durations are available only when every point in that segment has a usable explicit-zone timestamp and time does not reverse; the aggregate is a sum of segment durations, not wall-clock elapsed/moving time. Missing or invalid optional fields stay unavailable. Duplicate status is `not_checked` until an activity catalog exists.

Routes/waypoints are separately counted and are not substituted for recorded-track geometry. Extensions are not normalized by this preview; their bytes stay in the untouched source file. Steps over 1,000 m and antimeridian extents produce review warnings without modifying coordinates. Invalid required coordinates cause a refusal rather than dropping points. The tool accepts namespace-qualified UTF-8 GPX 1.1 only; GPX 1.0/other encodings need a separately verified adapter.

Limits: 16 MiB source bytes; 200,000 GPX points including route/waypoints; 1,024 track segments; 128 tracks; 1,000,000 XML nodes; depth 32; 4,096 characters per text/attribute value. A namespace-aware standard-library XML pull parser processes bounded chunks; DTD/entity declarations and NUL input are refused before parsing. No native XML runtime dependency was added. This Python prototype is source tooling, not a bundled native GPX import feature.

## AllTrails path and next implementation

AllTrails' official help, checked 2026-10-04, documents activity export on the website: profile → Activities → select the actual recorded activity → three-dot menu → Download route → GPX Track. Exporting a trail page/custom route is a different source and can represent intended geometry rather than the user's recording. [Official export instructions](https://support.alltrails.com/hc/en-us/articles/37230403315476-Downloading-files-from-AllTrails).

S7 A–C is paused at the user's request. After a real activity file is supplied and continuation is authorized, inspect its segments/point count and optional fields using this tool, then implement:

1. Preserve the original under its content hash; normalize full-resolution points and segment boundaries into bounded activity assets. Detect exact duplicates without overwriting a prior activity.
2. Add an explicit preview/import action and transactional journal metadata in the existing app-owned SQLite repository, with backup/readback and asset failure recovery. No core_memdb migration is required.
3. Render a distinct recorded-track overlay, fit to extent, inspect original points/times/elevation, and reopen after restart. Keep disjoint segments separate and allow history browsing without detailed coverage.
4. Add notes/dated visits and manually confirmed peak completion. Terrain and a small Issaquah pack are optional later additions when the actual activity needs them.

Addresses can be added alongside this work. A small batch of 2–5 locations exercises real labels/categories/coverage. A real exported GPX is needed before claiming compatibility with that user's AllTrails history. No personal-profile API or automatic sync was implemented or assumed. Cancellable background region preparation remains a separate S5 follow-up, not a prerequisite for storing locations or inspecting a track.

## Verification

Completed 2026-10-04: C application/tools compile; expanded place-intake tests pass across fresh command processes, including offline list pagination, coordinate changes, missing references, invalid inputs, saved-route reopen/staleness, linked pet/service search, receipts/rollback and owner locks. Utility workspace, common route query and headless runtime/workspace pin checks pass. Six GPX test cases pass with independent XML structure readback and equatorial/antimeridian distance oracles. Exact source bytes/hash, namespaced geometry, distinct segments, optional fields, declared origin and bounded refusals are checked. The personal Home database hash matches the S5 checkpoint.

Development package and live-owner verification are recorded separately in `build/locations-tracks-proof/`; source proof is not an installed Desktop release or human acceptance. See the [dated checklist](/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/map_forge/archive/2026-10-06_pre_ui_feature_checkpoint/locations_tracks_progress_2026-10-04.md).
