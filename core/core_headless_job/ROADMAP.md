# core_headless_job Roadmap

## Mission
Provide one shared semantic boundary for outer headless job bundles and shared
job reports while keeping inner scene payloads program-specific.

## Immediate Steps
1. Preserve the v0.1 bundle/report bridge for existing adopters.
2. Use the platform-v1 fixtures to implement one collapsed local coordinator
   and worker profile.
3. Prove the PhysicsSim-cache to RayTracing-render workflow through app-local
   adapters without changing either application algorithm.

## Hardened Current State
- Shared job/report schema-family defaults are centralized.
- Required identity/schema/path/output fields are validated consistently.
- Artifact validation is reusable across report emitters.
- Platform-v1 now owns canonical job/event/result/artifact/workflow/worker
  semantic records.
- State, event, outcome, transition, relative-path, UTC timestamp, SHA-256,
  topological-order, and capability invariants are standalone-test covered.
- Canonical JSON examples are checked without adding JSON ownership to core.
- The boundary stays intentionally narrow: no parser, scheduler, or worker
  dispatch behavior is included.

## Future Steps
1. Add app-local PhysicsSim and RayTracing adapters against the v1 contract.
2. Prove idempotent submit, durable attempts/events/claims/leases, restart
   recovery, and artifact lineage in the collapsed local runtime.
3. Add compatibility translators for current local/VPS payloads only after
   local proof.
4. Admit CosmOS only after its host-worker boundary freezes and a translation
   fixture proves identity/result/acknowledgement fencing.
5. Add shared serializer helpers only if multiple consumers would otherwise
   duplicate the same parser/writer behavior.
