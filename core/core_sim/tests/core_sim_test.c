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
    CoreSimLoopState state;
    CoreSimStepPolicy policy = test_policy();
    CoreSimPassOutcome outcome;
    CoreSimFrameOutcome frame_outcome;
    CoreSimFrameSummary summary;
    CoreSimArtifactRunHeader run_header;
    CoreSimFrameRecord frame_record;
    const char *reason_names[4];
    size_t reason_count;
    CoreSimStageMark marks[] = {
        { "frame_begin", 10.0 },
        { "after_events", 10.125 },
        { "after_update", 10.250 },
        { "after_submit", 10.500 },
    };
    CoreSimStageTiming timings[3];
    size_t timing_count = 0u;
    uint64_t empty_hash = core_sim_pass_order_hash(0);
    uint64_t invalid_hash = core_sim_pass_order_hash(&invalid_order);

    if (!expect_true(core_sim_loop_init(&state, &policy), "helper state init")) return 0;
    core_sim_pass_outcome_init(&outcome, 42u);
    frame_outcome = core_sim_frame_outcome_make_invalid(CORE_SIM_STATUS_PASS_FAILED,
                                                        "summary failure");
    frame_outcome.frame_index = 7u;
    frame_outcome.ticks_executed = 2u;
    frame_outcome.passes_executed = 5u;
    frame_outcome.reason_bits = CORE_SIM_FRAME_REASON_TICK_EXECUTED |
                                CORE_SIM_FRAME_REASON_RENDER_REQUESTED |
                                CORE_SIM_FRAME_REASON_PASS_FAILED;
    frame_outcome.failed_pass_id = 99u;
    frame_outcome.failed_pass_name = "failing_pass";
    frame_outcome.simulation_time_advanced_seconds = 0.5;
    frame_outcome.accumulator_remaining_seconds = 0.125;
    state.tick_index = 4u;
    state.simulation_time_seconds = 1.0;
    reason_count = core_sim_frame_reason_names(frame_outcome.reason_bits,
                                               reason_names,
                                               sizeof(reason_names) / sizeof(reason_names[0]));
    return expect_true(strcmp(core_sim_version(), CORE_SIM_VERSION_STRING) == 0,
                       "version string") &&
           expect_true(strcmp(core_sim_status_name(CORE_SIM_STATUS_OK), "ok") == 0,
                       "status name ok") &&
           expect_true(strcmp(core_sim_status_name(CORE_SIM_STATUS_PASS_FAILED), "pass_failed") == 0,
                       "status name pass failed") &&
           expect_true(strcmp(core_sim_frame_reason_name(CORE_SIM_FRAME_REASON_TICK_EXECUTED),
                              "tick_executed") == 0,
                       "reason name tick") &&
           expect_true(reason_count == 3u, "reason name count") &&
           expect_true(strcmp(reason_names[0], "tick_executed") == 0,
                       "reason name first") &&
           expect_true(strcmp(reason_names[2], "pass_failed") == 0,
                       "reason name third") &&
           expect_true(core_sim_pass_order_valid(0), "null order valid") &&
           expect_true(!core_sim_pass_order_valid(&invalid_order), "invalid order rejected") &&
           expect_true(empty_hash != 0u, "empty hash nonzero") &&
           expect_true(invalid_hash != empty_hash, "pass hash captures pass metadata") &&
           expect_true(core_sim_artifact_run_header_init(&run_header,
                                                         "test_program",
                                                         "test_host",
                                                         &state,
                                                         &invalid_order),
                       "artifact run header") &&
           expect_true(strcmp(run_header.schema_version, CORE_SIM_ARTIFACT_SCHEMA_VERSION) == 0,
                       "artifact schema version") &&
           expect_true(strcmp(run_header.core_sim_version, CORE_SIM_VERSION_STRING) == 0,
                       "artifact core sim version") &&
           expect_true(run_header.pass_order_hash == invalid_hash,
                       "artifact pass hash") &&
           expect_true(run_header.pass_count == 1u,
                       "artifact pass count") &&
           expect_true(outcome.status == CORE_SIM_STATUS_OK, "pass outcome status") &&
           expect_true(outcome.pass_id == 42u, "pass outcome id") &&
           expect_true(outcome.message == 0, "pass outcome message") &&
           expect_true(core_sim_frame_summary_from_outcome(&frame_outcome, &summary),
                       "summary computed") &&
           expect_true(strcmp(summary.status_name, "pass_failed") == 0,
                       "summary status") &&
           expect_true(summary.failed && summary.failed_pass_id == 99u,
                       "summary failure fields") &&
           expect_true(summary.reason_count == 3u, "summary reason count") &&
           expect_true(nearly_equal(summary.simulation_time_advanced_seconds, 0.5),
                       "summary sim time") &&
           expect_true(core_sim_frame_record_from_outcome(&frame_record,
                                                          &state,
                                                          &(CoreSimFrameRequest){ 0.75, 0, &invalid_order },
                                                          &frame_outcome),
                       "frame record") &&
           expect_true(strcmp(frame_record.schema_version, CORE_SIM_ARTIFACT_SCHEMA_VERSION) == 0,
                       "frame record schema version") &&
           expect_true(frame_record.frame_index == 7u && frame_record.tick_index_after == 4u,
                       "frame record indices") &&
           expect_true(nearly_equal(frame_record.input_dt_seconds, 0.75),
                       "frame record input dt") &&
           expect_true(nearly_equal(frame_record.simulation_time_after_seconds, 1.0),
                       "frame record sim time after") &&
           expect_true(frame_record.failed && frame_record.failed_pass_id == 99u,
                       "frame record failure fields") &&
           expect_true(core_sim_stage_timings_compute(marks,
                                                      sizeof(marks) / sizeof(marks[0]),
                                                      timings,
                                                      sizeof(timings) / sizeof(timings[0]),
                                                      &timing_count),
                       "stage timings computed") &&
           expect_true(timing_count == 3u, "stage timing count") &&
           expect_true(strcmp(timings[0].name, "after_events") == 0,
                       "stage timing name") &&
           expect_true(nearly_equal(timings[2].duration_seconds, 0.250),
                       "stage timing duration");
}

