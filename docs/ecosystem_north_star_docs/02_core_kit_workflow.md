
# Core & Kit Integration Workflow
## Purpose
This document defines **how new Core and Kit components are added, validated, and adopted** without destabilizing the ecosystem.

---

## Universal Rules
- Core libraries must not depend on UI or rendering.
- Kits may depend on core but must remain optional.
- Apps adopt new components incrementally.
- Every addition must be verifiable in isolation.

---

## Workflow: Adding a New Core Library

### Step 1 — Specification
- Define scope and responsibilities
- List explicit dependencies
- Define public API surface
- Define versioning strategy

### Step 2 — Standalone Validation
- Build and test independently
- Unit tests for all public APIs
- No app integration yet

### Step 3 — Tooling Support
- Add CLI or test harness if applicable
- Validate real-world usage scenarios

### Step 4 — Export-Only Adoption
- Add minimal integration to **one** app
- No UI changes
- Validate correctness and performance

### Step 5 — Documentation
- Update core documentation
- Add usage examples
- Define compatibility guarantees

---

## Workflow: Adding a New Kit Library

### Step 1 — Minimal Feature Definition
- Define the smallest useful subset
- Avoid framework creep

### Step 2 — Core Dependency Audit
- Ensure kit only depends on core + rendering/UI as needed

### Step 3 — Isolated Demo
- Build a tiny demo app using the kit
- No dependency on major apps

### Step 4 — First App Integration
- Embed in exactly one application
- Validate usability and performance

### Step 5 — Optional Adoption
- Other apps may adopt at will
- No forced migration

---

## Validation Matrix

| Component Type | Validated By |
|---------------|------------|
| core_data | Unit tests + DataLab |
| core_pack | CLI dump tool + DataLab |
| kit_viz | Demo app + embedded panel |
| kit_ui | One app migration |
| core_trace | Replay determinism test |

---

## Failure Conditions (Stop Work If These Occur)
- Core library requires UI code
- Kit becomes mandatory dependency
- App cannot build standalone
- Format changes without version bump

---

## Principle
**Every new capability must first work alone, then serve one app, then earn the right to serve all.**
