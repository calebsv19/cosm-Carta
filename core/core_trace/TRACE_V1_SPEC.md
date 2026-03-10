# Trace v1 Spec

Status: frozen v1 profile for `core_trace` packs.

## Purpose
Define a stable chunk contract for timeline samples and markers stored in `.pack`.

## Required chunks
- `TRHD`: trace header record

## Optional chunks
- `TRSM`: sample records (`CoreTraceSampleF32[]`)
- `TREV`: marker records (`CoreTraceMarker[]`)

## Header (`TRHD`)
`CoreTraceHeaderV1` fields:
- `trace_profile_version` (`u32`, must be `1`)
- `reserved` (`u32`, currently zero)
- `sample_count` (`u64`)
- `marker_count` (`u64`)

## Payload rules
- `TRSM` byte size must equal `sample_count * sizeof(CoreTraceSampleF32)`.
- `TREV` byte size must equal `marker_count * sizeof(CoreTraceMarker)`.
- lane/label strings are bounded by `core_trace.h` constants and stored in fixed buffers.

## Compatibility
- Unknown additional chunks are ignored by `core_trace_import_pack`.
- Breaking changes require a new trace profile version and migration plan.
