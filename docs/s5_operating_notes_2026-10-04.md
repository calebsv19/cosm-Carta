# S5 native continuity and performance

2026-10-04. Implementation owner: `codex/map-forge-main-edit`, based on `bb693e963c069c308fad18f095fb070d207d29ef`. This checkpoint extends the existing dirty S1-S4 source. VERSION remains 0.3.0. Personal place records, region packs and the installed Desktop application are separate from disposable diagnostics and the development bundle.

## Attributed failures

The initial native window displayed approximately 7.2 FPS. The first synchronized public-area replay stopped at its first resize, after 27.97 seconds. A process sample identified `vkWaitForFences` in the second begin-frame attempt: bundled vk_renderer 1.3.2 resets its frame fence before acquiring the swapchain image, then an out-of-date/suboptimal result can abandon that frame without a submission. Surface recovery alone leaves an unsignaled fence. The app now rebuilds command synchronization through the published renderer command lifecycle API after recovery, preserving completed frame-owned allocations. Recovery failure disables the unusable backend instead of waiting on an abandoned fence. An upstream renderer fix should eventually replace this app compatibility workaround; no shared module or version was changed here.

Four other mechanisms contributed to discontinuity or excessive work:

- A timer could expire the old presentation while the decoded replacement was still waiting for retained GPU assets. Selection now tests actual resources, preserves a drawable held tile, and searches ready alternate bands/parents before settling for an unuploaded CPU candidate.
- The scheduler treated coarse fallback coverage as completion and stopped draining detailed requests. Coverage now permits continued refinement.
- Successful road/water meshes did not necessarily set the generic `mesh_ready` flag. They could therefore be uploaded again every frame. Typed retained readiness and intentionally empty geometry now suppress redundant preparation.
- Several children could draw the same parent polygon repeatedly, and a global coverage gate could remove a ready current tile's fill while a neighbor loaded. Parent draws are deduplicated per frame; exact current-band tiles preserve their own fill. Native band replacement uses one ready presentation at full configured opacity. SDL's single-sided blend uses full opacity when only one side exists.

The integration-exhaustion counter also counted worker-waiting frames with no ready results. It now counts a reached integration count/time cap only when ready results remain; processing limits and regression thresholds were retained. A full rebuild also exposed a missing JSON link dependency in the existing S4 pin-layout test, now linked explicitly.

Lifecycle instrumentation also exposed a hash-table insertion bug: a collision could overwrite an occupied entry even though an empty slot was available. That insertion is fixed. Queue acceptance is no longer mislabeled as a completed GPU upload. Obsolete nonresident decode preparation is invalidated before rerequest, and stale records can record a fresh decode.

## Residency, lifetime and budgets

Current presentation references are protected in both the decoded cache and retained GPU cache, including parent source tiles spanning tile-level changes. Asset destruction waits for submitted GPU work to finish. Vulkan validation identified cached text descriptors freed during shutdown while still referenced; the local text adapter now waits at that retirement boundary too. Inactive LRU entries can be evicted; pressure cannot evict a pinned last presentation just to admit another tile. Both asynchronous insertion and synchronous loading use decoded byte admission.

Default decoded payload admission is 64 MiB per road layer and 32 MiB per other layer, including entry arrays, triangulation and water variants. Retained GPU buffer payload admission is 512 MiB. Upload admission reserves conservative geometry bytes before creating assets. These are payload limits, not a limit on total process RSS or driver memory: decode/preparation scratch, font textures, swapchain allocations and allocation overhead are separate. Existing frame integration/upload count and time slices remain in use; a single indivisible upload can exceed its slice.

Region transitions stage a bounded coarse view (at most 64 source coordinates) and retained assets before changing the active region identity. Current plus staged payload uses the remaining cache budget. Failure discards only the stage and retains the current map with an explicit status. The successful transition clears old-generation requests and presentation references before publishing the staged caches and camera. It never presents old pack geometry under the new pack identity. Staging is synchronous and can pause the UI for a large region; background region preparation is a later responsiveness improvement. The native Seattle-to-North-Seattle test exercises both successful commits. Empty layers are valid; triangulation failures retain available outlines.

