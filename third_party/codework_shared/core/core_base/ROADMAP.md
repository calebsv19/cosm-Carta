# core_base Roadmap

## Mission
`core_base` is the dependency-free foundation for every `core_*`, `kit_*`, and app module. It should remain small, stable, and portable while defining shared low-level behavior.

## North Star End State
- Single source of truth for:
  - memory allocation wrappers
  - result/error primitives
  - string/view helpers
  - hashing and deterministic IDs
  - path and platform helpers
  - endianness and byte-order checks
- Used by all other modules with no reverse dependency.
- No UI, renderer, file format, or app-domain logic.

## Current Surface
- Public API: `include/core_base.h`
- Implementation: `src/core_base.c`
- Tests: `tests/core_base_test.c`

## Product-Level Behavior Goals
- Deterministic outcomes for all pure helpers.
- Cross-platform behavior parity (macOS/Linux first).
- Stable error semantics that higher layers can propagate directly.
- Safety-first defaults for null/invalid arguments.

## Capability Layers

### Layer 1: Stable Primitive Contract
- Lock down `CoreResult` behavior and error code meaning.
- Keep allocation wrappers (`core_alloc`, `core_calloc`, `core_realloc`, `core_free`) stable.
- Guarantee helper behavior for edge inputs (empty strings, null pointers where allowed, zero lengths).

### Layer 2: Diagnostics and Debug Hooks
- Optional allocator hooks for tracking and leak diagnostics.
- Optional panic/assert policy hooks for test and debug builds.
- Debug-only tracing macros that compile out in release builds.

### Layer 3: Deterministic Utility Growth
- Additional stable helpers for:
  - bounded copy/parse routines
  - path normalization helpers
  - deterministic seed/hash utilities
- Keep every addition minimal and independent from IO and app concepts.

## Complexity Targets
- Must stay low complexity by design.
- New helpers require:
  - clear cross-module demand
  - platform behavior definition
  - tests for boundary conditions
- Avoid utility sprawl; reject helpers that belong in `core_io`, `core_data`, or `core_pack`.

## Testing Strategy
- Unit tests cover:
  - all exported functions
  - edge and failure paths
  - deterministic hash/path behavior
- Add sanitizer-target runs for memory wrappers.
- Add compatibility checks between architectures when possible.

## API Stability Policy
- `core_base.h` is considered high-stability API.
- Breaking changes require:
  - migration note
  - explicit downstream impact review
  - test updates in dependent modules

## Integration Expectations
- `core_io`, `core_data`, and `core_pack` rely on this module directly.
- Future `kit_*` modules should rely on `core_base` only via `core_*` unless a direct primitive is needed.

## Definition of Done (Long-Term)
- Stable primitive API with documented behavior and error semantics.
- Optional debug instrumentation path available.
- Full boundary-condition test coverage for exported helpers.
- No domain leakage from higher-level modules.
