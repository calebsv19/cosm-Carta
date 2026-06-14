# Mac VPS Report-Inbox Exec Current State

Status: current shared reference for the Mac-side `report-inbox` exec flow

## Purpose

Describe the current live contract for Mac-originated VPS Codex exec requests
that run through the `report-inbox` lane instead of the trio worker-job lane.

This lane is for:

- VPS inspection and context gathering
- bounded repo/runtime analysis
- durable VPS->Mac result return through one shared thread contract

This lane is not yet for:

- arbitrary broad VPS mutation by default
- public website publication
- replacing the trio worker-job pipeline

## Current Live Flow

Preferred operator path on the Mac:

```bash
python3 bin/vps_exec_request.py \
  --thread-id <thread_id> \
  --subject "<subject>" \
  --author-intent "<intent>" \
  --exec-cwd <vps_cwd> \
  --exec-prompt-text "<prompt>" \
  --execution-profile vps_readonly \
  --upload \
  --run \
  --wait
```

What this wrapper now owns:

1. create local thread
2. upload thread to the VPS home-scoped report-inbox root
3. trigger the VPS `/report-inbox/<thread_id>/run` route
4. do one short first fetch
5. continue slower polling until a terminal exec result appears

## Current Mac Helpers

- `bin/create_codework_report_inbox.py`
- `bin/upload_codework_report_inbox.sh`
- `bin/fetch_codework_report_inbox.sh`
- `bin/run_codework_report_inbox_exec.sh`
- `bin/refresh_codework_report_inbox.py`
- `bin/vps_report_fetch`
- `bin/vps_exec_request.py`

## Shared Thread Shape

- `mac/message_summary.json`
- `mac/message_body.md`
- optional `mac/exec_request.json`
- optional `mac/exec_prompt.md`
- `vps/reply_summary.json`
- `vps/reply_body.md`
- optional `vps/analysis.json`
- optional `vps/exec_result.json`
- optional `vps/exec_run.log`
- `status/thread_status.json`
- `status/timeline.ndjson`
- `links/related_run_id.txt`
- `links/related_export_item_id.txt`
- `links/preview_url.txt`

Mac-only state stays local:

- `.mac_local_state.json`

## Current Polling Defaults

- first fetch delay: `10` seconds
- steady-state poll interval: `180` seconds
- recommended heavier-task interval: `300` seconds

The goal is:

- catch quick completions without immediate noisy polling
- let longer VPS tasks gather context before the next fetch

## Current VPS Profiles

Allowlisted today:

- `vps_readonly`
- `dashboard_workspace_readonly`
- `dashboard_workspace_write`
- `vps_workspace_write`
- `vps_apply_visualizer_catalog`
- `vps_verify_visualizer_catalog`
- `vps_apply_visualizer_site_static`
- `vps_verify_visualizer_site_static`
- `vps_publish_visualizer_drop`
- `vps_apply_root_site_static`
- `vps_verify_root_site_static`
- `vps_apply_root_site_static_privileged`
- `vps_apply_ecosystem_static`
- `vps_verify_ecosystem_static`
- `vps_validate_caddy_config`
- `vps_apply_caddy_config`
- `vps_service_status_web`
- `vps_service_reload_web`
- `vps_apply_systemd_units`
- `vps_apply_trader_lab_runtime`
- `vps_verify_trader_lab_app`
- `vps_apply_trader_lab_live`
- `vps_verify_trader_lab_http_loopback`
- `worker_runtime_tmp_write`

Current limitations:

- the live default remains read-only
- `systemctl`/system-bus inspection is still constrained inside the read-only
  Codex exec sandbox
- VPS compatibility handling is now expected to stay input-only during the
  transition window; the Mac now emits the canonical stored request shape and
  treats the returned `mac/exec_request.json` as source of truth
- `dashboard_workspace_write` is now live for bounded source-workspace edits in:
  - `/home/caleb/vps-observability-dashboard`
- `vps_workspace_write` is now live as a broader bounded source/docs profile
  with a single-selected-workspace-root execution model per run
- `vps_apply_visualizer_catalog` is now live as the first bounded
  helper-managed apply profile and now supports real bounded `/srv` mutation
  after the inner launch path began forwarding explicit writable roots into the
  spawned `codex exec`
- `vps_verify_visualizer_catalog` is now live and can rebuild under `/tmp` and
  compare against live `/srv` data without mutating runtime targets
- `vps_apply_visualizer_site_static` and
  `vps_verify_visualizer_site_static` are now live as the bounded visualizer
  site-shell publication pair, with generated `site/data` intentionally left
  to the separate catalog/publish lanes
- `vps_publish_visualizer_drop` is now live for one staged visualizer drop at
  a time plus the derived catalog rebuild it requires
- `vps_apply_root_site_static` and `vps_verify_root_site_static` are now live
  as bounded static-site profiles, but the first real root-site apply proved
  `/var/www/calebsv.tech/root` is owned by `root:root` and therefore still
  needs a separate privilege model before it becomes a usable deploy lane
- `vps_apply_ecosystem_static` and `vps_verify_ecosystem_static` are now live
  as a second bounded static-site apply/verify pair, with backend deploy and
  service-control intentionally left out of scope
- `vps_apply_ecosystem_backend_live` is now live as the bounded Ecosystem
  backend lane:
  sync the writable runtime server tree and restart only `ecosystem-api`
  through one allowlisted helper, executed from the outer runner so
  dependency install happens in the normal VPS shell context instead of the
  inner codex sandbox
- `vps_apply_vanlife_petcare_app` and `vps_verify_vanlife_petcare_app` are
  now live as the bounded Vanlife/Petcare lane family:
  a writable `/srv/websites/Vanlife_Petcare` app-sync lane with service
  restart intentionally kept separate
