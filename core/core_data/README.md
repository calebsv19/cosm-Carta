# core_data

Canonical in-memory data representation shared by apps.

## Implemented Surface
- Scalar values (f64)
- 1D float arrays
- 2D scalar and vector fields
- Typed metadata dictionary (string/f64/i64/bool)
- Float table container (named columns)
- Typed table container (f32/f64/i64/u32/bool columns)
- Dataset container

## Dependencies
- `core_base`

## Status
- Baseline copied-dataset carrier with contract-hardening tests.

## Current Contract
- All dataset additions copy caller-owned input into `CoreDataset` storage.
- Metadata keys must be non-empty and unique within a dataset.
- Item names must be non-empty and unique within a dataset.
- Zero-count float arrays are allowed and store `count == 0` with `values == NULL`.
- Zero-row float and typed tables are allowed when column definitions are valid.
- 2D field dimensions must both be non-zero.
- Dimension and allocation math is guarded against `size_t` overflow before allocation.

## Current Limits
- Whole dataset growth is still bounded by available contiguous memory.
- Table column names are validated, but schema descriptors and semantic validation are not implemented yet.
- `core_data` owns in-memory copied containers only; persistence and interchange remain outside this module.
