#include "app/app_internal.h"

#include "core/time.h"
#include "map/mercator.h"
#include "map/polygon_cache.h"

#include <stdlib.h>
#include <string.h>

static uint32_t app_budget_env_u32(const char *name,
                                   uint32_t fallback,
                                   uint32_t min_value,
                                   uint32_t max_value) {
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value) {
        return fallback;
    }
    if (parsed < (unsigned long)min_value) {
        return min_value;
    }
    if (parsed > (unsigned long)max_value) {
        return max_value;
    }
    return (uint32_t)parsed;
}

static double app_budget_env_f64(const char *name,
                                 double fallback,
                                 double min_value,
                                 double max_value) {
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    char *end = NULL;
    double parsed = strtod(value, &end);
    if (end == value) {
        return fallback;
    }
    if (parsed < min_value) {
        return min_value;
    }
    if (parsed > max_value) {
        return max_value;
    }
    return parsed;
}

void app_runtime_budget_policy_init(AppState *app) {
    if (!app) {
        return;
    }

    AppRuntimeBudgetPolicy policy = {0};
    policy.load_road_cap = app_budget_env_u32("MAPFORGE_BUDGET_LOAD_ROAD_CAP", 8u, 1u, 128u);
    policy.load_polygon_cap_small_view = app_budget_env_u32("MAPFORGE_BUDGET_LOAD_POLY_CAP_SMALL", 4u, 1u, 64u);
    policy.load_polygon_cap_medium_view = app_budget_env_u32("MAPFORGE_BUDGET_LOAD_POLY_CAP_MEDIUM", 3u, 1u, 64u);
    policy.load_polygon_cap_large_view = app_budget_env_u32("MAPFORGE_BUDGET_LOAD_POLY_CAP_LARGE", 2u, 1u, 64u);
    policy.load_polygon_building_bonus = app_budget_env_u32("MAPFORGE_BUDGET_LOAD_POLY_BUILDING_BONUS", 1u, 0u, 8u);
    policy.lane_l2_cap_low_budget = app_budget_env_u32("MAPFORGE_BUDGET_LANE_L2_LOW", 2u, 0u, 64u);
    policy.lane_l2_cap_high_budget = app_budget_env_u32("MAPFORGE_BUDGET_LANE_L2_HIGH", 3u, 0u, 64u);
    policy.lane_l3_cap_low_budget = app_budget_env_u32("MAPFORGE_BUDGET_LANE_L3_LOW", 0u, 0u, 64u);
    policy.lane_l3_cap_high_budget = app_budget_env_u32("MAPFORGE_BUDGET_LANE_L3_HIGH", 1u, 0u, 64u);
    policy.integrate_fallback_budget = app_budget_env_u32("MAPFORGE_BUDGET_INTEGRATE_FALLBACK", APP_TILE_INTEGRATE_BUDGET, 1u, 256u);
    policy.integrate_cap = app_budget_env_u32("MAPFORGE_BUDGET_INTEGRATE_CAP", 64u, 1u, 256u);
    policy.vk_poly_prep_integrate_budget = app_budget_env_u32("MAPFORGE_BUDGET_VK_POLY_PREP_INTEGRATE", APP_VK_POLY_PREP_INTEGRATE_BUDGET, 1u, 64u);
    policy.vk_poly_prep_integrate_time_slice_sec = app_budget_env_f64("MAPFORGE_BUDGET_VK_POLY_PREP_SLICE_SEC", APP_VK_POLY_PREP_INTEGRATE_TIME_SLICE_SEC, 0.0001, 0.0500);
    policy.vk_asset_build_budget = app_budget_env_u32("MAPFORGE_BUDGET_VK_ASSET_BUILD", APP_VK_ASSET_BUILD_BUDGET, 1u, 256u);
    policy.vk_asset_build_time_slice_sec = app_budget_env_f64("MAPFORGE_BUDGET_VK_ASSET_SLICE_SEC", APP_VK_ASSET_BUILD_TIME_SLICE_SEC, 0.0001, 0.0500);
    policy.vk_poly_asset_cap_small_view = app_budget_env_u32("MAPFORGE_BUDGET_VK_POLY_ASSET_CAP_SMALL", 2u, 0u, 64u);
    policy.vk_poly_asset_cap_default = app_budget_env_u32("MAPFORGE_BUDGET_VK_POLY_ASSET_CAP_DEFAULT", 1u, 0u, 64u);
    app->tile_state_bridge.budget_policy = policy;
}

