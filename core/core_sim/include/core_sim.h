#ifndef CORE_SIM_H
#define CORE_SIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum CoreSimStatus {
    CORE_SIM_STATUS_OK = 0,
    CORE_SIM_STATUS_INVALID_ARGUMENT = 1,
    CORE_SIM_STATUS_INVALID_POLICY = 2,
    CORE_SIM_STATUS_PASS_FAILED = 3
} CoreSimStatus;

typedef enum CoreSimFrameReason {
    CORE_SIM_FRAME_REASON_NONE = 0u,
    CORE_SIM_FRAME_REASON_TICK_EXECUTED = 1u << 0,
    CORE_SIM_FRAME_REASON_RENDER_REQUESTED = 1u << 1,
    CORE_SIM_FRAME_REASON_MAX_TICK_CLAMP_HIT = 1u << 2,
    CORE_SIM_FRAME_REASON_SINGLE_STEP_CONSUMED = 1u << 3,
    CORE_SIM_FRAME_REASON_PASS_FAILED = 1u << 4
} CoreSimFrameReason;

typedef struct CoreSimStepPolicy {
    double fixed_dt_seconds;
    uint32_t max_ticks_per_frame;
    bool drop_excess_accumulator_on_clamp;
} CoreSimStepPolicy;

typedef struct CoreSimLoopState {
    CoreSimStepPolicy policy;
    double accumulator_seconds;
    double simulation_time_seconds;
    uint64_t frame_index;
    uint64_t tick_index;
    bool paused;
    bool single_step_requested;
} CoreSimLoopState;

typedef struct CoreSimTickContext {
    double dt_seconds;
    double simulation_time_seconds;
    uint64_t frame_index;
    uint64_t tick_index;
    uint32_t tick_index_in_frame;
} CoreSimTickContext;

typedef struct CoreSimPassOutcome {
    CoreSimStatus status;
    uint32_t pass_id;
    const char *message;
} CoreSimPassOutcome;

typedef bool (*CoreSimPassFn)(void *user_context,
                              const CoreSimTickContext *tick,
                              CoreSimPassOutcome *outcome);

typedef struct CoreSimPassDescriptor {
    uint32_t pass_id;
    const char *name;
    CoreSimPassFn run;
} CoreSimPassDescriptor;

typedef struct CoreSimPassOrder {
    const CoreSimPassDescriptor *passes;
    size_t pass_count;
} CoreSimPassOrder;

typedef struct CoreSimFrameRequest {
    double frame_dt_seconds;
    void *user_context;
    const CoreSimPassOrder *pass_order;
} CoreSimFrameRequest;

typedef struct CoreSimFrameOutcome {
    CoreSimStatus status;
    uint64_t frame_index;
    uint32_t ticks_executed;
    uint32_t passes_executed;
    uint32_t reason_bits;
    bool render_requested;
    bool max_tick_clamp_hit;
    bool single_step_consumed;
    double simulation_time_advanced_seconds;
    double accumulator_remaining_seconds;
    uint32_t failed_pass_id;
    const char *failed_pass_name;
    const char *message;
} CoreSimFrameOutcome;

const char *core_sim_status_name(CoreSimStatus status);

void core_sim_step_policy_defaults(CoreSimStepPolicy *policy);
bool core_sim_step_policy_valid(const CoreSimStepPolicy *policy);
bool core_sim_pass_order_valid(const CoreSimPassOrder *pass_order);

bool core_sim_loop_init(CoreSimLoopState *state, const CoreSimStepPolicy *policy);
void core_sim_loop_reset(CoreSimLoopState *state);

void core_sim_loop_set_paused(CoreSimLoopState *state, bool paused);
void core_sim_loop_request_single_step(CoreSimLoopState *state);

void core_sim_pass_outcome_init(CoreSimPassOutcome *outcome, uint32_t pass_id);
CoreSimFrameOutcome core_sim_frame_outcome_make_invalid(CoreSimStatus status,
                                                        const char *message);
CoreSimFrameOutcome core_sim_loop_advance(CoreSimLoopState *state,
                                          const CoreSimFrameRequest *request);

#endif
