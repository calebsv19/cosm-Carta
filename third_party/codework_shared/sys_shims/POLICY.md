# sys_shims Policy

## Purpose
Define strict behavior boundaries for system include shims.

## Must
- Keep shim adoption explicit and opt-in.
- Keep helper names prefixed (`shim_*`).
- Keep wrappers small, testable, and portable.
- Document behavior contracts for every added helper.

## Must Not
- Replace libc behavior globally.
- Add hidden side effects in shim helpers.
- Introduce hard dependency from core libraries onto `sys_shims`.
- Mix app-domain logic into shim headers.

## Ownership Boundary
- `sys_shims` handles compatibility/wrapper surfaces only.
- Semantics for data, pack formats, scene/space logic remain in `core_*` modules.

## Validation Policy
- Every new shim header addition requires at least one compile smoke test.
- Behavior-affecting helpers require parity tests against direct libc use.
- If behavior differs intentionally, it must be documented in the header and test notes.
