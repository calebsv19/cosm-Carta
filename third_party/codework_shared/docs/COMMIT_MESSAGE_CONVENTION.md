# Shared Commit Message Convention

Use this format for staged baseline history:

`<type>(<scope>): <summary>`

## Allowed types
- `chore`
- `feat`
- `docs`
- `test`

## Standard scopes for this repo
- `repo`
- `core`
- `kit`
- `scripts`
- `sys_shims`
- `runtime`
- `showcase`
- `docs`

## Baseline commit sequence
1. `chore(repo): initialize shared repo scaffolding and policy`
2. `feat(core): initialize core module baseline`
3. `feat(kit): initialize kit module baseline`
4. `chore(scripts): initialize shared script baseline`
5. `feat(sys_shims): initialize sys_shims module baseline`
6. `feat(runtime): initialize assets, shape, timer_hud, and vk_renderer baseline`
7. `feat(showcase): initialize showcase app baseline`
8. `docs(docs): initialize shared public docs baseline`

## Optional body template
```
Scope: shared/<directory or group>
Intent: initialize release-ready baseline
Notes: <important caveats>
```
