#include "core_sim.h"

enum {
    CORE_SIM_DEFAULT_MAX_TICKS_PER_FRAME = 8
};

static CoreSimFrameOutcome core_sim_frame_outcome_init(const CoreSimLoopState *state) {
    CoreSimFrameOutcome outcome;

    outcome.status = CORE_SIM_STATUS_OK;
    outcome.frame_index = state ? state->frame_index : 0u;
    outcome.ticks_executed = 0u;
    outcome.passes_executed = 0u;
    outcome.reason_bits = CORE_SIM_FRAME_REASON_NONE;
    outcome.render_requested = false;
    outcome.max_tick_clamp_hit = false;
    outcome.single_step_consumed = false;
    outcome.simulation_time_advanced_seconds = 0.0;
    outcome.accumulator_remaining_seconds = state ? state->accumulator_seconds : 0.0;
    outcome.failed_pass_id = 0u;
    outcome.failed_pass_name = 0;
    outcome.message = 0;
    return outcome;
}

const char *core_sim_status_name(CoreSimStatus status) {
    switch (status) {
        case CORE_SIM_STATUS_OK:
            return "ok";
        case CORE_SIM_STATUS_INVALID_ARGUMENT:
            return "invalid_argument";
        case CORE_SIM_STATUS_INVALID_POLICY:
            return "invalid_policy";
        case CORE_SIM_STATUS_PASS_FAILED:
            return "pass_failed";
        default:
            return "unknown";
    }
}

void core_sim_step_policy_defaults(CoreSimStepPolicy *policy) {
    if (!policy) {
        return;
    }

    policy->fixed_dt_seconds = 1.0 / 60.0;
    policy->max_ticks_per_frame = CORE_SIM_DEFAULT_MAX_TICKS_PER_FRAME;
    policy->drop_excess_accumulator_on_clamp = true;
}

bool core_sim_step_policy_valid(const CoreSimStepPolicy *policy) {
    if (!policy) {
        return false;
    }
    if (policy->fixed_dt_seconds <= 0.0) {
        return false;
    }
    if (policy->max_ticks_per_frame == 0u) {
        return false;
    }
    return true;
}

bool core_sim_loop_init(CoreSimLoopState *state, const CoreSimStepPolicy *policy) {
    CoreSimStepPolicy resolved;

    if (!state) {
        return false;
    }

    if (policy) {
        resolved = *policy;
    } else {
        core_sim_step_policy_defaults(&resolved);
    }

    if (!core_sim_step_policy_valid(&resolved)) {
        return false;
    }

    state->policy = resolved;
    core_sim_loop_reset(state);
    return true;
}

void core_sim_loop_reset(CoreSimLoopState *state) {
    if (!state) {
        return;
    }

    state->accumulator_seconds = 0.0;
    state->simulation_time_seconds = 0.0;
    state->frame_index = 0u;
    state->tick_index = 0u;
    state->paused = true;
    state->single_step_requested = false;
}

void core_sim_loop_set_paused(CoreSimLoopState *state, bool paused) {
    if (!state) {
        return;
    }

    state->paused = paused;
    if (!paused) {
        state->single_step_requested = false;
    } else {
        state->accumulator_seconds = 0.0;
    }
}

void core_sim_loop_request_single_step(CoreSimLoopState *state) {
    if (!state) {
        return;
    }

    state->single_step_requested = true;
}

CoreSimFrameOutcome core_sim_frame_outcome_make_invalid(CoreSimStatus status,
                                                        const char *message) {
    CoreSimFrameOutcome outcome;

    outcome.status = status;
    outcome.frame_index = 0u;
    outcome.ticks_executed = 0u;
    outcome.passes_executed = 0u;
    outcome.reason_bits = CORE_SIM_FRAME_REASON_NONE;
    outcome.render_requested = false;
    outcome.max_tick_clamp_hit = false;
    outcome.single_step_consumed = false;
    outcome.simulation_time_advanced_seconds = 0.0;
    outcome.accumulator_remaining_seconds = 0.0;
    outcome.failed_pass_id = 0u;
    outcome.failed_pass_name = 0;
    outcome.message = message;
    return outcome;
}

void core_sim_pass_outcome_init(CoreSimPassOutcome *outcome, uint32_t pass_id) {
    if (!outcome) {
        return;
    }

    outcome->status = CORE_SIM_STATUS_OK;
    outcome->pass_id = pass_id;
    outcome->message = 0;
}

bool core_sim_pass_order_valid(const CoreSimPassOrder *pass_order) {
    size_t i;

    if (!pass_order || pass_order->pass_count == 0u) {
        return true;
    }
    if (!pass_order->passes) {
        return false;
    }
    for (i = 0u; i < pass_order->pass_count; ++i) {
        if (!pass_order->passes[i].run) {
            return false;
        }
    }
    return true;
}

