#include "app/app_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail_count = 0;

static void expect_true(bool cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        fail_count += 1;
    }
}

static void expect_u32(uint32_t actual, uint32_t expected, const char *msg) {
    if (actual != expected) {
        fprintf(stderr, "FAIL: %s (actual=%u expected=%u)\n", msg, actual, expected);
        fail_count += 1;
    }
}

int main(void) {
    AppState app;
    memset(&app, 0, sizeof(app));

    app_worker_contract_init(&app);
    expect_u32(app.worker_state_bridge.world_generation, 1u, "init world generation");
    expect_u32(app.worker_state_bridge.tile_generation, 1u, "init tile generation");
    expect_u32(app.tile_state_bridge.tile_request_id, 1u, "init tile_request_id sync");
    expect_u32(app.worker_state_bridge.route_generation, 0u, "init route generation");
    expect_true(app_worker_contract_tile_request_is_current(&app, 1u), "current tile request accepted");
    expect_true(!app_worker_contract_tile_request_is_current(&app, 2u), "stale tile request rejected");

    app_worker_contract_bump_world_generation(&app);
    expect_u32(app.worker_state_bridge.world_generation, 2u, "world generation bump");

    app_worker_contract_bump_tile_generation(&app);
    expect_u32(app.worker_state_bridge.tile_generation, 2u, "tile generation bump");
    expect_u32(app.tile_state_bridge.tile_request_id, 2u, "tile generation sync");
    expect_true(app_worker_contract_tile_request_is_current(&app, 2u), "new tile generation accepted");
    expect_true(app_worker_contract_request_is_stale(1u, app.worker_state_bridge.tile_generation), "old tile generation stale");

    app.worker_state_bridge.tile_generation = UINT_MAX;
    app.tile_state_bridge.tile_request_id = UINT_MAX;
    app_worker_contract_bump_tile_generation(&app);
    expect_u32(app.worker_state_bridge.tile_generation, 1u, "tile generation wraps to 1");
    expect_u32(app.tile_state_bridge.tile_request_id, 1u, "wrapped tile generation sync");

    uint32_t req1 = app_worker_contract_next_route_request(&app);
    expect_u32(req1, 1u, "first route request generation");
    app_worker_contract_note_route_submitted(&app, req1);
    expect_true(app_worker_contract_route_result_is_current(&app, req1), "submitted route result accepted");

    app_worker_contract_note_route_applied(&app, req1);
    expect_true(app_worker_contract_route_result_is_current(&app, req1), "applied route result still current");

    uint32_t req2 = app_worker_contract_next_route_request(&app);
    expect_u32(req2, 2u, "second route request generation");
    app_worker_contract_note_route_submitted(&app, req2);
    expect_true(!app_worker_contract_route_result_is_current(&app, req1), "older route result rejected after new submit");
    expect_true(app_worker_contract_route_result_is_current(&app, req2), "latest route result accepted");

    app_worker_contract_reset_route_pipeline(&app);
    expect_u32(app.worker_state_bridge.route_generation, 0u, "route generation reset");
    expect_u32(app.worker_state_bridge.route_latest_requested_id, 0u, "route requested reset");
    expect_u32(app.worker_state_bridge.route_latest_submitted_id, 0u, "route submitted reset");
    expect_u32(app.worker_state_bridge.route_latest_applied_id, 0u, "route applied reset");

    if (fail_count > 0) {
        fprintf(stderr, "app_worker_contract_test: %d failure(s)\n", fail_count);
        return 1;
    }
    puts("app_worker_contract_test: success");
    return 0;
}
