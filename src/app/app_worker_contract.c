#include "app/app_internal.h"

#include <stdint.h>

static uint32_t app_next_generation(uint32_t current) {
    uint32_t next = current + 1u;
    if (next == 0u) {
        next = 1u;
    }
    return next;
}

void app_worker_contract_init(AppState *app) {
    if (!app) {
        return;
    }
    app->worker_state_bridge.world_generation = 1u;
    app->worker_state_bridge.tile_generation = 1u;
    app->worker_state_bridge.route_generation = 0u;
    app->tile_state_bridge.tile_request_id = app->worker_state_bridge.tile_generation;
    app_worker_contract_reset_route_pipeline(app);
}

uint32_t app_worker_contract_bump_world_generation(AppState *app) {
    if (!app) {
        return 0u;
    }
    app->worker_state_bridge.world_generation = app_next_generation(app->worker_state_bridge.world_generation);
    return app->worker_state_bridge.world_generation;
}

uint32_t app_worker_contract_bump_tile_generation(AppState *app) {
    if (!app) {
        return 0u;
    }
    app->worker_state_bridge.tile_generation = app_next_generation(app->worker_state_bridge.tile_generation);
    app->tile_state_bridge.tile_request_id = app->worker_state_bridge.tile_generation;
    return app->worker_state_bridge.tile_generation;
}

uint32_t app_worker_contract_next_route_request(AppState *app) {
    if (!app) {
        return 0u;
    }
    app->worker_state_bridge.route_generation = app_next_generation(app->worker_state_bridge.route_generation);
    app->worker_state_bridge.route_latest_requested_id = app->worker_state_bridge.route_generation;
    return app->worker_state_bridge.route_generation;
}

void app_worker_contract_note_route_submitted(AppState *app, uint32_t request_id) {
    if (!app) {
        return;
    }
    app->worker_state_bridge.route_latest_submitted_id = request_id;
}

void app_worker_contract_note_route_applied(AppState *app, uint32_t request_id) {
    if (!app) {
        return;
    }
    app->worker_state_bridge.route_latest_applied_id = request_id;
}

void app_worker_contract_reset_route_pipeline(AppState *app) {
    if (!app) {
        return;
    }
    app->worker_state_bridge.route_generation = 0u;
    app->worker_state_bridge.route_latest_requested_id = 0u;
    app->worker_state_bridge.route_latest_submitted_id = 0u;
    app->worker_state_bridge.route_latest_applied_id = 0u;
}

bool app_worker_contract_request_is_stale(uint32_t request_id, uint32_t current_generation) {
    return request_id != current_generation;
}

bool app_worker_contract_tile_request_is_current(const AppState *app, uint32_t request_id) {
    if (!app) {
        return false;
    }
    return !app_worker_contract_request_is_stale(request_id, app->worker_state_bridge.tile_generation);
}

bool app_worker_contract_route_result_is_current(const AppState *app, uint32_t request_id) {
    if (!app) {
        return false;
    }
    if (request_id != app->worker_state_bridge.route_latest_submitted_id) {
        return false;
    }
    if (request_id < app->worker_state_bridge.route_latest_applied_id) {
        return false;
    }
    return true;
}

bool app_worker_contract_choose_evict_offset(const uint32_t *request_ids,
                                             uint32_t count,
                                             uint32_t current_generation,
                                             uint32_t *out_offset) {
    if (!request_ids || !out_offset || count == 0u) {
        return false;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (app_worker_contract_request_is_stale(request_ids[i], current_generation)) {
            *out_offset = i;
            return true;
        }
    }
    *out_offset = 0u;
    return true;
}
