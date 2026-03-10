# core_io Roadmap

## Mission
`core_io` is the canonical byte-movement layer for files and streams. It centralizes IO behavior so app code and higher-level modules do not duplicate fragile file handling.

## North Star End State
- One shared IO contract for:
  - read/write full file operations
  - streaming read/write adapters
  - buffer ownership and lifecycle
  - consistent IO error propagation
- Used by `core_pack`, import/export tools, and app pipelines.
- Predictable behavior across platforms and path variants.

## Current Surface
- Public API: `include/core_io.h`
- Implementation: `src/core_io.c`
- Tests: `tests/core_io_test.c`

## Product-Level Behavior Goals
- Reliable handling of partial reads/writes and short writes.
- Explicit ownership model for allocated buffers.
- Stable behavior for empty files, large files, and invalid paths.

## Capability Layers

### Layer 1: Baseline Reliability
- Harden `core_io_read_all` and `core_io_write_all` semantics.
- Expand tests around truncation, zero-byte payloads, and permission failures.
- Ensure all paths return precise `CoreResult` messages.

### Layer 2: Safe Write Semantics
- Add atomic write options (temp file + rename).
- Add optional flush/fsync policies for durability-sensitive outputs.
- Add file-existence and overwrite policies where needed.

### Layer 3: Streaming and Performance
- Add reusable buffered stream reader/writer utilities.
- Add chunked transfer APIs for large data handling.
- Optionally add mmap-backed read helpers behind portable guards.

### Layer 4: Filesystem Convenience APIs
- Add minimal path/file utility helpers needed by exporters/importers:
  - ensure parent directory exists
  - read file metadata (size/timestamps)
  - enumerate matching files (if needed by timeline tooling)

## Complexity Targets
- Keep abstractions tight and practical.
- Avoid introducing high-level serialization concerns (belong in `core_pack`/`core_data`).
- Prefer explicit APIs over hidden global state.

## Testing Strategy
- Unit tests for success and failure cases.
- Temp-directory integration tests for atomic writes and replacement semantics.
- Large-file tests and simulated short-read/short-write paths.

## API Stability Policy
- Existing baseline APIs remain stable.
- New APIs should be additive and clearly scoped.
- If behavior changes, document migration impact for modules using direct file IO today.

## Integration Expectations
- `core_pack` should continue to use `core_io` for utility reads.
- App exporters/importers should gradually route raw file operations through `core_io`.
- Future CLI tools should use this module for shared IO behavior.

## Definition of Done (Long-Term)
- Reliable baseline file APIs with robust tests.
- Optional atomic/durable write path available.
- Stream/chunk APIs sufficient for large dataset workflows.
- IO behavior unified across tools and apps.
