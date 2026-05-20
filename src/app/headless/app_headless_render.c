#include "app/app_headless.h"
#include "app_headless_render_internal.h"

#include "camera/camera.h"
#include "map/mercator.h"
#include "render/renderer.h"
#include "route/route_render.h"

#include <json-c/json.h>
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static bool map_forge_headless_render_ensure_dir(const char *path) {
    char tmp[1024];
    size_t len = 0u;
    if (!path || path[0] == '\0') {
        return false;
    }
    len = strnlen(path, sizeof(tmp) - 1u);
    if (len == 0u || len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, path, len);
    tmp[len] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return false;
        }
        *p = '/';
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static void map_forge_headless_render_camera_fit(Camera *camera,
                                                 const RouteGraph *graph,
                                                 const RoutePath *path,
                                                 const MapForgeHeadlessRenderPin *from_pin,
                                                 const MapForgeHeadlessRenderPin *to_pin,
                                                 int width,
                                                 int height) {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    bool have_bounds = false;
    if (!camera || !graph || !path || path->count == 0u || !graph->node_x || !graph->node_y || width <= 0 || height <= 0) {
        return;
    }

    for (uint32_t i = 0; i < path->count; ++i) {
        uint32_t node = path->nodes[i];
        float x = (float)graph->node_x[node];
        float y = (float)graph->node_y[node];
        if (!have_bounds) {
            min_x = max_x = x;
            min_y = max_y = y;
            have_bounds = true;
        } else {
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (from_pin && from_pin->valid) {
        float x = (float)from_pin->world_x;
        float y = (float)from_pin->world_y;
        if (!have_bounds) {
            min_x = max_x = x;
            min_y = max_y = y;
            have_bounds = true;
        } else {
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (to_pin && to_pin->valid) {
        float x = (float)to_pin->world_x;
        float y = (float)to_pin->world_y;
        if (!have_bounds) {
            min_x = max_x = x;
            min_y = max_y = y;
            have_bounds = true;
        } else {
            if (x < min_x) min_x = x;
            if (x > max_x) max_x = x;
            if (y < min_y) min_y = y;
            if (y > max_y) max_y = y;
        }
    }
    if (!have_bounds) {
        return;
    }

    {
        float world_w = max_x - min_x;
        float world_h = max_y - min_y;
        float pad_x = (float)width * 0.10f;
        float pad_y = (float)height * 0.10f;
        float usable_w = (float)width - pad_x * 2.0f;
        float usable_h = (float)height - pad_y * 2.0f;
        float ppm_x = usable_w / fmaxf(world_w, 1.0f);
        float ppm_y = usable_h / fmaxf(world_h, 1.0f);
        float ppm = fminf(ppm_x, ppm_y);
        double zoom = log2((double)ppm * mercator_world_size_meters() / 256.0);
        if (!isfinite(zoom)) {
            zoom = 14.0;
        }
        if (zoom < 10.0) {
            zoom = 10.0;
        }
        if (zoom > 18.0) {
            zoom = 18.0;
        }

        camera->x = camera->x_target = (min_x + max_x) * 0.5f;
        camera->y = camera->y_target = (min_y + max_y) * 0.5f;
        camera->zoom = camera->zoom_target = (float)zoom;
        camera->heading_rad = 0.0f;
        camera->heading_target_rad = 0.0f;
    }
}

static void map_forge_headless_render_playback_marker(Renderer *renderer,
                                                      const Camera *camera,
                                                      const MapForgeHeadlessPlaybackSample *sample) {
    SDL_FRect rect;
    float sx = 0.0f;
    float sy = 0.0f;
    if (!renderer || !camera || !sample || !sample->valid) {
        return;
    }
    camera_world_to_screen(camera,
                           (float)sample->world_x,
                           (float)sample->world_y,
                           renderer->width,
                           renderer->height,
                           &sx,
                           &sy);
    renderer_set_draw_color(renderer, 255, 230, 80, 240);
    rect.x = sx - 5.0f;
    rect.y = sy - 5.0f;
    rect.w = 10.0f;
    rect.h = 10.0f;
    renderer_fill_rect(renderer, &rect);
}

static const char *map_forge_headless_layer_name(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY: return "road_artery";
        case TILE_LAYER_ROAD_LOCAL: return "road_local";
        case TILE_LAYER_CONTOUR: return "contour";
        case TILE_LAYER_POLY_WATER: return "water";
        case TILE_LAYER_POLY_PARK: return "park";
        case TILE_LAYER_POLY_LANDUSE: return "landuse";
        case TILE_LAYER_POLY_BUILDING: return "building";
        default: return "unknown";
    }
}

static const char *map_forge_headless_band_name(TileZoomBand band) {
    return layer_policy_band_label(band);
}

static bool map_forge_headless_render_write_debug_json(const char *out_dir,
                                                       const AppState *layer_app,
                                                       const AppVisibleTileRenderStats *tile_stats,
                                                       const Camera *camera,
                                                       const MapForgeHeadlessPlaybackSample *sample) {
    char path[1024];
    const char *json_text = NULL;
    struct json_object *root = NULL;
    struct json_object *camera_obj = NULL;
    struct json_object *sample_obj = NULL;
    struct json_object *visible_obj = NULL;
    struct json_object *stats_obj = NULL;
    struct json_object *layers_obj = NULL;
    FILE *fp = NULL;
    if (!out_dir || !layer_app || !tile_stats || !camera) {
        return false;
    }

    root = json_object_new_object();
    camera_obj = json_object_new_object();
    sample_obj = json_object_new_object();
    visible_obj = json_object_new_object();
    stats_obj = json_object_new_object();
    layers_obj = json_object_new_object();

    json_object_object_add(root, "version", json_object_new_int(1));
    json_object_object_add(root, "kind", json_object_new_string("headless_render_debug"));
    json_object_object_add(root, "renderer_backend", json_object_new_string(renderer_backend_name(layer_app->renderer.backend)));

    json_object_object_add(camera_obj, "x", json_object_new_double(camera->x));
    json_object_object_add(camera_obj, "y", json_object_new_double(camera->y));
    json_object_object_add(camera_obj, "zoom", json_object_new_double(camera->zoom));
    json_object_object_add(camera_obj, "heading_rad", json_object_new_double(camera->heading_rad));
    json_object_object_add(root, "camera", camera_obj);

    json_object_object_add(sample_obj, "valid", json_object_new_boolean(sample && sample->valid));
    if (sample && sample->valid) {
        json_object_object_add(sample_obj, "world_x", json_object_new_double(sample->world_x));
        json_object_object_add(sample_obj, "world_y", json_object_new_double(sample->world_y));
        json_object_object_add(sample_obj, "route_time_s", json_object_new_double(sample->route_time_s));
        json_object_object_add(sample_obj, "heading_rad", json_object_new_double(sample->heading_rad));
        json_object_object_add(sample_obj, "segment_index", json_object_new_int64((int64_t)sample->segment_index));
    }
    json_object_object_add(root, "playback_sample", sample_obj);

    json_object_object_add(visible_obj, "valid", json_object_new_boolean(layer_app->tile_state_bridge.visible_valid));
    json_object_object_add(visible_obj, "zoom", json_object_new_int((int)layer_app->tile_state_bridge.visible_zoom));
    json_object_object_add(visible_obj, "tile_count", json_object_new_int64((int64_t)layer_app->tile_state_bridge.visible_tile_count));
    json_object_object_add(visible_obj, "top_left_x", json_object_new_int64((int64_t)layer_app->tile_state_bridge.visible_top_left.x));
    json_object_object_add(visible_obj, "top_left_y", json_object_new_int64((int64_t)layer_app->tile_state_bridge.visible_top_left.y));
    json_object_object_add(visible_obj, "bottom_right_x", json_object_new_int64((int64_t)layer_app->tile_state_bridge.visible_bottom_right.x));
    json_object_object_add(visible_obj, "bottom_right_y", json_object_new_int64((int64_t)layer_app->tile_state_bridge.visible_bottom_right.y));
    json_object_object_add(root, "visible_tiles", visible_obj);

    json_object_object_add(stats_obj, "draw_visible_tiles_count", json_object_new_int64((int64_t)tile_stats->visible_tiles));
    json_object_object_add(stats_obj, "loading_expected", json_object_new_int64((int64_t)tile_stats->loading_expected));
    json_object_object_add(stats_obj, "loading_done", json_object_new_int64((int64_t)tile_stats->loading_done));
    json_object_object_add(stats_obj, "vk_asset_misses", json_object_new_int64((int64_t)tile_stats->vk_asset_misses));
    json_object_object_add(stats_obj, "visible_coverage_ratio", json_object_new_double(layer_app->tile_state_bridge.visible_coverage_ratio));
    json_object_object_add(stats_obj, "draw_path_vk_count", json_object_new_int64((int64_t)layer_app->tile_state_bridge.draw_path_vk_count));
    json_object_object_add(stats_obj, "draw_path_fallback_count", json_object_new_int64((int64_t)layer_app->tile_state_bridge.draw_path_fallback_count));
    json_object_object_add(stats_obj, "transition_blend_draw_count", json_object_new_int64((int64_t)layer_app->tile_state_bridge.transition_blend_draw_count));
    json_object_object_add(root, "render_stats", stats_obj);

    for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
        struct json_object *layer_obj = json_object_new_object();
        json_object_object_add(layer_obj, "runtime_enabled", json_object_new_boolean(app_layer_runtime_enabled(layer_app, (TileLayerKind)i)));
        json_object_object_add(layer_obj, "active_runtime", json_object_new_boolean(app_layer_active_runtime(layer_app, (TileLayerKind)i)));
        json_object_object_add(layer_obj, "policy_band", json_object_new_int((int)layer_app->tile_state_bridge.previous_target_band[i]));
        json_object_object_add(layer_obj, "policy_band_label",
                               json_object_new_string(map_forge_headless_band_name(layer_app->tile_state_bridge.previous_target_band[i])));
        json_object_object_add(layer_obj, "target_band", json_object_new_int((int)layer_app->tile_state_bridge.layer_target_band[i]));
        json_object_object_add(layer_obj, "target_band_label",
                               json_object_new_string(map_forge_headless_band_name(layer_app->tile_state_bridge.layer_target_band[i])));
        json_object_object_add(layer_obj, "expected", json_object_new_int64((int64_t)layer_app->tile_state_bridge.layer_expected[i]));
        json_object_object_add(layer_obj, "loaded", json_object_new_int64((int64_t)layer_app->tile_state_bridge.layer_done[i]));
        json_object_object_add(layer_obj, "visible_expected", json_object_new_int64((int64_t)layer_app->tile_state_bridge.layer_visible_expected[i]));
        json_object_object_add(layer_obj, "visible_loaded", json_object_new_int64((int64_t)layer_app->tile_state_bridge.layer_visible_loaded[i]));
        json_object_object_add(layer_obj, "coverage_ratio", json_object_new_double(layer_app->tile_state_bridge.layer_coverage_ratio[i]));
        json_object_object_add(layer_obj, "state", json_object_new_int((int)layer_app->tile_state_bridge.layer_state[i]));
        json_object_object_add(layers_obj, map_forge_headless_layer_name((TileLayerKind)i), layer_obj);
    }
    json_object_object_add(root, "layers", layers_obj);

    snprintf(path, sizeof(path), "%s/headless_render_debug.json", out_dir);
    json_text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    fp = fopen(path, "wb");
    if (!fp) {
        json_object_put(root);
        return false;
    }
    if (json_text && json_text[0] != '\0') {
        (void)fwrite(json_text, 1u, strlen(json_text), fp);
    }
    fclose(fp);
    json_object_put(root);
    return true;
}

static void map_forge_headless_render_frame(Renderer *renderer,
                                            Camera *camera,
                                            AppState *layer_app,
                                            AppVisibleTileRenderStats *layer_stats,
                                            const MapForgeHeadlessJob *job,
                                            const MapForgeHeadlessRenderPin *from_pin,
                                            const MapForgeHeadlessRenderPin *to_pin,
                                            const RouteState *route_state,
                                            const MapForgeHeadlessPlaybackSample *sample) {
    bool alt_visible[ROUTE_ALTERNATIVE_MAX];
    if (!renderer || !camera || !job || !route_state) {
        return;
    }
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        alt_visible[i] = true;
    }
    if (sample && sample->valid && job->camera.follow_route) {
        camera->x = camera->x_target = (float)sample->world_x;
        camera->y = camera->y_target = (float)sample->world_y;
    }
    if (sample && sample->valid && job->camera.rotate_with_heading) {
        camera->heading_rad = sample->heading_rad;
        camera->heading_target_rad = sample->heading_rad;
    } else {
        camera->heading_rad = 0.0f;
        camera->heading_target_rad = 0.0f;
    }
    renderer_begin_frame(renderer);
    renderer_clear(renderer, 18, 22, 28, 255);
    if (layer_app && map_forge_headless_map_layers_prepare_frame(layer_app, renderer, camera)) {
        map_forge_headless_map_layers_draw(layer_app, layer_stats);
    }
    if (job->output.render_mode != MAPFORGE_HEADLESS_RENDER_MODE_MAP_ONLY) {
        route_render_draw(renderer,
                          camera,
                          &route_state->graph,
                          &route_state->path,
                          &route_state->drive_path,
                          &route_state->walk_path,
                          &route_state->alternatives,
                          route_state->objective,
                          alt_visible,
                          from_pin && from_pin->valid,
                          from_pin ? from_pin->nearest_node : 0u,
                          from_pin && from_pin->valid,
                          from_pin ? (float)from_pin->world_x : 0.0f,
                          from_pin ? (float)from_pin->world_y : 0.0f,
                          to_pin && to_pin->valid,
                          to_pin ? to_pin->nearest_node : 0u,
                          to_pin && to_pin->valid,
                          to_pin ? (float)to_pin->world_x : 0.0f,
                          to_pin ? (float)to_pin->world_y : 0.0f,
                          route_state->has_transfer,
                          route_state->transfer_node);
    }
    if (job->output.render_mode == MAPFORGE_HEADLESS_RENDER_MODE_MAP_ROUTE_MARKER &&
        sample && sample->valid) {
        map_forge_headless_render_playback_marker(renderer, camera, sample);
    }
}

bool map_forge_headless_render_route_images(const char *out_dir,
                                            const MapForgeHeadlessJob *job,
                                            const RegionInfo *region,
                                            const MapForgeHeadlessRenderPin *from_pin,
                                            const MapForgeHeadlessRenderPin *to_pin,
                                            const RouteState *route_state,
                                            const MapForgeHeadlessPlaybackSample *preview_sample,
                                            const MapForgeHeadlessPlaybackSample *frame_samples,
                                            uint32_t frame_count,
                                            MapForgeHeadlessImageExportResult *out_result) {
    SDL_Surface *surface = NULL;
    Renderer renderer;
    Camera camera;
    AppState layer_app;
    AppVisibleTileRenderStats debug_tile_stats;
    int width = 1280;
    int height = 720;
    bool sdl_video_ready = false;
    bool layer_app_ready = false;
    MapForgeHeadlessImageExportResult result;

    memset(&result, 0, sizeof(result));
    memset(&layer_app, 0, sizeof(layer_app));
    memset(&debug_tile_stats, 0, sizeof(debug_tile_stats));
    if (!out_dir || !job || !region || !route_state || !out_result || route_state->path.count < 2u) {
        return false;
    }
    if (!job->output.preview_png && !job->output.frames) {
        *out_result = result;
        return true;
    }
    if (job->camera.has_width && job->camera.width > 0) {
        width = job->camera.width;
    }
    if (job->camera.has_height && job->camera.height > 0) {
        height = job->camera.height;
    }

    if ((job->output.preview_png || job->output.frames) &&
        strcmp(job->output.frame_format, "bmp") != 0) {
        return false;
    }

    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    if ((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            return false;
        }
        sdl_video_ready = true;
    }

    memset(&renderer, 0, sizeof(renderer));
    renderer_set_backend(&renderer, RENDERER_BACKEND_SDL);
    surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        if (sdl_video_ready) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        return false;
    }
    renderer.sdl = SDL_CreateSoftwareRenderer(surface);
    if (!renderer.sdl) {
        SDL_FreeSurface(surface);
        if (sdl_video_ready) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        return false;
    }
    renderer.width = width;
    renderer.height = height;
    SDL_SetRenderDrawBlendMode(renderer.sdl, SDL_BLENDMODE_BLEND);

    camera_init(&camera);
    map_forge_headless_render_camera_fit(&camera, &route_state->graph, &route_state->path, from_pin, to_pin, width, height);
    if (job->camera.has_zoom) {
        camera.zoom = camera.zoom_target = job->camera.zoom;
    }
    layer_app_ready = map_forge_headless_map_layers_init(&layer_app, &renderer, region, width, height);

    if (job->output.preview_png) {
        char preview_path[1024];
        map_forge_headless_render_frame(&renderer,
                                        &camera,
                                        layer_app_ready ? &layer_app : NULL,
                                        &debug_tile_stats,
                                        job,
                                        from_pin,
                                        to_pin,
                                        route_state,
                                        preview_sample);
        snprintf(preview_path, sizeof(preview_path), "%s/preview.bmp", out_dir);
        if (SDL_SaveBMP(surface, preview_path) != 0) {
            renderer_end_frame(&renderer);
            if (layer_app_ready) {
                map_forge_headless_map_layers_shutdown(&layer_app);
            }
            renderer_shutdown(&renderer);
            SDL_FreeSurface(surface);
            if (sdl_video_ready) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            }
            return false;
        }
        renderer_end_frame(&renderer);
        result.preview_written = true;
        snprintf(result.preview_artifact, sizeof(result.preview_artifact), "preview.bmp");
        if (layer_app_ready &&
            map_forge_headless_render_write_debug_json(out_dir, &layer_app, &debug_tile_stats, &camera, preview_sample)) {
            result.render_debug_written = true;
            snprintf(result.render_debug_artifact, sizeof(result.render_debug_artifact), "headless_render_debug.json");
        }
    }

    if (job->output.frames && frame_samples && frame_count > 0u) {
        char frames_dir[1024];
        snprintf(frames_dir, sizeof(frames_dir), "%s/frames", out_dir);
        if (!map_forge_headless_render_ensure_dir(frames_dir)) {
            renderer_shutdown(&renderer);
            SDL_FreeSurface(surface);
            if (sdl_video_ready) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            }
            return false;
        }
        for (uint32_t i = 0; i < frame_count; ++i) {
            char frame_path[1024];
            map_forge_headless_render_frame(&renderer,
                                            &camera,
                                            layer_app_ready ? &layer_app : NULL,
                                            NULL,
                                            job,
                                            from_pin,
                                            to_pin,
                                            route_state,
                                            &frame_samples[i]);
            snprintf(frame_path, sizeof(frame_path), "%s/frame_%06u.bmp", frames_dir, i + 1u);
            if (SDL_SaveBMP(surface, frame_path) != 0) {
                renderer_end_frame(&renderer);
                if (layer_app_ready) {
                    map_forge_headless_map_layers_shutdown(&layer_app);
                }
                renderer_shutdown(&renderer);
                SDL_FreeSurface(surface);
                if (sdl_video_ready) {
                    SDL_QuitSubSystem(SDL_INIT_VIDEO);
                }
                return false;
            }
            renderer_end_frame(&renderer);
            result.frames_written_count += 1u;
        }
        result.frames_written = result.frames_written_count > 0u;
        if (result.frames_written) {
            snprintf(result.frames_dir_artifact, sizeof(result.frames_dir_artifact), "frames/");
        }
    }

    if (layer_app_ready) {
        map_forge_headless_map_layers_shutdown(&layer_app);
    }
    renderer_shutdown(&renderer);
    SDL_FreeSurface(surface);
    if (sdl_video_ready) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    *out_result = result;
    return true;
}
