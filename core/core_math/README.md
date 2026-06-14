# core_math

Shared numerical primitives for ecosystem modules.

## Scope (v1.0.1)
- 2D/3D vector arithmetic
- dot/cross products
- length/normalization helpers

## Dependencies
- `core_base`

## Current Contract
- owns dependency-light generic Vec2/Vec3 numeric primitives only
- current surface is exactly:
  - `vec2`: add, sub, scale, dot, length, normalize
  - `vec3`: add, sub, scale, dot, cross, length, normalize
- normalize helpers currently require:
  - non-null input and output pointers
  - finite vector length
  - length greater than `1e-20f`
- on normalize failure, the output vector is left unchanged

## Boundaries
- no matrix helpers
- no interpolation, statistics, random, or noise helpers
- no unit conversion (`core_units` owns that)
- no object-transform or scene-contract meaning (`core_object` / `core_scene` own that)
- no spatial placement or import-fit policy (`core_space` owns that)
- no viewport/camera transforms (`core_viewport2d` owns that)
- no renderer, solver, or app-local geometry semantics

## Status
- bootstrap implementation with boundary-condition tests
- current proving host remains `line_drawing` through local `math_util.h` wrappers