- `vps_apply_vanlife_petcare_live` is now live as the bounded single-flow
  Vanlife/Petcare lane:
  sync the writable app tree and restart only `vanlife-petcare` through one
  allowlisted helper
- the Mac-side report-inbox run helpers now default to the live private API
  base:
  - `http://10.0.9.10:9102`
  - the old `http://127.0.0.1:9102` default was stale after the dashboard API
    moved to the VPS private interface
- `vps_service_visualizer_web_reload` is now explicitly modeled as planned but
  disabled because the current inner exec still lacks a reliable
  `systemctl`/system-bus boundary
- `vps_apply_systemd_units` is now live as a fixed privileged wrapper lane for
  an allowlist of existing managed installers under `/home/caleb/bin/ops`
- Trader Lab runtime/bootstrap lanes are now staged:
  - `vps_apply_trader_lab_runtime`
  - `vps_verify_trader_lab_app`
  - `vps_apply_trader_lab_live`
  - `vps_verify_trader_lab_http_loopback`
- current Trader Lab publication boundary:
  - the runtime tree under `/srv/websites/trader-lab` is bootable on loopback
  - `stocks.calebsv.tech` Caddy publication is now live through the existing
    fixed wrapper and still returns the intended `404` to non-WireGuard paths
  - a WireGuard-path probe on the VPS now reaches the Trader Lab backend
    through Caddy, confirming the private reverse-proxy route is live
  - the remaining host privilege gap is narrower:
    the Trader Lab managed systemd selector is still missing from the live
    root-owned managed-systemd wrapper and matching sudoers fragment
  - until that host-owned wrapper/sudoers pair is refreshed, the bounded
    publish lanes cannot finish `trader-lab-backend` systemd publication
- `vps_verify_trader_lab_http_loopback` is live as a bounded outer-runner HTTP
  probe profile:
  - it checks only `http://127.0.0.1:8000/api/health` and
    `http://127.0.0.1:8000/api/system`
  - it does not grant network to the inner `codex exec` sandbox
  - proof thread `vps-trader-lab-loopback-http-proof-20260603` returned HTTP
    `200` for both endpoints
  - the Mac request generator now rejects `--allow-network` for profiles whose
    canonical `network_policy` is forbidden, so
    `permissions.allow_network` and `network_policy` do not drift apart

## Proven Live State

The Mac-side flow has now been proven through full live wrapper smokes:

- `mac-vps-exec-full-smoke-20260523`
- `mac-vps-exec-post-fix-smoke-20260523`
- `mac-vps-canonical-shape-smoke-20260523`
- `mac-dashboard-workspace-write-smoke-rerun-20260523`
- `mac-dashboard-workspace-write-final-rerun-20260523`
- `mac-dashboard-workspace-write-bmp-png-proof-20260523`
- `growth-sim-worker-real-test-runtime-write-rerun-20260523`
- `growth-sim-worker-real-test-runtime-write-rerun-post-root-fix-20260524`
- `vps-workspace-write-source-enable-20260523a`
- `vps-workspace-write-smoke-docs-20260523a`
- `vps-workspace-write-add-bin-release-20260524a`
- `vps-workspace-write-smoke-bin-release-20260524a`
- `vps-apply-profile-prep-audit-20260524a`
- `vps-implement-apply-profile-slice-20260524a`
- `vps-inspect-new-profile-defs-20260524a`
- `vps-visualizer-apply-preflight-20260524a`
- `vps-apply-visualizer-catalog-live-20260524a`
- `vps-verify-visualizer-catalog-live-20260524a`
- `vps-inspect-inner-sandbox-launch-20260524a`
- `vps-fix-inner-writable-roots-20260524a`
- `vps-apply-visualizer-catalog-retry-20260524b`
- `vps-visualizer-post-apply-url-check-20260524a`
- `vps-service-status-feasibility-20260524a`
- `vps-next-slice-root-site-audit-20260524a`
- `vps-add-root-site-apply-profiles-20260524a`
- `vps-root-site-proof-source-write-20260524a`
- `vps-apply-root-site-static-20260524a`
- `vps-verify-root-site-static-20260524a`
- `vps-next-slice-ecosystem-audit-20260524a`
- `vps-add-ecosystem-static-profiles-20260524a`
- `vps-ecosystem-proof-source-write-20260524a`
- `vps-apply-ecosystem-static-20260524a`
- `vps-verify-ecosystem-static-20260524a`
- `trader-lab-vps-runtime-apply-20260526a`
- `trader-lab-vps-runtime-apply-20260526b`
- `trader-lab-vps-systemd-apply-20260526a`

Current proven behaviors:

- one-command create/upload/run/wait works
- terminal `exec_result.json` is fetched back to the Mac
- VPS-side outer-runner finalization now produces clean reply text instead of
  the older misleading inner-session write-blocker wording
- canonical single-shape request emission is now proven live against the
  deployed read-only VPS path
- bounded `dashboard_workspace_write` is now proven live from the Mac side:
  - docs-only workspace write succeeds
  - returned `exec_result.json` now includes populated write-run metadata
  - no `/srv` mutation, service restart, or network use is needed for the
    bounded source-workspace proof
  - a final rerun now also confirms:
    - the Mac emitter is aligned to the active VPS write profile
    - `validation_commands_run` preserves a complete command string entry
    - local summary drift for `allow_network` has been corrected on the Mac
      side
  - a more complex bounded artifact proof now also confirms:
    - the lane can create a workspace-local `tmp/` proof directory
    - a Python standard-library generator can produce both BMP and PNG outputs
    - multi-file validation commands and checksum/signature reporting work
    - the whole flow can stay inside the allowed workspace root with no `/srv`
      or network use
