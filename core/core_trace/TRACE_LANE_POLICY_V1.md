# Trace Lane Policy v1

This policy keeps lane naming consistent across apps so DataLab and tooling can group traces predictably.

## Lane naming rules
- Use short snake_case names.
- Keep names under `CORE_TRACE_LANE_NAME_MAX`.
- Use stable names for equivalent signals across apps.

## Recommended baseline lanes
- `dt`: frame delta timing
- `event`: human-readable marker events
- `density_avg`: average scalar field signal for physics-style traces
- `sample_count`: renderer/sample-loop counters

## Marker label rules
- Keep labels concise and actionable.
- Use stable lifecycle labels where possible (`start`, `checkpoint`, `end`).
