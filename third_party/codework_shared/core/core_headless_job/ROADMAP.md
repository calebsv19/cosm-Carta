# core_headless_job Roadmap

## Mission
Provide one shared semantic boundary for outer headless job bundles and shared
job reports while keeping inner scene payloads program-specific.

## Immediate Steps
1. Stabilize the shared typed outer-envelope/report structs.
2. Keep validation deterministic and JSON-free for early adopters.
3. Prove the boundary with `ray_tracing` and `physics_sim` before extending it
   to non-trio programs.

## Hardened Current State
- Shared job/report schema-family defaults are centralized.
- Required identity/schema/path/output fields are validated consistently.
- Artifact validation is reusable across report emitters.
- The boundary stays intentionally narrow: no parser, scheduler, or worker
  dispatch behavior is included.

## Future Steps
1. Add optional closed vocab helpers for report states/stages once two hosts
   agree on the lifecycle surface.
2. Add optional adapter helpers for current runner-status formats.
3. Add optional pack/JSON bridge helpers only after the semantic boundary is
   proven stable in more than one program.
