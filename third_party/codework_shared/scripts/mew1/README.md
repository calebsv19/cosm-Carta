# MEW1 Build-Operations Helper

`mew1.py` is the reusable, non-runtime helper surface for the Persistent
Main-Edit Worktree Contract. It belongs to scaffold/build operations rather
than `shared/core`, `shared/kit`, a program runtime, Release Control, or the
Production Registry.

The helper is read-only except for the explicit `write-identity` output file.
It never creates, removes, resets, cleans, rebases, merges, launches, closes,
publishes, deploys, or registers a worktree, package, process, or release.

## Commands

- `fingerprint`: reproduce the RayTracing pilot's complete tracked/untracked
  Git source-state fingerprint.
- `lane-state`: read canonical and Main Edit branch, commit, version,
  cleanliness, status paths, source fingerprints, ahead/behind counts, and the
  registered worktree inventory. Its derived state is intentionally limited to
  `synced_clean`, `active_dirty`, `checkpointed`, or `canonical_drift`; later
  lifecycle claims require their own test/adoption evidence.
- `write-identity`: emit `codework_local_development_build_identity_v1` for an
  already-built local development binary.
- `verify-identity`: validate required identity fields and exact binary/source
  bytes. Compatibility schemas must be named explicitly with
  `--accepted-schema`.
- `process-audit`: perform a bounded literal process lookup, with an optional
  bounded `lsof` path fallback. It is evidence only and cannot terminate a
  process.

Run focused tests:

```text
python3 -m unittest discover -s scripts/mew1/tests -v
```

Example RayTracing readback from the CodeWork workspace:

```text
python3 shared/scripts/mew1/mew1.py lane-state \
  --canonical-root ray_tracing \
  --main-edit-root _worktrees/ray_tracing_main_edit
```

Public program documentation should replace these workspace-relative examples
with the portable `<workspace>` and `<repo>` placeholders.
