# Carta S4 operating notes — 2026-10-04

## Current checkpoint

S4P1–P3 are implemented in the existing Main Edit worktree. A1/B1/C1 deliver common home-to-road queries, named saved routes and a usable service sidebar. These bounded results do not close every checkbox in the broader S4 roadmap. Source is uncommitted, VERSION remains 0.3.0, and canonical/public/Desktop Carta has not been replaced.

Development app: build/targets/macOS-arm64/dist/dev/main-edit/Carta Main Edit.app. Build with `make package-desktop-main-edit-self-test`; use the existing desktop release target helper when invoking make in this worktree. The package carries source/binary identity, a bundled Helpers/mapforge_places CLI, and the MapForge-Main-Edit runtime namespace.

The real Home pin has been saved in the private runtime. Its municipal address-point result and the native/public map footprints were compared; entrance-level detail was intentionally excluded. Home remains a general place, with no invented pets or services. The user explicitly confirmed Home placement; broader workflow acceptance remains separate.

## Normal app workflow

1. Open Map for all places, or Dog Walking for places in either walking or sitting collections. Home is initially general and appears in Map.
2. Ctrl+F opens local search. Search place names, addresses, linked pet names or service keys (`dog_walking`, `pet_sitting`, `drop_in`, `custom`). Enter finishes typing but retains the filter; Ctrl+F then Esc clears it. Workspace filters still apply. Searching never performs online lookup.
3. Select a sidebar place to inspect address, location quality, linked pets and roles. Existing name, privacy and collection controls still work. Pet/service entity creation is currently performed through the command service; comprehensive native editing forms are a remaining slice.
4. Shift-click a saved place row for start, right-click another saved row for goal. Choose Walk/Car and the existing route objective controls. The pin remains at the home; a mode-legal road segment is snapped independently within 200 ground metres. Straight home connectors are approximate and timed at walking pace, not confirmed entrances.
5. Save Route opens route-name entry. Ctrl+A replaces the suggested name; Enter saves, Esc cancels. Open Next Route cycles saved intents and recomputes using current places/map data. Keyboard equivalents: Ctrl+Shift+S and Ctrl+Shift+O.
6. Changed coordinates, graph bytes or query policy are reported as changed when reopening. Original intent and endpoint snapshots are retained. A missing place/pack or unavailable route is refused explicitly; a dedicated offline intent browser is not yet available.

Map labels show at close zoom or for a selected pin. Existing renderer performance/zoom flashing remains S5 work; this checkpoint does not claim a rendering repair.

## Conversational intake boundary

The assistant interprets the user's address, pet and service facts into one structured request. The app service owns validation, stable IDs, duplicate rules, transactions and readback. There is no installed MCP server or autonomous natural-language parser in Carta at this checkpoint. Codex can invoke the existing structured CLI/adapter for the requested workflow.

Use an absolute `MAPFORGE_RUNTIME_DIR` and the correct runtime namespace. Never use personal storage for synthetic fixtures. Write private request files outside the source tree, with restrictive permissions.

Example synthetic input:

```json
{
  "request_id": "unique-user-action-id",
  "name": "Example home",
  "address": "100 Example Street Seattle WA 98105",
  "pet": {"name": "Miso", "species": "dog"},
  "services": ["dog_walking", "pet_sitting"]
}
```

```sh
MAPFORGE_RUNTIME_DIR=/absolute/private/runtime \
python3 tools/place_intake.py --input /absolute/private/request.json \
  --binary build/targets/macOS-arm64/tools/mapforge_places \
  --resolve seattle_address_points --commit
```

Only the address is sent to Seattle's public GIS locator; no pet/care notes are transmitted. The full normalized address, including unit, must agree with the single score-100 matched PointAddress result. Fuzzy/ambiguous/no-match candidates require review and are not silently committed. Provider errors leave records untouched. Supplied coordinates (`location` with lat/lon/source/quality) support manual placement and other areas without online lookup. `approximate` quality requires explicit acceptance. Seattle's municipal result is address-point evidence, not surveyed positioning.

The helper first checks a request receipt. Identical retries return the saved result/readback without another lookup. Reusing that request ID with changed input is refused. Distinct actions at the same normalized address reuse the home; separate units and ambiguous pet identities remain distinct. An existing home's coordinate is preserved on a new service intake; only a guarded place update replaces it.

Online provider reference: https://gisdata.seattle.gov/cosgis/rest/services/locators/AddressPoints/GeocodeServer. Public GIS source attribution and match evidence are stored on the private place record. No paid geocoding account or full offline address database was introduced.

## Structured operations

CLI: `MAPFORGE_RUNTIME_DIR=/absolute/runtime mapforge_places --request request.json`.

