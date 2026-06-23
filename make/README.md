# make

This directory owns Carta's modular Makefile fragments. The top-level
`Makefile` is intentionally a small include router.

## Ownership

- `sources-*.mk`: source inventory for app, tests, and tools.
- `rules-*.mk`: build, test, ops, memory-check, and tool rules.
- `package-macos.mk`: desktop app packaging and package self-test rules.
- `release.mk`: release artifact helpers.
- `paths.mk`, `config.mk`, `flags.mk`, and `target.mk`: build configuration and
  target selection.

## Pass Notes

- `R0`: keep build routing in this directory and avoid growing the top-level
  `Makefile`.
- `R1`: duplication candidates include repeated target setup and path handling.
- `R3`: package and launcher diagnostics should stay clear and stage-specific.
- `R5`: cheap callable testability probes are grouped under `test-r5-callable`;
  artifact-producing saved-pin/frame/video/visualizer smokes stay in
  `run-headless-smoke-artifacts`.
- `R6`: source-run visual proof starts with `visual-artifact`; later local demo
  proof targets should stay discoverable here.

## Visual Proof Targets

- `visual-harness`: build/readiness target for manual visual validation; it
  prints the source binary path.
- `visual-artifact`: source-run first-frame proof target. It runs the headless
  route demo, writes `visual_artifacts/source_run_first_frame/route_demo_seattle/preview.bmp`,
  validates that the image is nonempty, and prints `visual-artifact=<path>` on
  success. The `visual_artifacts/` root is ignored and is not packaged. Failure
  means the source-built app could not complete the headless route demo or the
  preview artifact was missing/empty.
- `carta-local-proof`: one-command local proof aggregate. It runs
  `test-r5-callable`, `visual-artifact`, `run-headless-smoke-core`,
  `test-headless-saved-pin-visualizer-publish-wrapper`, and
  `package-desktop-self-test` sequentially, then prints
  `carta-local-proof passed`. It runs headlessly, does not perform remote
  publish/upload, and fails when any constituent local proof gate fails.

## R5 Testability Index

Use `make test-r5-callable` for the cheap R5 probes that exercise callable
route-preview, headless-playback, tile-source, source-policy, metrics-rollup,
and saved-pin publish planning behavior without rendering videos, staging
visualizer drops, uploading, or contacting a remote host.

Current callable lane:

- `test-route-preview`: route-preview callable behavior and shared fixture use.
- `test-headless-playback`: headless playback callable parity fixture coverage.
- `test-tile-source-archive`: deterministic archive/cache/fallback tile-source
  fixture coverage.
- `test-runtime-source-policy`: local tile-source policy guardrails.
- `test-archive-metrics-rollup`: local archive metrics rollup contract.
- `test-headless-saved-pin-visualizer-publish-plan`: no-render saved-pin
  visualizer publish request/result planning.

Keep these out of `test-r5-callable` unless explicitly promoting a slow gate:

- `test-headless-saved-pin-visualizer-publish-wrapper`
- `run-headless-smoke-artifacts`
- video, frame, staging, upload, package, and remote publish checks