- bounded `worker_runtime_tmp_write` is now also proven live from the Mac side:
  - unchanged `growth_sim` real-worker rerun succeeds from the live installed
    package root
  - the lane can read `/srv/codework-worker/packages/...`
  - the lane can write run outputs under `/tmp/codework-worker-runs/...`
  - `/srv` write attempts remain rejected
  - the returned reply enumerates the expected worker output artifacts,
    including `report.json`, frame BMPs, field-frame pack output, and
    `preview.mp4`
  - a post-fix rerun now also confirms:
    - `workspace_write_root = "/tmp/codework-worker-runs"`
    - populated `workspace_files_changed` and
      `workspace_files_changed_absolute`
    - the runtime lane now returns the created request, report, preview, frame,
      field-frame, and video paths correctly
- bounded `vps_workspace_write` is now also proven live from the Mac side:
  - VPS dashboard source enablement completed in:
    - `vps-workspace-write-source-enable-20260523a`
  - the profile is now active in VPS source with:
    - single-selected-workspace-root execution per run
    - a fixed default `cwd` inside `allowed_roots`
    - post-run changed-path boundary validation
  - docs-root smoke succeeded in:
    - `vps-workspace-write-smoke-docs-20260523a`
  - release-root expansion completed in:
    - `vps-workspace-write-add-bin-release-20260524a`
  - release-root smoke succeeded in:
    - `vps-workspace-write-smoke-bin-release-20260524a`
  - the lane can now perform bounded source/docs writes in the selected root
    with:
    - no deploy/restart
    - no service control
    - no network
    - no `/srv` mutation
- Trader Lab VPS bootstrap is now partly proven from the Mac side:
  - source upload into `/home/caleb/trader-lab` succeeded
  - bounded runtime sync into `/srv/websites/trader-lab` succeeded after the
    runtime root was pre-created
  - the host Python lacked `venv` and `pip`, so a non-privileged unmanaged
    `uv` install under `/home/caleb/.local/uv-trader-lab` was used to create
    the runtime `.venv`, install `fastapi` and `uvicorn`, and seed the runtime
    `.env`
  - manual user-space `uvicorn` launch on the VPS answered
    `http://127.0.0.1:8000/api/health` in `observe_only` mode with the kill
    switch enabled
  - the private `stocks.calebsv.tech` Caddy route is now published and proven:
    public access returns `404`, while a WireGuard-path probe reaches the app
    through Caddy
  - a temporary user-level `systemd --user` fallback service now keeps Trader
    Lab running as `caleb` on the VPS runtime tree
  - bounded root-owned managed-systemd publication is still blocked because the
    live `apply_managed_systemd_unit_privileged` wrapper and matching sudoers
    allowlist still omit the `trader-lab-backend` selector
- bounded visualizer apply/verify profile bring-up is now also proven from the
  Mac side:
  - preflight audit completed in:
    - `vps-visualizer-apply-preflight-20260524a`
  - first live apply attempt completed in:
    - `vps-apply-visualizer-catalog-live-20260524a`
  - first live verify proof completed in:
    - `vps-verify-visualizer-catalog-live-20260524a`
  - inner launch-path inspection completed in:
    - `vps-inspect-inner-sandbox-launch-20260524a`
  - writable-root forwarding fix landed in live VPS source in:
    - `vps-fix-inner-writable-roots-20260524a`
  - post-fix live apply retry succeeded in:
    - `vps-apply-visualizer-catalog-retry-20260524b`
  - post-apply live URL verification succeeded in:
    - `vps-visualizer-post-apply-url-check-20260524a`
  - current proven state:
    - the first live apply failure isolated the real defect: the runner passed
      `sandbox_mode` into `codex exec` but dropped explicit `writable_roots`
    - `runReportInboxCodexExec()` now appends one `--add-dir <root>` per
      bounded writable root, excluding duplicates and the primary `cwd`
    - the bounded apply lane can now write into:
      - `/srv/websites/codework-visualizer/current/site/data`
      - `/srv/codework-visualizer/state`
      through the allowlisted helper path
    - the post-fix rebuild changed all four live catalog outputs:
      - `catalog.json`
      - `runs.json`
      - `archives.json`
      - `programs.json`
    - the live visualizer catalog now uses fully qualified
      `https://visualizer.calebsv.tech/artifacts/...` URLs across the four
      generated JSON files, with `625` fully qualified string values and `0`
      remaining root-relative `/artifacts/...` values
    - the verify lane remains useful for file-based diff/compare and tmp-root
      rebuild validation
- bounded second-site static apply bring-up is now also proven from the Mac
  side:
  - service-status feasibility audit completed in:
    - `vps-service-status-feasibility-20260524a`
  - current proven service-status boundary:
    - `systemctl status`, `systemctl show`, `systemctl cat`, and
      `systemctl list-unit-files` are still blocked from the inner exec path by
      `Failed to connect to system scope bus via local transport: Operation not permitted`
    - only weaker offline unit-file inspection plus partial heuristic probes
      (`ps`, partial `ss`, limited `journalctl`) are currently viable
  - root-site static profile bring-up completed in:
    - `vps-next-slice-root-site-audit-20260524a`
    - `vps-add-root-site-apply-profiles-20260524a`
    - `vps-root-site-proof-source-write-20260524a`
    - `vps-apply-root-site-static-20260524a`
    - `vps-verify-root-site-static-20260524a`
  - current proven root-site boundary:
    - the profile model is valid, but the live target
      `/var/www/calebsv.tech/root` is owned by `root:root`
    - `deploy_root_site.sh` exits early with
      `Target directory is not writable by caleb`
    - root site therefore still needs a separate privilege/sudo-style boundary
      before it becomes a usable bounded apply lane
  - ecosystem static profile bring-up completed in:
    - `vps-next-slice-ecosystem-audit-20260524a`
    - `vps-add-ecosystem-static-profiles-20260524a`
    - `vps-ecosystem-proof-source-write-20260524a`
    - `vps-apply-ecosystem-static-20260524a`
    - `vps-verify-ecosystem-static-20260524a`
  - current proven ecosystem static state:
    - the bounded helper `/home/caleb/bin/deploy/deploy_ecosystem.sh` can
      deploy the static site into `/var/www/calebsv.tech/ecosystem`
    - the helper required `--allow-dirty` for the harmless untracked proof
      asset, then completed the sync step successfully
    - the deployed proof file
      `/var/www/calebsv.tech/ecosystem/ops/ecosystem-static-proof-20260524a.txt`
      matches the source proof file exactly
    - source and deployed `index.html` also match at checksum level
    - the helper's post-deploy release-link verification still reported a host
      resolution failure for `ecosystem.calebsv.tech`, so network-dependent
      post-checks remain a separate boundary from the successful filesystem
      apply

