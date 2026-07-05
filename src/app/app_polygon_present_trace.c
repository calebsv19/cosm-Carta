#include "app/app_tile_render_internal.h"

#include "core/log.h"
#include "map/layer_policy.h"

#include <stdlib.h>
#include <string.h>

static bool app_trace_env_truthy(const char *value) {
    if (!value || value[0] == '\0') {
        return false;
    }
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "YES") == 0 ||
           strcmp(value, "on") == 0 ||
           strcmp(value, "ON") == 0;
}

bool app_polygon_present_trace_enabled(void) {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    cached = app_trace_env_truthy(getenv("MAPFORGE_POLYGON_PRESENT_TRACE")) ? 1 : 0;
    return cached != 0;
}

void app_polygon_present_trace_emit(const AppPolygonPresentTraceDecision *decision) {
    if (!decision || !app_polygon_present_trace_enabled()) {
        return;
    }

    log_info("poly_present layer=%s zoom=%.3f req=%u/%u/%u draw=%u/%u/%u same=%u fallback_depth=%u "
             "target=%s resolved=%s opacity=%.3f fade=%.3f vk=%u assets=%u hold(hit=%u valid=%u) "
             "blend(attempt=%u drawn=%u) asset(present=%u fill_ready=%u fill_drawn=%u fill_skip=%u fill_missing=%u outline=%u) "
             "cpu(allowed=%u drawn=%u) enqueue(attempt=%u budget_block=%u ok=%u) no_draw_after_miss=%u",
             layer_policy_label(decision->kind),
             decision->zoom,
             decision->requested_coord.z,
             decision->requested_coord.x,
             decision->requested_coord.y,
             decision->draw_coord.z,
             decision->draw_coord.x,
             decision->draw_coord.y,
             decision->same_coord ? 1u : 0u,
             decision->fallback_depth,
             layer_policy_band_label(decision->target_band),
             layer_policy_band_label(decision->resolved_band),
             decision->layer_opacity,
             decision->fade_multiplier,
             decision->vk_backend ? 1u : 0u,
             decision->vk_assets_enabled ? 1u : 0u,
             decision->present_hold_hit ? 1u : 0u,
             decision->present_hold_valid ? 1u : 0u,
             decision->band_blend_attempted ? 1u : 0u,
             decision->band_blend_drawn ? 1u : 0u,
             decision->asset_present ? 1u : 0u,
             decision->retained_fill_ready ? 1u : 0u,
             decision->retained_fill_drawn ? 1u : 0u,
             decision->retained_fill_skipped_budget ? 1u : 0u,
             decision->retained_fill_missing ? 1u : 0u,
             decision->retained_outline_drawn ? 1u : 0u,
             decision->cpu_fallback_allowed ? 1u : 0u,
             decision->cpu_fallback_drawn ? 1u : 0u,
             decision->asset_enqueue_attempted ? 1u : 0u,
             decision->asset_enqueue_budget_blocked ? 1u : 0u,
             decision->asset_enqueue_succeeded ? 1u : 0u,
             decision->no_draw_after_asset_miss ? 1u : 0u);
}