static bool core_sim_run_tick(CoreSimLoopState *state,
                              const CoreSimFrameRequest *request,
                              CoreSimFrameOutcome *outcome,
                              uint32_t tick_index_in_frame) {
    CoreSimTickContext tick;
    const CoreSimPassOrder *pass_order;
    size_t i;

    if (!state || !request || !outcome) {
        return false;
    }

    pass_order = request->pass_order;
    tick.dt_seconds = state->policy.fixed_dt_seconds;
    tick.simulation_time_seconds = state->simulation_time_seconds;
    tick.frame_index = state->frame_index;
    tick.tick_index = state->tick_index;
    tick.tick_index_in_frame = tick_index_in_frame;

    if (pass_order && pass_order->passes && pass_order->pass_count > 0u) {
        for (i = 0u; i < pass_order->pass_count; ++i) {
            CoreSimPassOutcome pass_outcome;
            const CoreSimPassDescriptor *pass = &pass_order->passes[i];

            core_sim_pass_outcome_init(&pass_outcome, pass->pass_id);
            if (!pass->run(request->user_context, &tick, &pass_outcome)) {
                outcome->status = pass_outcome.status != CORE_SIM_STATUS_OK
                                      ? pass_outcome.status
                                      : CORE_SIM_STATUS_PASS_FAILED;
                outcome->failed_pass_id = pass->pass_id;
                outcome->failed_pass_name = pass->name;
                outcome->message = pass_outcome.message;
                outcome->accumulator_remaining_seconds = state->accumulator_seconds;
                outcome->reason_bits |= CORE_SIM_FRAME_REASON_PASS_FAILED;
                return false;
            }
            outcome->passes_executed += 1u;
        }
    }

    state->tick_index += 1u;
    state->simulation_time_seconds += state->policy.fixed_dt_seconds;
    outcome->ticks_executed += 1u;
    outcome->simulation_time_advanced_seconds += state->policy.fixed_dt_seconds;
    outcome->render_requested = true;
    outcome->reason_bits |= CORE_SIM_FRAME_REASON_TICK_EXECUTED;
    outcome->reason_bits |= CORE_SIM_FRAME_REASON_RENDER_REQUESTED;
    return true;
}

CoreSimFrameOutcome core_sim_loop_advance(CoreSimLoopState *state,
                                          const CoreSimFrameRequest *request) {
    CoreSimFrameOutcome outcome;

    if (!state || !request) {
        return core_sim_frame_outcome_make_invalid(CORE_SIM_STATUS_INVALID_ARGUMENT,
                                                   "missing state or request");
    }
    if (!core_sim_step_policy_valid(&state->policy)) {
        return core_sim_frame_outcome_make_invalid(CORE_SIM_STATUS_INVALID_POLICY,
                                                   "invalid step policy");
    }
    if (!core_sim_pass_order_valid(request->pass_order)) {
        return core_sim_frame_outcome_make_invalid(CORE_SIM_STATUS_INVALID_ARGUMENT,
                                                   "invalid pass order");
    }

    outcome = core_sim_frame_outcome_init(state);

    if (request->frame_dt_seconds < 0.0) {
        outcome.status = CORE_SIM_STATUS_INVALID_ARGUMENT;
        outcome.message = "negative frame dt";
        return outcome;
    }

    if (state->paused) {
        state->accumulator_seconds = 0.0;
        if (state->single_step_requested) {
            if (!core_sim_run_tick(state, request, &outcome, 0u)) {
                state->single_step_requested = false;
                state->frame_index += 1u;
                return outcome;
            }
            outcome.single_step_consumed = true;
            outcome.reason_bits |= CORE_SIM_FRAME_REASON_SINGLE_STEP_CONSUMED;
            state->single_step_requested = false;
        }
        outcome.accumulator_remaining_seconds = state->accumulator_seconds;
        state->frame_index += 1u;
        return outcome;
    }

    state->accumulator_seconds += request->frame_dt_seconds;
    while (state->accumulator_seconds >= state->policy.fixed_dt_seconds) {
        if (outcome.ticks_executed >= state->policy.max_ticks_per_frame) {
            outcome.max_tick_clamp_hit = true;
            outcome.reason_bits |= CORE_SIM_FRAME_REASON_MAX_TICK_CLAMP_HIT;
            if (state->policy.drop_excess_accumulator_on_clamp) {
                state->accumulator_seconds = 0.0;
            }
            break;
        }

        state->accumulator_seconds -= state->policy.fixed_dt_seconds;
        if (!core_sim_run_tick(state, request, &outcome, outcome.ticks_executed)) {
            state->frame_index += 1u;
            return outcome;
        }
    }

    outcome.accumulator_remaining_seconds = state->accumulator_seconds;
    state->frame_index += 1u;
    return outcome;
}