## Current Write Profile Truth

Current live bounded write profiles:

- `dashboard_workspace_write`
  - `profile_lifecycle_state = active`
  - `profile_capability = workspace_write`
  - `allowed_roots = ["/home/caleb/vps-observability-dashboard"]`
  - `sandbox_mode = workspace-write`
  - `network_policy = forbid`
  - `git_policy = bounded_local_workspace_changes_no_push`
  - `service_restart_policy = forbidden`
  - `runtime_root_mutation_policy = forbidden`

Current Mac-side emitter now aligns to that live profile policy.

- `worker_runtime_tmp_write`
  - status: live
  - bounded simulation-worker runtime proof lane
  - read roots:
    - `/srv/codework-worker/packages/...`
  - writable runtime roots:
    - `/tmp/codework-worker-runs/...`
  - forbidden actions:
    - `/srv` mutation
    - service control
    - network
    - source-workspace mutation

This lane is intentionally separate from `dashboard_workspace_write`.

- `vps_workspace_write`
  - status: live
  - broader bounded source/docs workspace lane
  - execution model:
    - one selected allowed workspace root per run, derived from `cwd`
  - current live allowed roots:
    - `/home/caleb/vps-observability-dashboard`
    - `/home/caleb/bin/ops`
    - `/home/caleb/bin/caddy`
    - `/home/caleb/bin/deploy`
    - `/home/caleb/bin/release`
    - `/home/caleb/bin/smoke`
    - `/home/caleb/atlas-workspace/atlas`
    - `/home/caleb/config/caddy`
    - `/home/caleb/config/systemd`
    - `/home/caleb/ecosystem-workspace/Ecosystem`
    - `/home/caleb/encyclopedia-workspace/encyclopedia`
    - `/home/caleb/trader-lab`
    - `/home/caleb/visualizer-workspace/codework-visualizer`
    - `/home/caleb/vanlife-workspace/Vanlife_Petcare`
    - `/home/caleb/sites/root-site`
    - `/home/caleb/docs`
  - policy surface:
    - `profile_lifecycle_state = active`
    - `profile_capability = workspace_write`
    - `sandbox_mode = workspace-write`
    - `network_policy = forbid`
    - `git_policy = bounded_single_workspace_root_changes_no_push`
    - `service_restart_policy = forbidden`
    - `runtime_root_mutation_policy = forbidden`
  - current proof:
    - docs-root smoke created:
      - `/home/caleb/docs/current/vps-workspace-write-smoke-20260523a.md`
    - release-root smoke created:
      - `/home/caleb/bin/release/vps-workspace-write-smoke-20260524a.md`
  - current note:
    - the local Mac emitter must mirror the live VPS allowlist and lifecycle
      state to avoid stale `planned` metadata in request summaries

- `vps_apply_visualizer_catalog`
  - status: live
  - bounded helper-managed apply profile for visualizer catalog rebuilds
  - current allowlisted roots:
    - `/home/caleb/bin/release`
    - `/home/caleb/visualizer-workspace/codework-visualizer/server`
    - `/srv/artifacts/codework-visualizer`
    - `/srv/codework-visualizer/state`
    - `/srv/websites/codework-visualizer/current/site/data`
  - helper path:
    - `/home/caleb/bin/release/rebuild_visualizer_catalog.sh`
  - policy surface:
    - `profile_lifecycle_state = active`
    - `profile_capability = bounded_apply`
    - `sandbox_mode = workspace-write`
    - `network_policy = forbid`
    - `git_policy = forbid`
    - `service_restart_policy = forbidden`
    - `source_workspace_mutation_policy = forbidden`
    - `runtime_root_mutation_policy = allow_bounded_helper_managed_runtime_targets`
  - proof sequence:
    - first live run:
      - helper execution started cleanly, but the actual runtime write failed
        on temp-file creation in the live `/srv` data directory
    - post-fix live retry:
      - succeeded after the inner launcher began forwarding explicit
        `writable_roots` with `--add-dir`
  - current live result:
    - the helper can now rebuild the four live visualizer catalog outputs under
      `/srv/websites/codework-visualizer/current/site/data`
    - the active exec path now has effective writable-root authority for this
      bounded `/srv` apply lane

