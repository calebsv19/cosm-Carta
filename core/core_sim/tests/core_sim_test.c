#include "core_sim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct TestContext {
    uint32_t calls[16];
    uint32_t call_count;
    uint32_t fail_on_pass_id;
    double dt_total;
} TestContext;

static int nearly_equal(double a, double b) {
    return fabs(a - b) < 0.000000001;
}

static int expect_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static bool record_pass(void *user_context,
                        const CoreSimTickContext *tick,
                        CoreSimPassOutcome *outcome) {
    TestContext *ctx = (TestContext *)user_context;
    uint32_t pass_id = outcome ? outcome->pass_id : 0u;

    (void)tick;
    if (!ctx || !outcome) {
        return false;
    }
    if (ctx->call_count < (uint32_t)(sizeof(ctx->calls) / sizeof(ctx->calls[0]))) {
        ctx->calls[ctx->call_count++] = pass_id;
    }
    ctx->dt_total += tick->dt_seconds;
    if (ctx->fail_on_pass_id == pass_id) {
        outcome->status = CORE_SIM_STATUS_PASS_FAILED;
        outcome->message = "requested failure";
        return false;
    }
    return true;
}

static CoreSimStepPolicy test_policy(void) {
    CoreSimStepPolicy policy;
    core_sim_step_policy_defaults(&policy);
    policy.fixed_dt_seconds = 0.25;
    policy.max_ticks_per_frame = 4u;
    policy.drop_excess_accumulator_on_clamp = true;
    return policy;
}

static int test_paused_executes_no_ticks(void) {
    CoreSimLoopState state;
    CoreSimStepPolicy policy = test_policy();
    TestContext ctx;
    CoreSimPassDescriptor pass = { 10u, "tick", record_pass };
    CoreSimPassOrder order = { &pass, 1u };
    CoreSimFrameRequest request = { 1.0, &ctx, &order };
    CoreSimFrameOutcome outcome;

    memset(&ctx, 0, sizeof(ctx));
    if (!expect_true(core_sim_loop_init(&state, &policy), "init paused")) return 0;

    outcome = core_sim_loop_advance(&state, &request);
    return expect_true(outcome.status == CORE_SIM_STATUS_OK, "paused status") &&
           expect_true(outcome.reason_bits == CORE_SIM_FRAME_REASON_NONE, "paused reason bits") &&
           expect_true(outcome.ticks_executed == 0u, "paused tick count") &&
           expect_true(ctx.call_count == 0u, "paused pass count") &&
           expect_true(state.frame_index == 1u, "paused frame index");
}

static int test_single_step_while_paused(void) {
    CoreSimLoopState state;
    CoreSimStepPolicy policy = test_policy();
    TestContext ctx;
    CoreSimPassDescriptor pass = { 11u, "tick", record_pass };
    CoreSimPassOrder order = { &pass, 1u };
    CoreSimFrameRequest request = { 0.0, &ctx, &order };
    CoreSimFrameOutcome outcome;

    memset(&ctx, 0, sizeof(ctx));
    if (!expect_true(core_sim_loop_init(&state, &policy), "init single-step")) return 0;
    core_sim_loop_request_single_step(&state);

    outcome = core_sim_loop_advance(&state, &request);
    return expect_true(outcome.status == CORE_SIM_STATUS_OK, "single-step status") &&
           expect_true(outcome.ticks_executed == 1u, "single-step tick count") &&
           expect_true(outcome.single_step_consumed, "single-step consumed") &&
           expect_true((outcome.reason_bits & CORE_SIM_FRAME_REASON_TICK_EXECUTED) != 0u,
                       "single-step tick reason") &&
           expect_true((outcome.reason_bits & CORE_SIM_FRAME_REASON_SINGLE_STEP_CONSUMED) != 0u,
                       "single-step reason") &&
           expect_true(ctx.call_count == 1u, "single-step pass count") &&
           expect_true(ctx.calls[0] == 11u, "single-step pass id") &&
           expect_true(nearly_equal(state.simulation_time_seconds, 0.25), "single-step time");
}

static int test_active_fixed_step_count_and_remainder(void) {
    CoreSimLoopState state;
    CoreSimStepPolicy policy = test_policy();
    TestContext ctx;
    CoreSimPassDescriptor pass = { 12u, "tick", record_pass };
    CoreSimPassOrder order = { &pass, 1u };
    CoreSimFrameRequest request = { 0.60, &ctx, &order };
    CoreSimFrameOutcome outcome;

    memset(&ctx, 0, sizeof(ctx));
    if (!expect_true(core_sim_loop_init(&state, &policy), "init active")) return 0;
    core_sim_loop_set_paused(&state, false);

    outcome = core_sim_loop_advance(&state, &request);
    return expect_true(outcome.status == CORE_SIM_STATUS_OK, "active status") &&
           expect_true(outcome.ticks_executed == 2u, "active ticks") &&
           expect_true((outcome.reason_bits & CORE_SIM_FRAME_REASON_RENDER_REQUESTED) != 0u,
                       "active render reason") &&
           expect_true(ctx.call_count == 2u, "active pass calls") &&
           expect_true(nearly_equal(outcome.accumulator_remaining_seconds, 0.10), "active remainder") &&
           expect_true(nearly_equal(state.simulation_time_seconds, 0.50), "active sim time");
}

