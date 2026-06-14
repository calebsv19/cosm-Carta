# core_authored_texture Roadmap

## Mission
Provide one shared semantic source of truth for authored-texture manifest meaning across `drawing_program` export and `ray_tracing` runtime ingest.

## Immediate Steps
1. Keep the module JSON-free and UI-free.
2. Freeze manifest vocabulary, primitive/binding/output enums, and face-role completeness rules.
3. Keep semantic-net layout/slot/orientation plus corner/edge/adjacency validation shared and strict.
4. Let apps adopt shared validation/constants incrementally before widening into any shared adapter layer.

## Future Steps
1. Lock the current bridge-first adoption with focused exporter/loader compatibility tests and docs.
2. Reassess whether small shared JSON adapter helpers are justified only after both apps converge on the semantic contract and subtree rollout is clean.
3. Keep image IO, runtime UX, and persistence policy app-local unless a later bounded lane proves clear cross-host value.