The importer currently records building `point_step=1` for every band. Adding more point skipping would change shape without a stable source-coordinate error bound. This checkpoint therefore retains existing pack bytes. A future versioned profile should simplify complete source rings in Mercator coordinates before tile clipping, preserve shared borders/holes and topology, then compare silhouettes, routes, storage and transition captures on a small extract before rebuilding a personal pack. The measured duplicate upload and scheduling defects are addressed first.

## Reproduction

Run from Main Edit after compiling:

```sh
TARGET_CONTRACT_HELPER=/Users/calebsv/Desktop/CodeWork/bin/desktop_release_target_contract.sh make
python3 tools/run_s5_diagnostics.py \
  --binary build/targets/macOS-arm64/toolchains/clang/bin/mapforge \
  --regions /Users/calebsv/Desktop/CodeWork/map_forge/data/regions \
  --output build/s5-proof/new-native-run --passes 4
```

Use a new output directory for each run. Add `--backend sdl` for a dummy/software comparison or `--region-only` for Seattle/North Seattle transitions. The runner records source/binary and archive hashes, shared versions, process exit, backend confirmation, frame-stage percentiles, payload/RSS, lifecycle/presentation warnings and captures. It rejects incomplete navigation runs, lifecycle/invariant failures, region failures and an unintended SDL fallback. It uses a temporary runtime and isolated theme-persistence path with an explicit soft_light preset, and does not read the personal Home pin or save its replay camera in personal settings.

Each navigation pass takes 32 seconds: overview, slow zoom 13-17, close-up hold, fast reversal, boundary pan, building toggle and a resize. Four passes exercise a cold first pass and three warm passes at the same public UW-area camera. Native captures are limited to 16 and a 256 MiB raw byte estimate; the report converts them losslessly to PNG with pixel-data roundtrip/readback checks and removes only those generated BMP duplicates. Frame CSV output is capped at 20,000 rows. Polygon traces use a configurable stride, 32 in the runner. Captures and synchronous region preparation contribute real long frames and are included in percentiles.

## Acceptance boundary

Native and SDL measurements, test receipts and selected before/after frames live under ignored `build/s5-proof/`. The native full corpus is the evidence for Vulkan behavior; headless exports and SDL tests are supplemental. A CPU-only coarse baseline can have a lower frame time while failing to refine, so it is not a fair same-detail FPS comparison. The original 7.2 FPS observation and the corrected detailed-map timings must not be reported as a controlled speedup ratio.

Initial acquisition of a never-loaded tile may have no presentation; that is loading, not proof that previously drawable coverage was lost. Intentionally empty layers and quarantined fills are distinguished from missing retained geometry in the traces. The fixed corpus and readiness/pressure tests do not establish that every possible map, opacity setting or device is free of flashing. Your own visual acceptance of the final Main Edit navigation remains a separate checkpoint.

## Recorded measurements

The four-pass non-validation native run (`accepted-native-v2`) completed 128.02 s / 7,679 frames with zero sampled polygon no-draw decisions, zero presenter invariant failures and zero lifecycle invalid transitions. Warm navigation across every phase had p50 **16.6207 ms**, p95 **17.1205 ms**, maximum **54.183 ms** (capture frames included). Fixed close-up windows remained around 16.6 ms p50 / 17.0-17.3 ms p95, with zero missing essential coverage after acquisition. This supports approximately 60 FPS in this North Seattle corpus, not a guarantee for all packs/devices/settings.

| Measurement | Four-pass native peak | Scope |
|---|---:|---|
| Decoded tile payload | 50.73 MiB | Entry arrays and owned decoded/prepared geometry |
| Retained GPU buffers | 147.56 MiB | Buffer payload, excluding textures/driver allocations |
| Peak process RSS | 507.11 MiB | OS high-water mark; includes allocations beyond tile caches |

End-of-pass GPU payload: 139.01, 142.77, 147.27, 147.56 MiB. The final warm pass added about 0.29 MiB, while decoded end-of-pass payload stayed near 23 MiB. Resource growth tapered in this bounded corpus; a long-duration memory plateau is not claimed.

The final two-pass validation replay (`native-validation-v2`) completed 64.02 s / 3,821 frames with **zero Vulkan validation messages**, lifecycle/invariant failures, sampled no-draw decisions or retained-presentation misses. It recorded a maximum L0 pending-queue age of 135.0 ms (time awaiting dispatch, not full decode-to-display latency), maximum main upload queue of 7, zero CPU/GPU budget refusals and 155 decoded LRU evictions. Seven polygon fill failures remained quarantined with available outlines; they did not repeatedly trigger rebuilding. Validation increases process overhead: its RSS peak was 596.23 MiB.