static int test_max_tick_clamp_drops_excess(void) {
    CoreSimLoopState state;
    CoreSimStepPolicy policy = test_policy();
    TestContext ctx;
    CoreSimPassDescriptor pass = { 13u, "tick", record_pass };
    CoreSimPassOrder order = { &pass, 1u };
    CoreSimFrameRequest request = { 5.0, &ctx, &order };
    CoreSimFrameOutcome outcome;

    memset(&ctx, 0, sizeof(ctx));
    if (!expect_true(core_sim_loop_init(&state, &policy), "init clamp")) return 0;
    core_sim_loop_set_paused(&state, false);

    outcome = core_sim_loop_advance(&state, &request);
    return expect_true(outcome.status == CORE_SIM_STATUS_OK, "clamp status") &&
           expect_true(outcome.ticks_executed == 4u, "clamp ticks") &&
           expect_true(outcome.max_tick_clamp_hit, "clamp hit") &&
           expect_true((outcome.reason_bits & CORE_SIM_FRAME_REASON_MAX_TICK_CLAMP_HIT) != 0u,
                       "clamp reason") &&
           expect_true(nearly_equal(outcome.accumulator_remaining_seconds, 0.0), "clamp remainder");
}

static int test_pass_order_and_failure(void) {
    CoreSimLoopState state;
    CoreSimStepPolicy policy = test_policy();
    TestContext ctx;
    CoreSimPassDescriptor passes[] = {
        { 1u, "first", record_pass },
        { 2u, "second", record_pass },
        { 3u, "third", record_pass },
    };
    CoreSimPassOrder order = { passes, 3u };
    CoreSimFrameRequest request = { 0.25, &ctx, &order };
    CoreSimFrameOutcome outcome;

    memset(&ctx, 0, sizeof(ctx));
    ctx.fail_on_pass_id = 2u;
    if (!expect_true(core_sim_loop_init(&state, &policy), "init failure")) return 0;
    core_sim_loop_set_paused(&state, false);

    outcome = core_sim_loop_advance(&state, &request);
    return expect_true(outcome.status == CORE_SIM_STATUS_PASS_FAILED, "failure status") &&
           expect_true(outcome.ticks_executed == 0u, "failure tick count") &&
           expect_true(outcome.passes_executed == 1u, "failure completed passes") &&
           expect_true(ctx.call_count == 2u, "failure attempted passes") &&
           expect_true(ctx.calls[0] == 1u && ctx.calls[1] == 2u, "failure order") &&
           expect_true(outcome.failed_pass_id == 2u, "failure pass id") &&
           expect_true((outcome.reason_bits & CORE_SIM_FRAME_REASON_PASS_FAILED) != 0u,
                       "failure reason") &&
           expect_true(outcome.failed_pass_name != 0 &&
                       strcmp(outcome.failed_pass_name, "second") == 0,
                       "failure pass name");
}

static int test_public_helpers(void) {
    CoreSimPassDescriptor invalid_pass = { 1u, "missing", 0 };
    CoreSimPassOrder invalid_order = { &invalid_pass, 1u };
    CoreSimPassOutcome outcome;

    core_sim_pass_outcome_init(&outcome, 42u);
    return expect_true(strcmp(core_sim_status_name(CORE_SIM_STATUS_OK), "ok") == 0,
                       "status name ok") &&
           expect_true(strcmp(core_sim_status_name(CORE_SIM_STATUS_PASS_FAILED), "pass_failed") == 0,
                       "status name pass failed") &&
           expect_true(core_sim_pass_order_valid(0), "null order valid") &&
           expect_true(!core_sim_pass_order_valid(&invalid_order), "invalid order rejected") &&
           expect_true(outcome.status == CORE_SIM_STATUS_OK, "pass outcome status") &&
           expect_true(outcome.pass_id == 42u, "pass outcome id") &&
           expect_true(outcome.message == 0, "pass outcome message");
}

int main(void) {
    if (!test_public_helpers()) return 1;
    if (!test_paused_executes_no_ticks()) return 1;
    if (!test_single_step_while_paused()) return 1;
    if (!test_active_fixed_step_count_and_remainder()) return 1;
    if (!test_max_tick_clamp_drops_excess()) return 1;
    if (!test_pass_order_and_failure()) return 1;

    puts("core_sim_test: ok");
    return 0;
}