- `vps_verify_visualizer_catalog`
  - status: live
  - bounded file-based verify profile for visualizer catalog inspection
  - current allowlisted roots:
    - `/home/caleb/bin/release`
    - `/home/caleb/bin/smoke`
    - `/home/caleb/visualizer-workspace/codework-visualizer/server`
    - `/srv/artifacts/codework-visualizer`
    - `/srv/codework-visualizer/state`
    - `/srv/websites/codework-visualizer/current/site/data`
    - `/tmp`
  - writable verify root:
    - `/tmp/codework-visualizer-catalog-verify`
  - helper paths:
    - `/home/caleb/bin/release/rebuild_visualizer_catalog.sh`
    - `/home/caleb/bin/smoke/smoke_visualizer_private_site.sh`
  - first live result:
    - tmp rebuild proof succeeded and produced:
      - `/tmp/codework-visualizer-catalog-verify/output/catalog.json`
      - `/tmp/codework-visualizer-catalog-verify/output/runs.json`
      - `/tmp/codework-visualizer-catalog-verify/output/archives.json`
      - `/tmp/codework-visualizer-catalog-verify/output/programs.json`
      - `/tmp/codework-visualizer-catalog-verify/state/visualizer.db`
      - `/tmp/codework-visualizer-catalog-verify/state/catalog-last-built.txt`
    - after copying the live `visualizer.db` into the tmp state root, the tmp
      rebuild matched live run/archive totals and isolated the remaining
      difference to:
      - `generated_at`
      - URL style
    - live files still contain root-relative `/artifacts/...` values, while
      the tmp rebuild contains fully qualified
      `https://visualizer.calebsv.tech/artifacts/...` values

- `vps_apply_visualizer_site_static`
  - status: live
  - bounded helper-managed visualizer site-shell apply profile
  - current allowlisted roots:
    - `/home/caleb/bin/deploy`
    - `/home/caleb/visualizer-workspace/codework-visualizer`
    - `/srv/websites/codework-visualizer/current/site`
    - `/srv/websites/codework-visualizer/manifests`
  - helper path:
    - `/home/caleb/bin/deploy/deploy_visualizer_site.sh`
  - current live result:
    - bounded live shell publication is proven
    - generated `site/data` remains intentionally preserved for the separate
      catalog and staged-drop lanes

- `vps_verify_visualizer_site_static`
  - status: live
  - bounded file-based verify profile for visualizer site-shell source vs
    deployed comparison
  - writable verify root:
    - `/tmp/visualizer-site-static-verify`
  - current live result:
    - the deployed shell matches the source shell with `diff -rq --exclude data`

- `vps_publish_visualizer_drop`
  - status: live
  - bounded helper-managed staged-drop publication profile
  - current allowlisted roots:
    - `/home/caleb/bin/release`
    - `/srv/release-staging/codework-visualizer`
    - `/srv/artifacts/codework-visualizer`
    - `/srv/codework-visualizer/state`
    - `/srv/websites/codework-visualizer/current/site/data`
  - helper paths:
    - `/home/caleb/bin/release/publish_visualizer_drop.sh`
    - `/home/caleb/bin/release/verify_staged_visualizer_drop.sh`
    - `/home/caleb/bin/release/rebuild_visualizer_catalog.sh`
  - current live result:
    - bounded live publish proofs now cover the missing `ctest01` and
      `ctest02` staged drops
    - the live visualizer site data now contains the full
      `ctest01..ctest04` continuation lineage needed by sequence grouping
    - the later visualizer refresh follow-up now also confirms:
      - targeted single-drop publish is the correct operator path for one
        requested staged drop
      - the two malformed stale batch blockers
        `ray-tracing--motion-review--20260521t000331z--grimev2` and
        `ray-tracing--trio-headless-worker--20260521T025403Z--deepramp-preview`
        have been removed from `/srv/release-staging/codework-visualizer`
      - the topbar `Refresh Artifacts` action remains a batch publish path, so
        it is operationally broader than the per-drop publish route

- `vps_apply_root_site_static`
  - status: live
  - bounded helper-managed static root-site apply profile
  - current allowlisted roots:
    - `/home/caleb/bin/deploy`
    - `/home/caleb/sites/root-site`
    - `/var/www/calebsv.tech/root`
  - helper path:
    - `/home/caleb/bin/deploy/deploy_root_site.sh`
  - current live result:
    - profile activation is complete
    - first real apply failed because `/var/www/calebsv.tech/root` is
      `root:root` and not writable by `caleb`
    - this lane is therefore modeled but not yet operational without a
      separate privilege boundary

- `vps_verify_root_site_static`
  - status: live
  - bounded file-based verify profile for root-site source vs deployed static
    target comparison
  - writable verify root:
    - `/tmp/root-site-static-verify`
  - current note:
    - verify is structurally sound, but the first proof also confirmed the
      deployed proof file is absent because the corresponding apply lane is
      still blocked by target ownership

- `vps_apply_root_site_static_privileged`
  - status: live
  - bounded privileged root-site static apply profile for the root-owned live
    target
  - intended privilege model:
    - one fixed root-owned wrapper such as
      `/usr/local/bin/deploy_root_site_static_privileged`
    - invoked only through an allowlisted `sudo -n` rule for `caleb`
    - no caller-supplied path overrides or arbitrary shell arguments
  - required guardrails:
    - fixed source root:
      - `/home/caleb/sites/root-site`
    - fixed live target:
      - `/var/www/calebsv.tech/root`
    - fail-closed preflight before any sync
    - no `--with-caddy`
    - no service reload
    - no broad `sudo` or broad `/var/www` mutation outside that one target
  - current live state:
    - the fixed wrapper is installed at:
      - `/usr/local/bin/deploy_root_site_static_privileged`
    - `sudo -n /usr/local/bin/deploy_root_site_static_privileged` succeeds for
      `caleb`
    - extra arguments fail closed
    - the inner exec launcher now forwards:
      - `/home/caleb/sites/root-site`
      - `/usr/local/bin`
      - `/var/www/calebsv.tech/root`
      into the bounded Mac-originated privileged apply lane
    - the outer runner now handles the privileged helper execution directly for
      this profile instead of sending `sudo -n` through the inner sandboxed
      `codex exec`
    - fetched proof thread
      `vps-apply-root-site-static-privileged-20260524b` now proves:
      - outer-runner command:
        - `sudo -n /usr/local/bin/deploy_root_site_static_privileged`
      - helper exit code: `0`
      - live deployed proof file:
        - `/var/www/calebsv.tech/root/assets/ops/root-site-privileged-proof-20260524a.txt`
      - readback contents:
        - `Mac-side privileged root-site lane proof created on 2026-05-24.`

