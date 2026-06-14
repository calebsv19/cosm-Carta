# core_base

Foundational, UI-free utilities shared by all core and kit libraries.

## Implemented Surface
- Memory wrappers
- Borrowed string slices
- Error/result type
- Hashing
- Path helpers
- Endianness/platform helpers

## Dependencies
- None

## Status
- Stable primitive base layer with boundary-condition coverage.

## Current Contract
- `CoreStr` is a borrowed view; it does not allocate, own, or free storage.
- Zero-length `CoreStr` values compare equal even when their backing pointers differ.
- `core_path_basename` and `core_path_ext` return pointers into the original input string; callers do not own the returned storage.
- `core_path_ext` returns the last `.` in the basename when present, including dotfiles and empty extension suffixes such as `"file."`.
- `core_alloc(0)` and `core_calloc(..., 0)` return `NULL`.
- `core_realloc(ptr, 0)` frees `ptr` and returns `NULL`.
- `core_hash*_fnv1a(NULL, 0)` returns the normal FNV offset basis for the empty payload.
- `core_hash*_fnv1a(NULL, len > 0)` returns `0` as the stable invalid-input sentinel.

## Roadmap Only
- Owned strings, IDs, allocator diagnostics/hooks, and additional platform/path helpers remain roadmap work, not current public API.
