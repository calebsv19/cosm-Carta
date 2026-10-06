# Pre-S7 runtime checkpoint — 2026-10-04

This is the existing dirty Carta Main Edit writer on `codex/map-forge-main-edit`, VERSION 0.3.0. Source implementation and focused proof precede final bundle acceptance. No commit, regular Desktop release replacement, hiking activity import, AllTrails login, scheduling or map download is authorized by this checkpoint. S7 A–C remains paused.

## Installed map opening

Opening another installed region now prepares validation, metadata, bounded visible coarse tiles and polygon triangulation in one existing core_workers job. The current map stays active while a bottom status panel reports progress. Cancel or Escape requests cancellation; worker checks include the expensive triangulation loop. Failure, an unfinished location draft/modal, or memory/display refusal retains the current map. Shutdown joins and disposes the worker before renderer teardown.

The worker owns private CPU caches and never calls Vulkan or mutates AppState. The owner uploads at most two occupied tile entries per tick, checking an elapsed 3 ms slice between entries, and publishes only completed ownership. A single GPU upload is indivisible, so 3 ms is a soft slice, not an absolute frame guarantee. Active cache admission reserves headroom while staging; cancellation/failure restores prior limits. Combined staged/active decoded arrays stay within the existing per-layer limits: 64 MiB per road layer and 32 MiB per other layer. Retained GPU mesh buffers have a combined 512 MiB ceiling. These are payload limits, not a bound on total process RSS, route graphs, fonts or driver memory.

Startup catalog scanning and OS-controlled folder access still precede normal application interaction. Background region preparation does not remove macOS folder permission prompts. Leave permissions to the normal prompt; do not manually toggle System Settings to repair a new ad-hoc build identity.

## Rendering and text

Submission counters now describe remaining eligible work, excluding consumed queues and unavailable prefetch quota. Visible missing tiles receive bounded priority. Slow frames can admit up to four nominal frames of cheap asynchronous requests, with existing decode, integration, prefetch and GPU limits maintained. Mixed-layer ready results use the existing active-layer/fallback count and 1.8 ms integration time slice rather than being throttled by whichever small polygon layer is first. In-flight decode waiting retains a short event-loop wake interval.

Large staged polygon rings are triangulated off-thread and record the attempted cache limit, preventing owner-thread retries of already attempted rings. Existing polygon safety limits remain; unsupported or failed rings are quarantined, and no MFT on-disk format changes.

Native review also found pale hard-coded pins-pane text under the light theme. The pane and metadata facts now use the existing theme palette; active tab labels retain contrasting text. Clipped Vulkan labels now reuse kit_render's existing bounded text texture/font cache. App-local clipping intersects the caller's prior scissor and restores it after drawing, including failed draws; it does not squeeze the string into a shorter rectangle. Existing renderer/font cache shutdown and wait-idle contracts remain in use. No shared library API, source, version or adoption record changed.

## Verification

The required app/tools build and focused region preparation, owner transition, cancellation, shutdown, guarded publication, cache reservation, large-ring cancellation and tile submission regression checks pass. Pin panel and production place-dialog checks pass after the theme/cache changes. The personal canonical document is unchanged: 10 places, 12 pets, 13 services and zero saved routes. One approximately 3.19 km Home-to-Bongo walking route was previewed and saved only in a private disposable review copy; original records and map packs were not modified.

D1 passed with original limits and zero load-clamp events. D2 passed within its existing third whole-comparison attempt, after two fallback comparison failures and two unstable baseline coverage rows. The combined full test/smoke invocation then stopped in D3 at candidate fallback peak 50 versus baseline 40. One isolated diagnostic D3 rerun passed on its first attempt. This is retained comparative instability, not a stable all-green aggregate run; thresholds were not changed. All skipped location/route/utility/GPX-preview unit checks were run separately, core/artifact/continuity headless checks passed, and carta-local-proof passed with initial/resized Vulkan captures and package verification.

A local ignored `data/regions` symlink references canonical existing packs for headless fixtures that require the default worktree path. It duplicates no map bytes. Diagnostics use disposable state and compact generated BMPs losslessly into bounded PNG captures. No test route or synthetic client enters the personal workspace.

Native IDE-font zoom/pan/resize and region-switch validation results, final bundle identity and remaining UI review status are recorded in `build/pre-s7-proof/receipt.json` and appended below. Earlier failed diagnostic attempts remain retained, including missing validation-library launch configuration and a row cap reached after text caching improved throughput. These failures are not substituted for successful validation.

## Stop boundary

S7A activity catalog, S7B real GPX import/recovery, and S7C recorded-track overlays/reopening remain untouched. Existing GPX preview tests exercise the earlier read-only prototype only. No recording is requested or imported by this checkpoint. Next implementation requires the user's explicit resumption and actual exported tracks; human visual acceptance of this build remains separate from agent/source/package proof.

## Final native checkpoint — 2026-10-04

The final IDE-font Vulkan replay completed two passes over 64.00175 s with 29,793 recorded frames. Frame p95 was 3.3927 ms and max 43.9466 ms in this automated replay. The separate installed-region replay completed Seattle (66 prepared tiles, 162.36 ms) and North Seattle (36 prepared tiles, 16,052.08 ms) while rendering continued: frame p95 42.3259 ms, max 73.3171 ms, owner update max 21.6001 ms. Both runs exited zero with zero validation messages, missing retained presentation, invalid lifecycle, invariant or region failures. GPU mesh peaks were 179,037,696 and 145,776,516 bytes; decoded CPU payload peaks 56,111,544 and 39,080,288 bytes. Total RSS peaked around 656 MiB and 568 MiB respectively; payload caps are not total-RSS promises. Native capture proof covers real basemap/layers, zoom and resize; automated frame timing is not a claim about every foreground workload.

The first rebuilt Main Edit native client view showed ten saved markers and the repaired nonblank Fit Visible basemap. Final palette/cache changes have current-source captured map/header proof and focused production event tests. The next rebuilt bundle was held in app_init's map-folder opendir by normal OS access, before the interactive workspace became available. A second raw review owner was available to execute but could not be selected by the native computer-use adapter (not a registered app). No further permission workarounds were attempted. Owned review processes were closed before final packaging. Native populated route rename/archive/restore/open and final backup-dialog walkthrough remain pending; repository/production event behavior passes, but that does not replace pixel review. Human acceptance remains unclaimed.

The final bundle is `build/targets/macOS-arm64/dist/dev/main-edit/Carta Main Edit.app`. Source docs are frozen before its identity/signature/namespace self-test. The normal macOS prompt, if shown when reopening, is the only OS action needed to resume the interactive verification. No more feature planning or address data is needed for that verification, and S7 A–C remains paused.
