# S9 hiking workspace and independent terrain geometry — 2026-10-04

The Hiking & History workspace now has a compact primary toolbar, a scrollable route browser and a selected-route card. Clicking an entry selects it and frames the whole route with padding, resets rotation and persists that view. Fit route and Fit all are explicit alternatives. Selected route is the default map display; personal peak markers remain context. Search and category/archive filters affect the browser without discarding the current selection. Tools reveals import-adjacent editing, export, archive, nearby peaks and terrain reload; Details reveals point inspection, source coordinates/time/elevation and profile inspection. Existing GPX import preview, edit/cancel, export and archive/restore semantics remain intact.

The Hiking header replaces unrelated road/vehicle controls. Drawing, mouse hit testing and replay diagnostics share history_layout geometry. Dark and light themes use existing core_theme/core_font; this slice introduces no shared API or shared version change. UI grouping and geographic policy remain application-owned.

## Terrain use

Choose Relief for contour-free shaded raster terrain plus independent native vector contours and GNIS labels. Topo retains the original raster USGS cartographic product, including its baked labels; it remains available for comparison. Off hides terrain. Vector geometry stays in projected coordinates while labels are placed in screen coordinates at a fixed readable size with a light halo and collision filtering. Contour intervals change with zoom and density: 20, 100 or 500 metres. Public place/feature names do not imply a personal visit. Personal peak labels receive priority over duplicate public context.

Offline contours use genuine USGS 3DEP elevation exports with source queries, dimensions, geographic extents and SHA-256 receipts. Marching squares interpolates crossings, resolves saddle cells and rejects missing-data cells. Connected equal-height chains are simplified with a tolerance of 0.2 times the decimated sample spacing; endpoints and disconnected sections remain separate. This makes linework crisp without increasing source elevation accuracy. These are overview contours rather than survey measurements. GNIS point features are labels, not trail geometry or proof of completion.

Sources: [USGS 3DEP elevation service](https://elevation.nationalmap.gov/arcgis/rest/services/3DEPElevation/ImageServer) and [USGS Geographic Names Information System](https://www.usgs.gov/tools/geographic-names-information-system-gnis), retrieved through the [official geographic names service](https://carto.nationalmap.gov/arcgis/rest/services/geonames/MapServer).

## Storage and runtime bounds

The prepared sidecar has 2,562 compressed indexed tiles and 988 deduplicated public labels: 7,487,488 bytes (7.14 MiB). The existing overview/detail raster pack is 92,426,240 bytes (88.14 MiB). Combined persistent terrain data is 99,913,728 bytes (95.29 MiB), excluding small JSON provenance. Relief regeneration preserves Topo tiles and geographic extents. Both databases are read-only at runtime. No personal GPX asset is changed.

The native sidecar reader admits at most 64 MiB, strict schema/projection/coordinate bounds and tile CRCs; compressed and decoded tiles each have a 512 KiB cap. Runtime residency is capped at 24 decoded tiles (12 MiB theoretical), at most 20 desired keys, two decodes per frame and 8,000 visible contour segments. Dense views coarsen before drawing rather than dropping an arbitrary part of the view. Connected contours use the existing batched line-strip renderer. Labels are independently capped and collision-filtered. Missing sidecars leave the original map/terrain usable; invalid tiles are rejected and Reload clears failed cache entries.

Preparation requires NumPy/Pillow only in the offline tool. The native application has no Python or network requirement. Replaceable raw preparation DEMs total about 251 MiB and are removed after final artifact verification; public metadata and source query receipts remain in ignored proof output.

## Verification and delivery boundary

Required make, affected UI/viewport/pins/place tools/vector reader/terrain policy tests passed. Tests cover exact planar contour crossings, saddle topology, no-data rejection, chain simplification, CRC rejection, replacement preservation and click-to-fit bounds. Production readers checked all 2,562 vector and 2,943 raster tiles. Native source captures exercised selection, rotation, pan, resize, terrain-off, Details, Tools and edit/cancel under strict Vulkan validation. Batched dark-theme replay measured roughly 16.6 ms median in most phases and 23.1 ms in the dense rotated view; these observations do not guarantee 60 FPS for every route/view.

Final packaged native captures, regression results, immutable binary/source identities, personal-data integrity and cleanup are recorded in build/s9-proof. The local Main Edit remains version 0.3.0; no commit, public release or public promotion is performed. Human design and geographic acceptance remain separate from automated proof.
