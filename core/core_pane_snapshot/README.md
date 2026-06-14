# core_pane_snapshot

Shared pane snapshot structs and validation helpers for workspace hosts.

## Scope

1. Define v1 snapshot metadata, node, module-binding, and aggregate view structs.
2. Validate schema/meta, dense node indexes, node ids, graph connectivity, and binding references for imported snapshots.
3. Provide stable result-code strings for host diagnostics.

## Boundary

`core_pane_snapshot` owns shared snapshot schema and validation meaning only.

It does not own:

1. `core_pack` file or chunk IO.
2. JSON parsing or writing.
3. Host runtime layout mutation.
4. Module registry lookup or module descriptor loading.
5. Pane solve, splitter interaction, renderer chrome, or authoring UX.
6. Snapshot persistence timing, storage paths, or recovery policy.
7. App-specific module config/state semantics.

## Contract

- `CorePaneSnapshotV1` is a borrowed validation view over caller-owned node and binding arrays. The library does not retain those arrays after validation returns.
- Validation supports only exact schema `v1.0`. Future minor compatibility remains outside the current contract.
- Meta `flags` and `reserved0` must be zero.
- Leaf node records must use horizontal axis, zero ratio, `UINT32_MAX` child indexes, and zero min-size fields.
- Split node records must use horizontal or vertical axis, finite ratio in `[0, 1]`, finite non-negative min-size fields, and distinct non-self child indexes.
- Node indexes must be dense across `[0, node_count)`, node ids must be non-zero and unique, and every reachable node must be connected from the declared root.
- Module bindings must use non-zero instance, pane, and module ids, and binding pane ids must resolve to leaf nodes only.
- `config_variant` and `state_flags` are carried through binding records but are not semantically validated by this module.
- Temporary internal allocation failure currently returns `CORE_PANE_SNAPSHOT_ERR_INVALID_META`; callers should treat that result as a generic validation/import failure rather than a precise malformed-input distinction.

## Build

```sh
make -C shared/core/core_pane_snapshot
```

## Test

```sh
make -C shared/core/core_pane_snapshot test
```

## Status

Hardened contract/docs/test pass complete (`v0.1.1`):

- README now truth-locks borrowed-array lifetime, carried-but-unvalidated binding fields, and allocation-failure result behavior.
- Standalone tests now cover meta validation, node identity/field-shape failures, non-finite split values, dense-index and disconnected-graph failures, duplicate binding pane and invalid binding ids, zero-binding/null-binding behavior, and full result-string coverage.
- WorkspaceSandbox remains the proving host. Serializer ownership, `core_pack` adapters, JSON helpers, module registry lookup, and runtime mutation remain host or adapter concerns.

## Change Notes

- `0.1.1`: truth-locked borrowed snapshot-array lifetime, carried binding-field boundaries, and allocation-failure result semantics, and expanded standalone validation coverage across meta, node, graph, binding, and result-string paths.
- `0.1.0`: initial v1 snapshot schema structs, validator, and result-string surface.