| Operation | Purpose |
| --- | --- |
| places.search | Bounded local search returning linked pets/services |
| places.get | Read one stable place ID and its details |
| intake.receipt | Resolve an action ID before retrying |
| intake.preview | Validate input/duplicates and return store/candidate tokens |
| intake.commit | Commit the same resolved candidate using both preview tokens |
| places.update | Guard a place correction with place_id, expected_revision and preview tokens |
| routes.preview | Compute between stable place IDs for a declared region/mode/objective |
| routes.save | Save name/intent with request_id and the returned preview_token |
| routes.get | Recompute a saved route_id and report staleness |
| routes.list | Inspect bounded saved intent/current references without reading a map graph |

Intake preview returns `store_token` and `candidate_token`; commit uses `expected_store_token` and `candidate_token`. Updates support name/address/notes/location and retain prior-place snapshots. Native pin saves also commit from a private clone, increment revisions and retain history; refused saves/deletes leave caller state and storage unchanged. A manual coordinate correction replaces current address-point quality with manual provenance and invalidates active/late route results. Deleting a place with active pet/service relationships is refused until a later archive/detach workflow is implemented. An explicit existing pet requires `pet.id`; duplicate names alone are not global identity. Service kinds currently create simple profiles, not schedules/durations/time windows. Service archival or pet-entity editing needs a later command/UI extension.

Routes use `from_place_id`, `to_place_id`, `region`, `mode` (`walk`/`car`) and `objective` (`distance`/`time`). Save requires a name, unique action ID and preview token. Plans retain IDs, requested coordinates, graph hash, query policy and last metrics rather than huge graph paths. They survive renames. Changed/missing references cannot silently become another household.

While the native app runs, the CLI submits to its private command inbox and the main-thread owner commits/readbacks/refreshes the map. Mutations are refused while the pin editor has actual unsaved changes or an active name-edit/new-pin draft: finish Save or Cancel first. A clean saved selection permits agent additions; successful additions focus and refresh their selected details. Read/search/preview operations remain available. The CLI may own the namespace only while the native owner lock is free. A timeout requires retrying the same action ID to resolve an uncertain commit.

## Storage and compatibility

Authoritative storage: `<runtime>/pins/places.sqlite`, app-owned schema version 1. The canonical JSON document retains unknown extensions, receipts, correction history and route intent. Relational place/pet/link/service tables are transactionally derived indexes with foreign keys, not competing mutable sources. Saves enforce the expected document signature and roll back on failure. Database/IPC files use private permissions. SQLite identity/version mismatches are refused.

Legacy `places.local.json`/region pin files are imported once with IDs, coordinates, collections and extension data retained; originals remain available. Native, CLI and headless readers prefer the same SQLite repository. Headless never migrates or writes the database. JSON legacy backup recovery remains supported; automatic SQLite backup recovery is not claimed. Preserve private backups rather than deleting a refused/corrupt repository.

Caps: 64 KiB per command; 100 search results; 1,000 route plans; 10,000 intake receipts; 2,000 correction snapshots. Capacity exhaustion refuses further writes. A reviewed archive/export/pruning workflow is future work. Nearest-segment search currently scans the graph; agent route queries reload the declared graph. Native workers retain their graph. Caching/index adoption is a later measured performance slice.

Main Edit launcher reads only literal `MAPFORGE_REGIONS_DIR`, `MAPFORGE_START_REGION` and `MAPFORGE_FOCUS_PLACE_ID` from its private `config/user.env`; explicit environment variables win. It does not evaluate shell expressions. Existing North Seattle pack is referenced in place, without another map copy/download. Startup focus opens the configured stored Home at zoom 17. Remove the focus setting when a more general startup view is desired.

## Evidence and next boundary

Full make test passed with the existing canonical packs in an isolated runtime. After subsequent changes, affected intake, query, pin, workspace, input, playback and preview tests passed. Tests cover preserved legacy extensions, rollback/FK integrity, request retries, different units, same-name pets, stale revisions/tokens, provider fixtures, transactional native edits/deletes, one-way same-edge paths, zero-distance routes, physical snap limit and saved-plan coordinate changes.

Native synthetic proof: two places, linked pet and both roles; second place added through running owner; local pet search; a 45.25 m walk saved and reopened after process restart; a named route saved through visible controls and read back from SQLite. Real Home receipt was returned through the live owner, and a separate headless process resolved the same SQLite Home and returned a zero-distance Home-to-Home route. Native/public screenshots and residential evidence remain in private proof paths, outside source.

Next S4 work: dedicated saved-route list/rename/archive/offline-intent browsing, editable pet/service forms, consistent typed query failures/settings/cancellation, and independently optimized alternative endpoint pairs. Then prioritize S5 measured rendering continuity/performance; S6 schedules and S7 GPX/journal reuse these stable place IDs. Do not turn planned routes into claims that a walk/hike was physically completed.

2026-10-04 follow-up: S5A-C have since been implemented; see the dated S5 notes. User priority is now saved locations and S7 hiking history, with S6 scheduling deferred. The bounded read-only GPX preview and saved-route listing are described in [locations/track notes](locations_tracks_operating_notes_2026-10-04.md). Earlier next-step recommendations above remain historical.