static int test_invalid_inputs_and_state_helpers(void) {
    CoreSimStepPolicy policy = test_policy();
    CoreSimLoopState state;
    CoreSimFrameOutcome outcome;
    CoreSimFrameSummary summary;
    CoreSimArtifactRunHeader header;
    CoreSimFrameRecord record;
    CoreSimStageTiming timings[2];
    size_t timing_count = 99u;
    const char *reason_names[2] = { "keep0", "keep1" };
    CoreSimStageMark descending_marks[] = {
        { "a", 2.0 },
        { "b", 1.0 },
    };
    CoreSimStageMark nonfinite_marks[] = {
        { "a", 0.0 },
        { "b", INFINITY },
    };
    CoreSimPassDescriptor valid_pass = { 7u, "ok", record_pass };
    CoreSimPassOrder valid_order = { &valid_pass, 1u };
    CoreSimFrameRequest valid_request = { 0.25, 0, &valid_order };

    if (!expect_true(!core_sim_step_policy_valid(0), "null policy invalid")) return 0;
    policy.fixed_dt_seconds = NAN;
    if (!expect_true(!core_sim_step_policy_valid(&policy), "nan dt invalid")) return 0;
    policy = test_policy();
    policy.fixed_dt_seconds = INFINITY;
    if (!expect_true(!core_sim_step_policy_valid(&policy), "infinite dt invalid")) return 0;
    policy = test_policy();
    policy.max_ticks_per_frame = 0u;
    if (!expect_true(!core_sim_step_policy_valid(&policy), "zero max ticks invalid")) return 0;

    if (!expect_true(!core_sim_loop_init(0, &policy), "null loop init")) return 0;
    policy = test_policy();
    policy.fixed_dt_seconds = NAN;
    if (!expect_true(!core_sim_loop_init(&state, &policy), "invalid loop init")) return 0;
    policy = test_policy();
    if (!expect_true(core_sim_loop_init(&state, &policy), "valid loop init")) return 0;

    state.accumulator_seconds = 1.0;
    state.single_step_requested = true;
    core_sim_loop_set_paused(&state, true);
    if (!expect_true(nearly_equal(state.accumulator_seconds, 0.0), "pause clears accumulator")) return 0;
    core_sim_loop_set_paused(&state, false);
    if (!expect_true(!state.single_step_requested, "unpause clears single-step request")) return 0;
    core_sim_loop_request_single_step(0);
    core_sim_loop_set_paused(0, true);
    core_sim_loop_reset(0);

    if (!expect_true(strcmp(core_sim_status_name((CoreSimStatus)99), "unknown") == 0,
                     "unknown status name")) return 0;
    if (!expect_true(strcmp(core_sim_frame_reason_name((CoreSimFrameReason)99), "unknown") == 0,
                     "unknown reason name")) return 0;
    if (!expect_true(core_sim_frame_reason_names(0x80000000u, reason_names, 2u) == 1u,
                     "unknown reason bit count")) return 0;
    if (!expect_true(strcmp(reason_names[0], "unknown") == 0, "unknown reason name stored")) return 0;

    if (!expect_true(!core_sim_artifact_run_header_init(0, "p", "h", &state, &valid_order),
                     "null artifact header")) return 0;
    if (!expect_true(!core_sim_artifact_run_header_init(&header, "p", "h", 0, &valid_order),
                     "null artifact state")) return 0;
    if (!expect_true(!core_sim_frame_summary_from_outcome(0, &summary), "null summary outcome")) return 0;
    if (!expect_true(!core_sim_frame_summary_from_outcome(&outcome, 0), "null summary out")) return 0;
    if (!expect_true(!core_sim_frame_record_from_outcome(0, &state, &valid_request, &outcome),
                     "null frame record")) return 0;
    if (!expect_true(!core_sim_frame_record_from_outcome(&record, 0, &valid_request, &outcome),
                     "null frame record state")) return 0;
    if (!expect_true(!core_sim_frame_record_from_outcome(&record, &state, 0, &outcome),
                     "null frame record request")) return 0;
    if (!expect_true(!core_sim_frame_record_from_outcome(&record, &state, &valid_request, 0),
                     "null frame record outcome")) return 0;

    if (!expect_true(!core_sim_stage_timings_compute(0, 2u, timings, 2u, &timing_count),
                     "null stage marks invalid")) return 0;
    if (!expect_true(timing_count == 0u, "stage count cleared on null marks")) return 0;
    timing_count = 99u;
    if (!expect_true(core_sim_stage_timings_compute(0, 0u, timings, 2u, &timing_count),
                     "empty stage marks valid")) return 0;
    if (!expect_true(timing_count == 0u, "empty stage count zero")) return 0;
    timing_count = 99u;
    if (!expect_true(!core_sim_stage_timings_compute(descending_marks, 2u, timings, 2u, &timing_count),
                     "descending marks invalid")) return 0;
    if (!expect_true(timing_count == 1u, "descending marks count reported")) return 0;
    timing_count = 99u;
    if (!expect_true(!core_sim_stage_timings_compute(nonfinite_marks, 2u, timings, 2u, &timing_count),
                     "non-finite marks invalid")) return 0;
    if (!expect_true(timing_count == 1u, "non-finite marks count reported")) return 0;
    timing_count = 99u;
    if (!expect_true(!core_sim_stage_timings_compute(descending_marks, 2u, 0, 0u, &timing_count),
                     "too-small timing buffer invalid")) return 0;
    if (!expect_true(timing_count == 1u, "too-small timing count reported")) return 0;

    outcome = core_sim_loop_advance(0, &valid_request);
    if (!expect_true(outcome.status == CORE_SIM_STATUS_INVALID_ARGUMENT, "null state advance")) return 0;
    outcome = core_sim_loop_advance(&state, 0);
    if (!expect_true(outcome.status == CORE_SIM_STATUS_INVALID_ARGUMENT, "null request advance")) return 0;

    state.policy = test_policy();
    outcome = core_sim_loop_advance(&state, &(CoreSimFrameRequest){ NAN, 0, &valid_order });
    if (!expect_true(outcome.status == CORE_SIM_STATUS_INVALID_ARGUMENT, "nan frame dt invalid")) return 0;
    outcome = core_sim_loop_advance(&state, &(CoreSimFrameRequest){ INFINITY, 0, &valid_order });
    if (!expect_true(outcome.status == CORE_SIM_STATUS_INVALID_ARGUMENT, "infinite frame dt invalid")) return 0;

    return 1;
}

int main(void) {
    if (!test_public_helpers()) return 1;
    if (!test_invalid_inputs_and_state_helpers()) return 1;
    if (!test_paused_executes_no_ticks()) return 1;
    if (!test_single_step_while_paused()) return 1;
    if (!test_active_fixed_step_count_and_remainder()) return 1;
    if (!test_max_tick_clamp_drops_excess()) return 1;
    if (!test_pass_order_and_failure()) return 1;

    puts("core_sim_test: ok");
    return 0;
}
