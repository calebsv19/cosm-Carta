#include "timer_hud/settings_loader.h"
#include "../core/timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../external/cJSON.h"

#define DEFAULT_HUD_VISUAL_MODE "text_compact"
#define DEFAULT_HUD_SCALE_MODE "dynamic"

TimeScopeSettings ts_settings = {
    .hud_enabled = true,
    .log_enabled = true,
    .event_tagging_enabled = false,

    .log_filepath = "timing.json",
    .log_format = "json",

    .render_mode = "throttled",
    .render_threshold = 0.033f,

    .hud_position = "top-left",
    .hud_offset_x = 10,
    .hud_offset_y = 10,

    .hud_visual_mode = DEFAULT_HUD_VISUAL_MODE,
    .hud_compact_text = true,
    .hud_graph_samples = 64,
    .hud_graph_width = 220,
    .hud_graph_height = 24,

    .hud_scale_mode = DEFAULT_HUD_SCALE_MODE,
    .hud_scale_fixed_max_ms = 16.0f,
    .hud_scale_decay = 0.94f,

    .hud_show_avg = true,
    .hud_show_minmax = false,
    .hud_show_stddev = false,
};

static int is_valid_hud_visual_mode(const char* mode) {
    return mode && (
        strcmp(mode, "text_compact") == 0 ||
        strcmp(mode, "history_graph") == 0 ||
        strcmp(mode, "hybrid") == 0
    );
}

static int is_valid_scale_mode(const char* mode) {
    return mode && (strcmp(mode, "dynamic") == 0 || strcmp(mode, "fixed") == 0);
}

static void json_copy_string(char* out, size_t out_cap, cJSON* root, const char* key) {
    cJSON* val = cJSON_GetObjectItem(root, key);
    if (!val || !cJSON_IsString(val) || !val->valuestring) return;
    strncpy(out, val->valuestring, out_cap - 1);
    out[out_cap - 1] = '\0';
}

static void sanitize_settings(void) {
    if (!is_valid_hud_visual_mode(ts_settings.hud_visual_mode)) {
        strncpy(ts_settings.hud_visual_mode, DEFAULT_HUD_VISUAL_MODE, sizeof(ts_settings.hud_visual_mode) - 1);
        ts_settings.hud_visual_mode[sizeof(ts_settings.hud_visual_mode) - 1] = '\0';
    }

    if (!is_valid_scale_mode(ts_settings.hud_scale_mode)) {
        strncpy(ts_settings.hud_scale_mode, DEFAULT_HUD_SCALE_MODE, sizeof(ts_settings.hud_scale_mode) - 1);
        ts_settings.hud_scale_mode[sizeof(ts_settings.hud_scale_mode) - 1] = '\0';
    }

    if (ts_settings.hud_graph_samples < 8) ts_settings.hud_graph_samples = 8;
    if (ts_settings.hud_graph_samples > TIMER_HISTORY_SIZE) ts_settings.hud_graph_samples = TIMER_HISTORY_SIZE;

    if (ts_settings.hud_graph_width < 80) ts_settings.hud_graph_width = 80;
    if (ts_settings.hud_graph_height < 16) ts_settings.hud_graph_height = 16;

    if (ts_settings.hud_scale_fixed_max_ms < 0.1f) ts_settings.hud_scale_fixed_max_ms = 0.1f;
    if (ts_settings.hud_scale_decay <= 0.0f || ts_settings.hud_scale_decay >= 1.0f) {
        ts_settings.hud_scale_decay = 0.94f;
    }
}

void save_settings_to_file(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;

    sanitize_settings();

    fprintf(f, "{\n");
    fprintf(f, "  \"hud_enabled\": %s,\n", ts_settings.hud_enabled ? "true" : "false");
    fprintf(f, "  \"log_enabled\": %s,\n", ts_settings.log_enabled ? "true" : "false");
    fprintf(f, "  \"event_tagging_enabled\": %s,\n", ts_settings.event_tagging_enabled ? "true" : "false");
    fprintf(f, "  \"log_filepath\": \"%s\",\n", ts_settings.log_filepath);
    fprintf(f, "  \"log_format\": \"%s\",\n", ts_settings.log_format);
    fprintf(f, "  \"render_mode\": \"%s\",\n", ts_settings.render_mode);
    fprintf(f, "  \"render_threshold\": %.3f,\n", ts_settings.render_threshold);
    fprintf(f, "  \"hud_position\": \"%s\",\n", ts_settings.hud_position);
    fprintf(f, "  \"hud_offset_x\": %d,\n", ts_settings.hud_offset_x);
    fprintf(f, "  \"hud_offset_y\": %d,\n", ts_settings.hud_offset_y);
    fprintf(f, "  \"hud_visual_mode\": \"%s\",\n", ts_settings.hud_visual_mode);
    fprintf(f, "  \"hud_compact_text\": %s,\n", ts_settings.hud_compact_text ? "true" : "false");
    fprintf(f, "  \"hud_graph_samples\": %d,\n", ts_settings.hud_graph_samples);
    fprintf(f, "  \"hud_graph_width\": %d,\n", ts_settings.hud_graph_width);
    fprintf(f, "  \"hud_graph_height\": %d,\n", ts_settings.hud_graph_height);
    fprintf(f, "  \"hud_scale_mode\": \"%s\",\n", ts_settings.hud_scale_mode);
    fprintf(f, "  \"hud_scale_fixed_max_ms\": %.3f,\n", ts_settings.hud_scale_fixed_max_ms);
    fprintf(f, "  \"hud_scale_decay\": %.3f,\n", ts_settings.hud_scale_decay);
    fprintf(f, "  \"hud_show_avg\": %s,\n", ts_settings.hud_show_avg ? "true" : "false");
    fprintf(f, "  \"hud_show_minmax\": %s,\n", ts_settings.hud_show_minmax ? "true" : "false");
    fprintf(f, "  \"hud_show_stddev\": %s\n", ts_settings.hud_show_stddev ? "true" : "false");
    fprintf(f, "}\n");

    fclose(f);
}

