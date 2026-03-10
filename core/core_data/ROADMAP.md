# core_data Roadmap

## Mission
`core_data` defines the canonical in-memory data model across the ecosystem. It is the shared schema layer that allows different apps and kits to interoperate without custom per-app structures.

## North Star End State
- Unified runtime representation for:
  - scalar values
  - arrays
  - structured 2D fields
  - vector fields
  - typed tables
  - typed metadata
- Clear mapping between `core_pack` chunks and in-memory `CoreDataset` items.
- Stable contracts used by analysis, visualization, authoring, and conversion apps.

## Current Surface
- Public API: `include/core_data.h`
- Implementation: `src/core_data.c`
- Tests: `tests/core_data_test.c`

## Product-Level Behavior Goals
- Predictable memory ownership rules for all dataset additions.
- Type-safe metadata/table handling with clear failure behavior.
- Canonical naming and semantics for common data objects (density, velocity, masks, etc.).

## Capability Layers

### Layer 1: Contract Hardening
- Preserve and harden existing dataset add/find/free APIs.
- Extend tests for invalid shape/type combinations and capacity growth.
- Document strict ownership and lifetime behavior for all value kinds.

### Layer 2: Schema and Semantics
- Introduce optional schema descriptors for datasets/items.
- Define conventions for common fields and units (name + unit + interpretation).
- Add validation helpers for shape/type/range checks before downstream rendering/compute.

### Layer 3: Transformation and Views
- Add helper utilities for:
  - derived fields (e.g., speed from velocity)
  - slicing/subsetting
  - lightweight view-based access (non-owning where safe)
- Minimize copy overhead for large data workflows.

### Layer 4: Interop Extensions
- Add optional annotations for provenance (source app, frame index range, transform chain).
- Prepare contracts for 3D fields/time-series references while keeping 2D core stable.
- Keep extensions additive and backward compatible.

## Complexity Targets
- Maintain strict type clarity as complexity grows.
- Keep schema evolution explicit and versioned.
- Prevent module drift into persistence concerns (belongs in `core_pack`).

## Testing Strategy
- Unit tests across each data kind and metadata type.
- Property-style tests for table/field invariants.
- Stress tests for large dimensions and capacity growth.
- Regression tests for schema-validation behavior.

## API Stability Policy
- Core dataset APIs are high-stability.
- New field/table types should be additive.
- Breaking semantic changes require migration docs and sample conversion utilities.

## Integration Expectations
- `kit_viz` and future kits consume `CoreDataset`/`CoreDataItem` directly.
- DataLab and other tools should avoid ad hoc structures once mappings are defined.
- App exporters should target canonical names/metadata conventions.

## Definition of Done (Long-Term)
- Mature schema-backed dataset contract with robust validation.
- Efficient support for large runtime datasets.
- Shared conventions adopted by multiple apps and kits.
- Clear, documented bridge from serialized pack content to runtime data objects.