- `vps_apply_ecosystem_static`
  - status: live
  - bounded helper-managed ecosystem static apply profile
  - current allowlisted roots:
    - `/home/caleb/bin/deploy`
    - `/home/caleb/ecosystem-workspace/Ecosystem`
    - `/var/www/calebsv.tech/ecosystem`
  - helper path:
    - `/home/caleb/bin/deploy/deploy_ecosystem.sh`
  - current live result:
    - bounded apply is successful for the static ecosystem site target
    - backend deploys and service restarts remain explicitly out of scope

- `vps_apply_ecosystem_backend_live`
  - status: live
  - bounded helper-managed Ecosystem backend sync + restart profile
  - live audit findings now confirm:
    - source backend:
      - `/home/caleb/ecosystem-workspace/Ecosystem/server`
    - live runtime:
      - `/srv/websites/Ecosystem/server`
    - runtime ownership:
      - `caleb:caleb`
    - service unit:
      - `ecosystem-api.service`
    - service user:
      - `caleb`
    - `CanReload=no`, so restart is the correct single-flow action
  - intended lane boundary:
    - use one allowlisted helper only
    - sync the backend workspace into `/srv/websites/Ecosystem/server`
    - restart only `ecosystem-api`
    - run the helper from the outer runner so npm install uses the normal VPS
      shell/network context
    - reuse a bounded non-privileged process recycle fallback, but wait through
      the bounded auto-restart window before failing when the service is
      between crash and recovery
    - do not widen into generic service control

- `vps_verify_ecosystem_static`
  - status: live
  - bounded file-based verify profile for ecosystem static source vs deployed
    target comparison
  - writable verify root:
    - `/tmp/ecosystem-static-verify`
  - current live result:
    - the deployed proof asset matches the source proof asset exactly
    - source and deployed `index.html` match at checksum level

- `vps_apply_vanlife_petcare_app`
  - status: live
  - bounded helper-managed Constant PetCare app-sync profile
  - public site identity:
    - `petcare.calebsv.tech`
    - brand: `Constant PetCare`
  - legacy implementation/profile names still use `vanlife_petcare` and
    `Vanlife_Petcare`
  - live audit findings now confirm:
    - source workspace:
      - `/home/caleb/vanlife-workspace/Vanlife_Petcare`
    - live runtime target:
      - `/srv/websites/Vanlife_Petcare`
    - live runtime target ownership:
      - `caleb:caleb`
    - existing helper:
      - `/home/caleb/bin/deploy/deploy_vanlife_petcare.sh`
    - the helper already supports a bounded non-restart path:
      - `--skip-restart`
    - the helper syncs source into the live app tree and installs production
      dependencies under the writable runtime target
  - intended lane boundary:
    - use the helper in app-sync mode only
    - force `--skip-restart`
    - keep `vanlife-petcare` service restart separate from this lane
    - do not widen into arbitrary `/srv` mutation or generic npm execution
  - current live result:
    - the live worker now recognizes the bounded app-sync profile
    - the deployed proof file exists at:
      - `/srv/websites/Vanlife_Petcare/ops/vanlife-app-proof-20260525a.txt`
    - the deployed proof file contents match the source proof marker exactly

- `vps_verify_vanlife_petcare_app`
  - status: live
  - bounded file-based verify profile for Constant PetCare source vs deployed
    runtime app tree comparison
  - intended verify exclusions:
    - `.git/`
    - `node_modules/`
    - `.env`
    - `server/data/`
  - writable verify root:
    - `/tmp/vanlife-petcare-verify`
  - current live result:
    - direct checksum comparison confirms the source and deployed proof marker
      match exactly
    - the only observed diff sample under the verify exclusion set is:
      - `Only in /srv/websites/Vanlife_Petcare/server/data: text-overrides.json`
    - that drift remains inside the intentionally excluded `server/data/`
      subtree

- `vps_apply_vanlife_petcare_live`
  - status: live
  - bounded helper-managed Constant PetCare sync + restart profile
  - public site identity:
    - `petcare.calebsv.tech`
    - brand: `Constant PetCare`
  - legacy implementation/profile names still use `vanlife_petcare` and
    `Vanlife_Petcare`
  - live audit findings now confirm:
    - the service unit:
      - `vanlife-petcare.service`
    - the service user:
      - `caleb`
    - the live process user:
      - `caleb`
    - `CanReload=no`, so the correct one-step behavior is restart rather than
      reload
  - live proof now confirms:
    - helper path:
      - `/home/caleb/bin/deploy/apply_vanlife_petcare_live.sh`
    - deploy/sync succeeds into:
      - `/srv/websites/Vanlife_Petcare`
    - restart succeeds through the bounded `caleb`-owned process recycle
      fallback
    - live service readback shows a fresh `vanlife-petcare` `MainPID` and
      fresh `since` timestamp after the rerun
  - lane boundary:
    - use one allowlisted helper only
    - sync the workspace into `/srv/websites/Vanlife_Petcare`
    - restart only `vanlife-petcare`
    - reuse the existing non-privileged systemd-supervised process recycle
      fallback when passwordless sudo is unavailable
    - do not widen into generic service control
  - preferred operator path when the Mac repo is the real source of truth:
    - first stage the current local tree into
      `/home/caleb/vanlife-workspace/Vanlife_Petcare`
    - then run the live apply with the intentional dirty-workspace override,
      because the staged VPS workspace now mirrors the desired Mac source
    - current wrapper:
      - `python3 bin/vps_stage_and_apply_vanlife_petcare_live.py`
    - operator-facing catalog:
      - `skills/mac-vps-handoff/references/deploy-catalog.md`
  - reason for the wrapper:
    - the live apply profile deploys from the VPS workspace
    - it does not fetch from GitHub or mutate git state itself
    - explicit Mac-side staging avoids stale-VPS-clone drift without widening
      the bounded VPS profile into generic git/network authority

