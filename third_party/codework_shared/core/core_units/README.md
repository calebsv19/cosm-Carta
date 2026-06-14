# core_units

Shared unit vocabulary and conversion primitives for cross-app scene/object workflows.

## Scope (bootstrap)
- Canonical unit identifiers and parser helpers
- Unit-to-unit scalar conversion
- World-scale conversion helpers for scene interchange

## Dependencies
- `core_base`

## Current Contract Notes
- Supported canonical unit kinds are `meter` / `m`, `centimeter` / `cm`, `millimeter` / `mm`, `inch` / `in`, and `foot` / `ft`.
- Parsing is case-insensitive for canonical singular names, backward-compatible plural legacy names, and symbols.
- Conversion is scalar and length-like only. Scene schema meaning, object geometry semantics, UI formatting/rounding, compiler dimension analysis, and runtime solver semantics remain host-owned.
- Negative scalar values are supported for conversion helpers, which keeps signed offsets and distances valid at host call sites.
- World scale must now be finite and greater than zero.
- Parse and conversion failure paths clear output values deterministically before returning an error.

## Status
- Compiler-aligned bootstrap implementation (`v0.2.0`) with singular canonical names, backward-compatible plural parse aliases, full unit vocabulary coverage, case-insensitive parse tests, negative scalar conversion coverage, and explicit finite world-scale validation.