SDL software comparison (`accepted-sdl`) completed 64.02 s / 3,134 frames with zero lifecycle/invariant/no-draw failures. Its coarse overview remains much slower than Vulkan (up to roughly 534 ms phase p95); this establishes policy parity and explicitly does not establish a usable software-rendering performance target.

Selected positional comparison: `before-06.png` and `accepted-native-v2/native-03.png` show the same UW-area camera at zoom 17 and the same 2560x1440 drawable. The earlier capture used a dark persisted theme and the later one a light theme; whole-image color differences are not a flicker metric. The road/stadium/building outlines align, while native readiness traces confirm current retained fills and detailed refinement in the corrected run. The baseline resize deadlock and outline-only/coarse scheduling behavior are the attributed defects, not a claimed theme improvement.

Source/binary/archive hashes are in each runner's `identity.json`. Native versions: vk_renderer 1.3.2 and vk_runtime 0.6.0. The runner records the requested logical dimensions; BMP/PNG dimensions record the actual drawable scale (2x in these captures). The final guard/telemetry/text-retirement changes were subsequently compiled and validated; the four-pass timing receipt records its own exact earlier source hash and must not be called the final bundle's binary hash.

## Final-source verification and development bundle

2026-10-04: the final-source two-pass native replay (`final-native`) completed **64.025 s / 3,840 frames**, warm p50 **16.6073 ms**, p95 **17.0707 ms**, maximum **44.4783 ms** including captures. It recorded zero sampled no-draw decisions, retained-presentation misses, lifecycle/presenter failures or CPU/GPU budget refusals. Final-run payload/RSS peaks were **49.95 MiB decoded**, **145.87 MiB retained GPU**, and **574.14 MiB peak RSS**. L0 pending-dispatch age peaked at 117.1 ms; the main upload queue peaked at 5. RSS varies with workload and driver/preparation allocations, so the earlier 507 MiB receipt is not a guaranteed process ceiling.

The final-source region-switch replay (`final-region-validation`) completed both commits with zero Vulkan validation messages or region/presentation/lifecycle failures. Under validation, North Seattle preparation caused a roughly **5.96 s** frame. The earlier non-validation transition was shorter, but region staging remains synchronous and should move to a cancellable worker before promising responsive large-region switching.

Compilation, focused policy/residency tests, all required unit/contract checks, D1/D2 profile checks, route/place/workspace tests, headless smoke and the source visual artifact passed across `full-regression-v3.log` and `final-regression-tail.log`. The first D3 experimental comparison failed on relative fallback-band peak (26 baseline versus 58 candidate); the isolated unchanged rerun passed with zero trend alerts. Both receipts remain. This comparison is timing-sensitive; no candidate budget preset was promoted and no limits/criteria were relaxed. Do not describe the first full `make test` invocation as an uninterrupted pass.

Main Edit package identity, architecture, namespace, signature and launcher self-tests passed (`main-edit-package-v2.log`). The initial packaging check inherited the diagnostic region override and correctly failed its fake-home namespace expectation; it was rerun with a clean environment. Final notes are followed by one more package rebuild/identity readback so the bundle fingerprint includes this checkpoint. The development bundle is `build/targets/macOS-arm64/dist/dev/main-edit/Carta Main Edit.app`; this work does not replace the installed Desktop app or publish a release.

Live development launch exposed a further startup stall: region discovery used `find` over legacy tile directories and remained blocked before starting the app binary. Discovery now checks only immediate pack roots for `tiles/` or `tiles.mbtiles`, preserving the same selection policy without enumerating tile contents. The stalled diagnostic launch processes were stopped, launcher contract checks were repeated, and the bundle was rebuilt before the final live launch.

A subsequent live attempt remained blocked in shell directory enumeration even after removing recursive `find`. Explicit existing operator-selected roots now bypass shell pack discovery and are handed to the native catalog after window creation, where package validation already occurs. This preserves OS access checks and does not change permissions. Automatic fallback discovery still uses bounded top-level checks. The explicit selection reason is now `explicit-env-directory`; it makes no unverified claim that packs have already been inspected.