- `vps_service_visualizer_web_reload`
  - status: planned
  - intentionally disabled service-control profile
  - current note:
    - the current inner exec path still lacks a reliable `systemctl`
      permission/system-bus boundary, so service status/reload remains
      separated from the now-live source/apply/verify profiles

- `vps_validate_caddy_config`
  - status: live
  - bounded source-validation profile for the managed Caddy template
  - allowlisted roots:
    - `/home/caleb/config/caddy`
    - `/home/caleb/bin/caddy`
  - live audit findings already confirm:
    - `caddy validate --config /home/caleb/config/caddy/Caddyfile.calebsv.tech.var-www`
      succeeds
    - the same template still has `caddy fmt` drift
  - lane boundary:
    - no `/etc/caddy` writes
    - no `systemctl reload`
    - no curl/network smoke

- `vps_apply_caddy_config`
  - status: live
  - bounded privileged Caddy apply profile
  - live audit findings now confirm:
    - the managed source roots are:
      - `/home/caleb/config/caddy`
      - `/home/caleb/bin/caddy`
    - current helper scripts under `/home/caleb/bin/caddy/apply_*_caddy_site.sh`
      inline:
      - source validation
      - backup of `/etc/caddy/Caddyfile`
      - install of the managed template
      - live validation
      - `systemctl reload caddy`
      - optional curl probes
    - the live target is:
      - `/etc/caddy/Caddyfile`
    - the live target is owned by:
      - `root:root`
    - `sudo -n true` was not previously available for the raw helper scripts,
      which is why the lane was bootstrapped through a fixed wrapper instead of
      widening direct helper authority
  - current live privilege model:
    - fixed wrapper:
      - `/usr/local/bin/apply_caddy_config_privileged`
    - fixed sudoers fragment:
      - `/etc/sudoers.d/apply_caddy_config_privileged`
    - outer-runner execution, not inner-sandbox `sudo`
    - fixed source template:
      - `/home/caleb/config/caddy/Caddyfile.calebsv.tech.var-www`
    - fixed live target:
      - `/etc/caddy/Caddyfile`
    - bounded duties:
      - validate source
      - back up live file
      - install the managed template
      - validate live file
      - reload `caddy`
      - check `systemctl is-active caddy`
    - excluded from this lane:
      - arbitrary sudo
      - caller-supplied target overrides
      - curl/network probes
      - generic service control beyond the fixed wrapper
  - fetched bootstrap + proof now confirm:
    - wrapper install succeeded
    - sudoers validation succeeded
    - `vps_apply_caddy_config` executes through the outer runner
    - `sudo -n /usr/local/bin/apply_caddy_config_privileged` completed with
      exit code `0`
    - live readback of `/etc/caddy/Caddyfile` succeeded

- `vps_service_status_web`
  - status: live
  - bounded outer-runner read-only service-status profile
  - allowlisted roots:
    - `/home/caleb/config/systemd`
    - `/etc/systemd/system`
    - `/usr/lib/systemd/system`
  - allowlisted units:
    - `caddy`
    - `system-dashboard-api`
    - `system-dashboard-history-sampler`
    - `vanlife-petcare`
    - `encyclopedia-api`
    - `codework-worker-dispatcher`
  - live audit findings already confirm:
    - `systemctl is-enabled`, `systemctl is-active`, `systemctl show`, and
      bounded `systemctl status --no-pager --lines=<small>` all work from the
      normal VPS shell for these units without sudo
    - the same commands remain blocked from the inner sandboxed exec path by
      the system-bus boundary
  - lane boundary:
    - outer-runner execution only
    - no reload/restart/start/stop
    - no arbitrary unit selection
    - no journalctl or network probes
  - fetched worker activation + proof now confirm:
    - live worker profile is active
    - proof thread:
      - `vps-service-status-web-proof-20260524a`
    - outer-runner summary:
      - `Outer runner executed the allowlisted read-only systemctl status lane directly for this profile.`
    - recorded commands include:
      - `/usr/bin/systemctl is-enabled <unit>`
      - `/usr/bin/systemctl is-active <unit>`
      - `/usr/bin/systemctl show <unit> --property=Id,LoadState,ActiveState,SubState,UnitFileState,FragmentPath`
      - `/usr/bin/systemctl status <unit> --no-pager --lines=12`

- `vps_service_reload_web`
  - status: live
  - bounded privileged service-reload profile
  - live audit findings now confirm:
    - `systemctl reload caddy` requires interactive authentication from the
      normal VPS shell
    - `systemctl reload codework-worker-dispatcher` also requires interactive
      authentication
    - among the currently audited web units, only `caddy` reports:
      - `CanReload=yes`
    - `system-dashboard-api` and `codework-worker-dispatcher` currently report:
      - `CanReload=no`
  - current live privilege model:
    - fixed wrapper:
      - `/usr/local/bin/reload_caddy_service_privileged`
    - fixed sudoers fragment:
      - `/etc/sudoers.d/reload_caddy_service_privileged`
    - outer-runner execution, not inner-sandbox `systemctl`
    - caddy-only scope for the first reload lane
  - fetched bootstrap + proof now confirm:
    - wrapper install succeeded
    - sudoers validation succeeded
    - `vps_service_reload_web` executes through the outer runner
    - `sudo -n /usr/local/bin/reload_caddy_service_privileged` completed with
      exit code `0`
    - proof thread:
      - `vps-service-reload-web-proof-20260524a`
    - recorded result:
      - `active`
  - excluded from this first reload lane:
    - arbitrary unit names
    - restart/start/stop
    - journalctl
    - generic service control beyond the fixed wrapper

