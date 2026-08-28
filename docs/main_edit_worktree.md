# MapForge Main Edit Worktree

The persistent Main Edit lane is the default single-writer integration lane for
ongoing `map_forge` / `Carta` functional development.

## Lane Identity

- canonical branch: `master`
- Main Edit branch: `codex/map-forge-main-edit`
- worktree convention: `<workspace>/_worktrees/map_forge_main_edit`
- development app: `Carta Main Edit.app`
- bundle ID: `com.cosm.carta.main-edit`
- runtime/log namespace: `MapForge-Main-Edit`
- identity schema: `codework_local_development_build_identity_v1`

The program `VERSION` is inherited from canonical and is not changed merely to
start or checkpoint development work.

## Start And Checkpoint Gates

Before editing, read every registered worktree and stop on unexpected
ownership, dirty paths, canonical product drift, or a second writer. Preserve
ignored build, region, runtime, package, proof, pin, local-icon, release,
worker, and vendored shared roots.

```sh
git -C <repo> status --short
git -C <repo> worktree list --porcelain
git -C <repo> rev-list --left-right --count master...codex/map-forge-main-edit
make -C <workspace>/_worktrees/map_forge_main_edit
make -C <workspace>/_worktrees/map_forge_main_edit main-edit-package-contract-checks
make -C <workspace>/_worktrees/map_forge_main_edit test
make -C <workspace>/_worktrees/map_forge_main_edit run-headless-smoke
make -C <workspace>/_worktrees/map_forge_main_edit visual-artifact
make -C <workspace>/_worktrees/map_forge_main_edit package-desktop-main-edit-self-test
git -C <workspace>/_worktrees/map_forge_main_edit diff --check
```

## Specialist Worktrees

MEW1 does not absorb or authorize cleanup of historical-evidence, security,
release, worker, renderer, or feature worktrees. A missing checkout marked
prunable by Git remains registered until a separately authorized ownership and
cleanup decision is complete.

## Integration And Retention Gates

Freshly classify canonical-only commits and rerun affected gates after any
reconciliation. Fast-forward only when canonical remains an ancestor of the
verified Main Edit tip. Independently read back final canonical identity and
cleanliness. Retain the named lane by default; recycling requires clean tracked
and untracked state, retained commit reachability, explicit ignored-artifact
handling, and no process owner.

Source adoption does not authorize a version change, release artifact,
Registry mutation, publication, deployment, push, Desktop-app replacement, or
specialist-lane cleanup.