void app_runtime_budget_reset_frame(AppState *app) {
    if (!app) {
        return;
    }
    memset(&app->tile_state_bridge.budget_frame, 0, sizeof(app->tile_state_bridge.budget_frame));
}

uint32_t app_tile_load_budget(TileLayerKind kind, uint32_t expected) {
    bool polygon_layer = (kind == TILE_LAYER_POLY_WATER ||
                          kind == TILE_LAYER_POLY_PARK ||
                          kind == TILE_LAYER_POLY_LANDUSE ||
                          kind == TILE_LAYER_POLY_BUILDING);
    if (polygon_layer) {
        if (expected <= 8u) {
            return expected;
        }
        if (kind == TILE_LAYER_POLY_BUILDING) {
            if (expected <= 32u) {
                return 4u;
            }
            if (expected <= 128u) {
                return 6u;
            }
            return 8u;
        }
        if (expected <= 32u) {
            return 3u;
        }
        if (expected <= 128u) {
            return 4u;
        }
        return 6u;
    }
    if (expected <= 8) {
        return expected;
    }
    if (expected <= 32) {
        return 16;
    }
    if (expected <= 128) {
        return 32;
    }
    if (expected <= 256) {
        return 48;
    }
    return 64;
}

uint32_t app_tile_integrate_budget(TileLayerKind kind, uint32_t expected) {
    bool polygon_layer = (kind == TILE_LAYER_POLY_WATER ||
                          kind == TILE_LAYER_POLY_PARK ||
                          kind == TILE_LAYER_POLY_LANDUSE ||
                          kind == TILE_LAYER_POLY_BUILDING);
    if (polygon_layer) {
        if (expected <= 8u) {
            return expected;
        }
        if (kind == TILE_LAYER_POLY_BUILDING) {
            if (expected <= 64u) {
                return 4u;
            }
            return 6u;
        }
        if (expected <= 32u) {
            return 3u;
        }
        return 4u;
    }
    if (expected <= 32) {
        return 32;
    }
    if (expected <= 128) {
        return 48;
    }
    return 64;
}

float app_layer_zoom_start(const AppState *app, TileLayerKind kind) {
    return layer_policy_zoom_start(kind, app ? app->view_state_bridge.building_zoom_bias : 0.0f);
}

bool app_layer_runtime_enabled(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return false;
    }
    if (kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    const LayerPolicy *policy = layer_policy_for(kind);
    if (policy && !policy->enabled) {
        if (!(kind == TILE_LAYER_CONTOUR && app->tile_state_bridge.contour_runtime_enabled)) {
            return false;
        }
    }
    return app->view_state_bridge.layer_user_enabled[kind];
}

float app_layer_fade_multiplier(const AppState *app, TileLayerKind kind) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return 1.0f;
    }
    if (!app->view_state_bridge.zoom_logic_enabled) {
        return 1.0f;
    }
    float start_zoom = ((float)app->view_state_bridge.layer_fade_start_milli[kind] / 1000.0f) * 20.0f;
    float span_zoom = 0.15f + ((float)app->view_state_bridge.layer_fade_speed_milli[kind] / 1000.0f) * 6.0f;
    float z = app->view_state_bridge.camera.zoom;
    if (z >= start_zoom) {
        return 1.0f;
    }
    if (z <= start_zoom - span_zoom) {
        return 0.0f;
    }
    return app_clampf((z - (start_zoom - span_zoom)) / span_zoom, 0.0f, 1.0f);
}

bool app_layer_active_runtime(const AppState *app, TileLayerKind kind) {
    if (!app_layer_runtime_enabled(app, kind)) {
        return false;
    }
    if (!app->view_state_bridge.zoom_logic_enabled) {
        return true;
    }
    return app_layer_fade_multiplier(app, kind) > 0.001f;
}

void app_update_vk_line_budget(AppState *app) {
    if (!app) {
        return;
    }
    if (renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN) {
        return;
    }
    app->renderer.vk_line_budget = layer_policy_vk_line_budget(app->view_state_bridge.camera.zoom, app->tile_state_bridge.visible_tile_count);
}

