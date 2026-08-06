# core_screen_pick

`core_screen_pick` is the renderer-neutral screen-space candidate index used by
CodeWork editors to make whole-object hover and selection deterministic.

Version: `0.1.0`

## Contract

- Callers project one eligible selection anchor per object into viewport-local
  logical pixels.
- The core stores candidates in a uniform hashed grid.
- Queries use a 28-pixel default capture radius and rank by squared screen
  distance, then frontmost view depth for distance ties, then stable key.
- Hover and click can consume the same immutable index and query semantics.
- Rebuilds are transactional: invalid input or allocation failure preserves the
  prior valid index.
- A bounded ranked query supports host-owned overlap cycling.

## Ownership boundary

The module owns projected-candidate storage, spatial indexing, and deterministic
ranking only. It does not own camera projection, viewport input, visibility,
occlusion, scene identity, selection state, UI, gizmos, handles, topology or
face picking, drag policy, rendering, or authoring arbitration.

## Build and test

```sh
make -C shared/core/core_screen_pick clean all
make -C shared/core/core_screen_pick test
```