bool ts_load_settings(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        fprintf(stderr, "[TimeScope] Failed to open settings file: %s\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }
    fread(buffer, 1, (size_t)size, f);
    buffer[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buffer);
    if (!root) {
        const char* error_ptr = cJSON_GetErrorPtr();
        fprintf(stderr, "[TimeScope] Failed to parse JSON settings.\n");
        if (error_ptr) {
            fprintf(stderr, "  Error before: %.20s\n", error_ptr);
        }
        fprintf(stderr, "[TimeScope] Full contents:\n%s\n", buffer);
        free(buffer);
        return false;
    }
    free(buffer);

    cJSON* val;
    if ((val = cJSON_GetObjectItem(root, "hud_enabled")) && cJSON_IsBool(val)) {
        ts_settings.hud_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "log_enabled")) && cJSON_IsBool(val)) {
        ts_settings.log_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "event_tagging_enabled")) && cJSON_IsBool(val)) {
        ts_settings.event_tagging_enabled = cJSON_IsTrue(val);
    }

    json_copy_string(ts_settings.log_filepath, sizeof(ts_settings.log_filepath), root, "log_filepath");
    json_copy_string(ts_settings.log_format, sizeof(ts_settings.log_format), root, "log_format");
    json_copy_string(ts_settings.render_mode, sizeof(ts_settings.render_mode), root, "render_mode");

    if ((val = cJSON_GetObjectItem(root, "render_threshold")) && cJSON_IsNumber(val)) {
        ts_settings.render_threshold = (float)val->valuedouble;
    }

    json_copy_string(ts_settings.hud_position, sizeof(ts_settings.hud_position), root, "hud_position");

    if ((val = cJSON_GetObjectItem(root, "hud_offset_x")) && cJSON_IsNumber(val)) {
        ts_settings.hud_offset_x = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_offset_y")) && cJSON_IsNumber(val)) {
        ts_settings.hud_offset_y = val->valueint;
    }

    json_copy_string(ts_settings.hud_visual_mode, sizeof(ts_settings.hud_visual_mode), root, "hud_visual_mode");

    if ((val = cJSON_GetObjectItem(root, "hud_compact_text")) && cJSON_IsBool(val)) {
        ts_settings.hud_compact_text = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "hud_graph_samples")) && cJSON_IsNumber(val)) {
        ts_settings.hud_graph_samples = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_graph_width")) && cJSON_IsNumber(val)) {
        ts_settings.hud_graph_width = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_graph_height")) && cJSON_IsNumber(val)) {
        ts_settings.hud_graph_height = val->valueint;
    }

    json_copy_string(ts_settings.hud_scale_mode, sizeof(ts_settings.hud_scale_mode), root, "hud_scale_mode");

    if ((val = cJSON_GetObjectItem(root, "hud_scale_fixed_max_ms")) && cJSON_IsNumber(val)) {
        ts_settings.hud_scale_fixed_max_ms = (float)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_scale_decay")) && cJSON_IsNumber(val)) {
        ts_settings.hud_scale_decay = (float)val->valuedouble;
    }

    if ((val = cJSON_GetObjectItem(root, "hud_show_avg")) && cJSON_IsBool(val)) {
        ts_settings.hud_show_avg = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "hud_show_minmax")) && cJSON_IsBool(val)) {
        ts_settings.hud_show_minmax = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "hud_show_stddev")) && cJSON_IsBool(val)) {
        ts_settings.hud_show_stddev = cJSON_IsTrue(val);
    }

    sanitize_settings();
    cJSON_Delete(root);
    return true;
}
