# core_io

Unified IO abstractions for byte streams and files.

## Implemented Surface
- Whole-file binary reads via `core_io_read_all`
- Whole-file binary writes via `core_io_write_all`
- Atomic replace writes via `core_io_write_all_atomic`
- Owned buffer release via `core_io_buffer_free`
- Path existence checks via `core_io_path_exists`

## Planned Surface
- Stream reader/writer helpers on top of the existing callback typedefs
- Buffered and chunked IO helpers
- Additional safe-write options such as durability policies

## Current Limits
- `core_io_read_all` now reads incrementally and does not depend on
  `fseek` / `ftell` / `long` file-size measurement.
- `core_io_write_all_atomic` now declares the POSIX feature surface it relies
  on directly in the implementation so `mkstemp` / `fdopen` stay visible under
  strict C11 Linux package builds.
- The API is still a whole-file helper, so the practical limit remains bounded
  by available memory and `size_t` addressable buffer size.
- `core_io_write_all_atomic` is intentionally minimal: it writes to a temporary
  sibling path, closes the file, then renames it into place.
- The atomic helper does not yet add `fsync` durability, directory-sync, parent
  directory creation, or configurable overwrite policy.
- Parent-directory creation, overwrite policy, and flush/fsync durability are
  not part of the current API.

## Dependencies
- `core_base`

## Status
- Baseline whole-file implementation with unit tests for current behavior.
- Minimal atomic replace helper is implemented as an additive API.
- Stream and buffered IO remain planned, not implemented.
