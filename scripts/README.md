# scripts

This directory owns Carta's local wrapper scripts for headless route artifacts,
saved-pin rendering, visualizer drop staging, and upload helpers.

## Ownership

- Rendering wrappers should call the built app or focused helper scripts rather
  than embedding app behavior.
- Visualizer staging and upload helpers are operator surfaces. Publishing or
  remote execution must stay approval-gated and use the relevant handoff lane
  when a remote host is involved.
- Script outputs should land in ignored run, staging, or temporary directories.

## Pass Notes

- `R0`: this is a support-script lane, not runtime source.
- `R1`: duplication candidates include saved-pin command construction and
  artifact path handling.
  - `scripts/saved_pin_common.sh` is the app-local helper for shared saved-pin
    wrapper validation, orientation lists, nonce sanitation, copy-if-requested
    behavior, and narrow argument assembly support.
- `R3`: diagnostics should name expected output roots and failing stage.
- `R4`: shell argument quoting, output roots, and publish boundaries are
  security-sensitive.

## Trusted Operator Overrides

These environment variables are local operator controls. They are not request
JSON, pin data, route names, drop ids, or other user-supplied payload fields.

- `MAPFORGE_BINARY`: executable used by saved-pin render wrappers. It may be an
  executable path or a command name resolved through the operator's `PATH`.
- `MAPFORGE_VIDEO_HELPER`: shell helper used only when saved-pin skill output
  requests video generation. It must point to a readable local script file.
- `MAPFORGE_STAGE_TOOL`: Python staging helper used by visualizer-drop staging.
  It must point to a readable local script file.
- `MAPFORGE_OSMIUM_PATH`: C region/graph conversion override for PBF ingest. It
  is executed argv-style by the tools and must point to an executable osmium
  binary; when unset, the tools use fixed probes plus `PATH` fallback.

Wrapper scripts preflight these override values before invoking them. Keep
remote publish/upload behavior approval-gated through the handoff lane rather
than turning request data into executable paths.