- `vps_apply_systemd_units`
  - status: live
  - bounded privileged managed-systemd publication profile
  - live audit findings now confirm:
    - managed source root:
      - `/home/caleb/config/systemd`
    - existing installer roots:
      - `/home/caleb/bin/ops`
    - live target:
      - `/etc/systemd/system`
    - live target ownership:
      - `root:root`
    - existing managed installers already exist for:
      - `visualizer-api`
      - `encyclopedia-api`
      - `codework-worker-dispatcher`
      - `vanlife-refresh`
    - the raw installer scripts still require privilege directly, which is why
      the lane uses a fixed wrapper instead of calling those installers from
      the inner sandbox
  - current live privilege model:
    - one fixed privileged wrapper such as:
      - `/usr/local/bin/apply_managed_systemd_unit_privileged`
    - outer-runner execution, not inner-sandbox `sudo`
    - one explicit allowlisted selector mapped to the existing managed
      installers under `/home/caleb/bin/ops`
  - excluded from this live lane:
    - arbitrary unit-file copies
    - arbitrary unit names
    - generic `systemctl`
    - generic `/etc/systemd/system` mutation outside the allowlisted units
  - fetched live proof now confirms:
    - proof thread:
      - `vps-apply-systemd-units-proof-20260525b`
    - selected selector:
      - `encyclopedia-api`
    - recorded command:
      - `sudo -n /usr/local/bin/apply_managed_systemd_unit_privileged encyclopedia-api`
    - live readback:
      - `/etc/systemd/system/encyclopedia-api.service`
    - proof exit code:
      - `0`

## Future v0.2 Direction

The next major expansion should be separate service-aware apply/reload profiles
rather than widening the normal bounded source/docs write lanes implicitly. The
immediate blocker has shifted:

- keep the current source/config coverage
- preserve the now-working apply + verify pair as the template for later
  bounded runtime lanes
- treat root-owned `/var/www` targets as a separate privilege-boundary problem,
  even when the profile model itself is valid
- for that privilege-boundary case, the preferred model is now live:
  one fixed root-owned wrapper invoked via an allowlisted `sudo -n` rule
  instead of ownership drift or broad ACL changes
- the next concrete application of that pattern is now also live:
  Caddy now uses a read-only source-validation lane plus one fixed privileged
  apply wrapper instead of widened `/etc/caddy` or generic `systemctl`
  authority
- the next concrete service-observability step is now explicit:
  use the now-live outer-runner read-only allowlisted `systemctl` status lane
  rather than trying to make inner-sandbox `systemctl` work
- the next concrete control step should stay narrow:
  keep the now-live caddy-only privileged reload wrapper narrow instead of
  widening reload authority across all web units
- continue to keep service-control as a separate later profile family
- current CodeWork worker follow-up is now proven live:
  the `codework-worker-dispatcher` managed apply lane can now publish a changed
  unit and force the running dispatcher to pick it up, because the managed
  installer now does `enable`, then `restart` when already active or `start`
  otherwise
- the adjacent bounded source-write lane is now also widened live to include:
  - `/home/caleb/bin/ops`
  so managed installer scripts can be patched through the Mac->VPS report-inbox
  flow instead of falling back to ad hoc shell work
- this exact combination is now proven with the dispatcher path:
  patch `/home/caleb/bin/ops/install_codework_worker_dispatcher_service.sh`
  through `vps_workspace_write`, then reapply
  `codework-worker-dispatcher.service` through `vps_apply_systemd_units`, with
  the live dispatcher re-registering
  `trio-headless-v1`, `behavior-sim-visual-review-v1`, and
  `ball-bounce-sim-headless-review-v1`
- the report-inbox exec lifecycle now also has live stale-running recovery:
  new exec threads persist outer runner and inner child PID metadata in
  `vps/exec_result.json`, and the live thread read path now recovers
  `running` execs into terminal failure when both tracked processes are gone
- that recovery path is now proven end to end through:
  - `vps-report-inbox-exec-recovery-fix-20260525a`
  - `vps-reapply-dispatcher-for-exec-recovery-20260525a`
  - `vps-stale-running-recovery-proof-20260525a`

Current known non-blocking quality issues:

- nested multi-file writes under one new directory can currently report
  `workspace_files_changed = ["tmp/"]` instead of an itemized file list
- the lane is functionally correct, but changed-file reporting is still coarse
  for directory-shaped proofs
- pre-recovery legacy stuck threads that were created before PID metadata was
  persisted are not auto-finalized blindly and still require manual cleanup if
  an operator wants those historical `running` artifacts cleared

Current next profile-enable boundary:

- the current live source-workspace lane is still only:
  - `/home/caleb/vps-observability-dashboard`
- the current live runtime lane is still intentionally separate and bounded to:
  - package reads under `/srv/codework-worker/packages/...`
  - tmp-only writes under `/tmp/codework-worker-runs/...`
- `vps_workspace_write` is now the live broader bounded source/docs lane
- service reload/apply remains intentionally separate from that live profile

Recommended direction:

- keep the outer runner as the authoritative thread-artifact writer
- add explicit write-capable profiles with narrow path allowlists
- keep source work in source workspaces, not live `/srv` runtime roots, unless a
  profile explicitly says otherwise
- for new managed VPS apps such as Trader Lab, extend source/config coverage in
  `vps_workspace_write` first, then add separate Caddy/systemd apply selectors
  instead of widening the source-write lane into runtime bootstrap authority

Candidate profile families:

- `vps_workspace_write`
- `dashboard_workspace_write`

Each future write-capable profile should standardize:

- allowed working roots
- whether network is permitted
- whether Git operations are allowed
- whether service-control inspection/restarts are allowed
- expected output and artifact return behavior
- whether edits are advisory, patch-producing, or directly mutating
