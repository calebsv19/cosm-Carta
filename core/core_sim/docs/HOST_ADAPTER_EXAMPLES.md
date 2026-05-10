# core_sim Host Adapter Examples

Status: reference examples for `core_sim v0.3.0`.

These examples show the four proven host shapes. They are deliberately small:
`core_sim` owns cadence, ordered pass execution, and frame outcomes; apps own
domain state and policy.

## 1. Fixed-Step Focused Process

Reference host: `gravity_orbit_sim`.

Use when one focused model advances through one domain step per tick.

```c
static bool orbit_tick(void *user_context,
                       const CoreSimTickContext *tick,
                       CoreSimPassOutcome *outcome) {
    OrbitWorld *world = (OrbitWorld *)user_context;
    (void)tick;
    if (!world) {
        outcome->status = CORE_SIM_STATUS_INVALID_ARGUMENT;
        outcome->message = "missing world";
        return false;
    }
    orbit_world_step(world);
    return true;
}

CoreSimPassDescriptor pass = { 1u, "orbit_world_step", orbit_tick };
CoreSimPassOrder order = { &pass, 1u };
CoreSimFrameRequest request = { frame_dt_seconds, &world, &order };
CoreSimFrameOutcome outcome = core_sim_loop_advance(&runtime->sim_loop, &request);
```

Keep local: world schema, equations, preset/scenario persistence, input, render,
and UI policy.

## 2. Entity/Group Pass Order

Reference host: `behavior_sim`.

Use when many entities are updated by ordered systems.

```c
static bool run_behavior_pass(void *user_context,
                              const CoreSimTickContext *tick,
                              CoreSimPassOutcome *outcome) {
    BehaviorPassContext *ctx = (BehaviorPassContext *)user_context;
    BehaviorPassId pass_id = (BehaviorPassId)outcome->pass_id;
    (void)tick;
    if (!ctx || behavior_system_run(pass_id, ctx) != 0) {
        outcome->status = CORE_SIM_STATUS_PASS_FAILED;
        outcome->message = behavior_pass_name(pass_id);
        return false;
    }
    return true;
}
```

Build the pass descriptor array from the app's canonical system order. Keep
entity storage, behavior meaning, metrics, editor state, and UI local.

## 3. Solver/Substep Pass Network

Reference host: `physics_sim`.

Use when one frame must execute an exact number of solver substeps.

```c
CoreSimStepPolicy policy;
policy.fixed_dt_seconds = frame_dt_seconds / (double)substep_count;
policy.max_ticks_per_frame = (uint32_t)substep_count;
policy.drop_excess_accumulator_on_clamp = true;

scene->runtime_loop.policy = policy;
core_sim_loop_set_paused(&scene->runtime_loop, false);

CoreSimFrameRequest request = {
    policy.fixed_dt_seconds * (double)substep_count,
    &scene_step_context,
    &scene_pass_order
};
CoreSimFrameOutcome outcome = core_sim_loop_advance(&scene->runtime_loop, &request);
```

Keep local: numerical methods, backend selection, emitters, obstacles, objects,
mode hooks, scene time semantics, HUD payloads, and renderer submission.

## 4. Progressive/Render Frame

Reference host: `ray_tracing`.

Use when the top-level loop is a render-progress frame, not a physics-only tick.

```c
static const CoreSimPassDescriptor passes[] = {
    { 1u, "events", run_events },
    { 2u, "update", run_update },
    { 3u, "route", run_route },
    { 4u, "submit", run_submit },
    { 5u, "loop_conditions", run_loop_conditions },
};

CoreSimPassOrder order = { passes, sizeof(passes) / sizeof(passes[0]) };
CoreSimFrameRequest request = { loop->policy.fixed_dt_seconds, &frame_ctx, &order };
CoreSimFrameOutcome outcome = core_sim_loop_advance(loop, &request);
```

Keep local: render accumulation, camera/path/light state, scene routing,
window/frame delay policy, renderer calls, exports, and cancellation policy.

## Adapter Rules

- Initialize one persistent `CoreSimLoopState` per simulation loop or nested
  solver shell.
- Keep pass names stable; diagnostics and future trace adapters use them.
- Map `CoreSimFrameOutcome` back into local redraw, mode, diagnostics, and
  error state.
- Do not mutate authoritative simulation state in render-derive or submit
  passes unless the app intentionally models rendering as the simulation.
- Do not promote app domain vocabulary into `core_sim`; keep it in the adapter.
