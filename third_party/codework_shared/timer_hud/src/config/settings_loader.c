#include "timer_hud/settings_loader.h"
#include "../core/session.h"
#include "../core/timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../external/cJSON.h"

#define DEFAULT_HUD_VISUAL_MODE "text_compact"
#define DEFAULT_HUD_SCALE_MODE "dynamic"

static const TimerHUDSettings k_default_settings = {
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

static TimerHUDSession* default_session(void) {
    return ts_default_session_internal();
}

static TimerHUDSettings* session_settings_mutable(TimerHUDSession* session) {
    return session ? &session->settings : NULL;
}

static const TimerHUDSettings* session_settings(const TimerHUDSession* session) {
    return session ? &session->settings : NULL;
}

TimerHUDSettings* ts_legacy_settings_mutable(void) {
    return &default_session()->settings;
}

const char* ts_visual_mode_name(TimerHUDVisualMode mode) {
    switch (mode) {
        case TIMER_HUD_VISUAL_MODE_TEXT_COMPACT:
            return "text_compact";
        case TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH:
            return "history_graph";
        case TIMER_HUD_VISUAL_MODE_HYBRID:
            return "hybrid";
        case TIMER_HUD_VISUAL_MODE_STATS:
            return "stats";
        case TIMER_HUD_VISUAL_MODE_SPIKES:
            return "spikes";
        case TIMER_HUD_VISUAL_MODE_COMPARE:
            return "compare";
        case TIMER_HUD_VISUAL_MODE_INVALID:
        default:
            return NULL;
    }
}

TimerHUDVisualMode ts_visual_mode_from_string(const char* mode) {
    if (!mode) {
        return TIMER_HUD_VISUAL_MODE_INVALID;
    }
    if (strcmp(mode, "text_compact") == 0) {
        return TIMER_HUD_VISUAL_MODE_TEXT_COMPACT;
    }
    if (strcmp(mode, "history_graph") == 0) {
        return TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH;
    }
    if (strcmp(mode, "history") == 0) {
        return TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH;
    }
    if (strcmp(mode, "hybrid") == 0) {
        return TIMER_HUD_VISUAL_MODE_HYBRID;
    }
    if (strcmp(mode, "stats") == 0) {
        return TIMER_HUD_VISUAL_MODE_STATS;
    }
    if (strcmp(mode, "spikes") == 0) {
        return TIMER_HUD_VISUAL_MODE_SPIKES;
    }
    if (strcmp(mode, "compare") == 0) {
        return TIMER_HUD_VISUAL_MODE_COMPARE;
    }
    return TIMER_HUD_VISUAL_MODE_INVALID;
}

void ts_get_default_settings_copy(TimerHUDSettings* out_settings) {
    if (!out_settings) {
        return;
    }
    *out_settings = k_default_settings;
}

static int is_valid_hud_visual_mode(const char* mode) {
    return ts_visual_mode_from_string(mode) != TIMER_HUD_VISUAL_MODE_INVALID;
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

static void sanitize_settings_in_place(TimerHUDSettings* settings) {
    if (!settings) {
        return;
    }

    if (!is_valid_hud_visual_mode(settings->hud_visual_mode)) {
        strncpy(settings->hud_visual_mode,
                DEFAULT_HUD_VISUAL_MODE,
                sizeof(settings->hud_visual_mode) - 1);
        settings->hud_visual_mode[sizeof(settings->hud_visual_mode) - 1] = '\0';
    }

    if (!is_valid_scale_mode(settings->hud_scale_mode)) {
        strncpy(settings->hud_scale_mode,
                DEFAULT_HUD_SCALE_MODE,
                sizeof(settings->hud_scale_mode) - 1);
        settings->hud_scale_mode[sizeof(settings->hud_scale_mode) - 1] = '\0';
    }

    if (settings->hud_graph_samples < 8) settings->hud_graph_samples = 8;
    if (settings->hud_graph_samples > TIMER_HISTORY_SIZE) settings->hud_graph_samples = TIMER_HISTORY_SIZE;

    if (settings->hud_graph_width < 80) settings->hud_graph_width = 80;
    if (settings->hud_graph_height < 16) settings->hud_graph_height = 16;

    if (settings->hud_scale_fixed_max_ms < 0.1f) settings->hud_scale_fixed_max_ms = 0.1f;
    if (settings->hud_scale_decay <= 0.0f || settings->hud_scale_decay >= 1.0f) {
        settings->hud_scale_decay = 0.94f;
    }
}

const TimerHUDSettings* ts_session_get_settings(const TimerHUDSession* session) {
    return session_settings(session);
}

const TimerHUDSettings* ts_get_settings(void) {
    return ts_session_get_settings(default_session());
}

void ts_session_get_settings_copy(const TimerHUDSession* session, TimerHUDSettings* out_settings) {
    if (!session || !out_settings) {
        return;
    }
    *out_settings = session->settings;
}

void ts_get_settings_copy(TimerHUDSettings* out_settings) {
    ts_session_get_settings_copy(default_session(), out_settings);
}

bool ts_session_apply_settings(TimerHUDSession* session, const TimerHUDSettings* settings) {
    TimerHUDSettings* destination = session_settings_mutable(session);
    if (!destination || !settings) {
        return false;
    }
    *destination = *settings;
    sanitize_settings_in_place(destination);
    return true;
}

bool ts_apply_settings(const TimerHUDSettings* settings) {
    return ts_session_apply_settings(default_session(), settings);
}

void ts_session_set_hud_enabled(TimerHUDSession* session, bool enabled) {
    TimerHUDSettings* settings = session_settings_mutable(session);
    if (!settings) {
        return;
    }
    settings->hud_enabled = enabled;
}

void ts_set_hud_enabled(bool enabled) {
    ts_session_set_hud_enabled(default_session(), enabled);
}

bool ts_session_is_hud_enabled(const TimerHUDSession* session) {
    const TimerHUDSettings* settings = session_settings(session);
    return settings ? settings->hud_enabled : false;
}

bool ts_is_hud_enabled(void) {
    return ts_session_is_hud_enabled(default_session());
}

void ts_session_set_log_enabled(TimerHUDSession* session, bool enabled) {
    TimerHUDSettings* settings = session_settings_mutable(session);
    if (!settings) {
        return;
    }
    settings->log_enabled = enabled;
}

void ts_set_log_enabled(bool enabled) {
    ts_session_set_log_enabled(default_session(), enabled);
}

bool ts_session_is_log_enabled(const TimerHUDSession* session) {
    const TimerHUDSettings* settings = session_settings(session);
    return settings ? settings->log_enabled : false;
}

bool ts_is_log_enabled(void) {
    return ts_session_is_log_enabled(default_session());
}

void ts_session_set_event_tagging_enabled(TimerHUDSession* session, bool enabled) {
    TimerHUDSettings* settings = session_settings_mutable(session);
    if (!settings) {
        return;
    }
    settings->event_tagging_enabled = enabled;
}

void ts_set_event_tagging_enabled(bool enabled) {
    ts_session_set_event_tagging_enabled(default_session(), enabled);
}

bool ts_session_is_event_tagging_enabled(const TimerHUDSession* session) {
    const TimerHUDSettings* settings = session_settings(session);
    return settings ? settings->event_tagging_enabled : false;
}

bool ts_is_event_tagging_enabled(void) {
    return ts_session_is_event_tagging_enabled(default_session());
}

const char* ts_session_get_hud_visual_mode(const TimerHUDSession* session) {
    const TimerHUDSettings* settings = session_settings(session);
    return settings ? settings->hud_visual_mode : NULL;
}

const char* ts_get_hud_visual_mode(void) {
    return ts_session_get_hud_visual_mode(default_session());
}

TimerHUDVisualMode ts_session_get_hud_visual_mode_kind(const TimerHUDSession* session) {
    return ts_visual_mode_from_string(ts_session_get_hud_visual_mode(session));
}

TimerHUDVisualMode ts_get_hud_visual_mode_kind(void) {
    return ts_session_get_hud_visual_mode_kind(default_session());
}

const char* ts_session_get_log_filepath(const TimerHUDSession* session) {
    const TimerHUDSettings* settings = session_settings(session);
    return settings ? settings->log_filepath : NULL;
}

const char* ts_get_log_filepath(void) {
    return ts_session_get_log_filepath(default_session());
}

bool ts_session_set_hud_visual_mode(TimerHUDSession* session, const char* mode) {
    TimerHUDSettings* settings = session_settings_mutable(session);
    TimerHUDVisualMode parsed = ts_visual_mode_from_string(mode);
    const char* normalized_mode = ts_visual_mode_name(parsed);
    if (!settings || !normalized_mode) {
        return false;
    }

    strncpy(settings->hud_visual_mode, normalized_mode, sizeof(settings->hud_visual_mode) - 1);
    settings->hud_visual_mode[sizeof(settings->hud_visual_mode) - 1] = '\0';
    return true;
}

bool ts_set_hud_visual_mode(const char* mode) {
    return ts_session_set_hud_visual_mode(default_session(), mode);
}

bool ts_session_set_hud_visual_mode_kind(TimerHUDSession* session, TimerHUDVisualMode mode) {
    const char* normalized_mode = ts_visual_mode_name(mode);
    if (!normalized_mode) {
        return false;
    }
    return ts_session_set_hud_visual_mode(session, normalized_mode);
}

bool ts_set_hud_visual_mode_kind(TimerHUDVisualMode mode) {
    return ts_session_set_hud_visual_mode_kind(default_session(), mode);
}

void ts_session_save_settings_to_file(TimerHUDSession* session, const char* path) {
    FILE* file = NULL;
    TimerHUDSettings settings_copy;

    if (!session || !path) return;

    file = fopen(path, "w");
    if (!file) return;

    settings_copy = session->settings;
    sanitize_settings_in_place(&settings_copy);

    fprintf(file, "{\n");
    fprintf(file, "  \"hud_enabled\": %s,\n", settings_copy.hud_enabled ? "true" : "false");
    fprintf(file, "  \"log_enabled\": %s,\n", settings_copy.log_enabled ? "true" : "false");
    fprintf(file, "  \"event_tagging_enabled\": %s,\n", settings_copy.event_tagging_enabled ? "true" : "false");
    fprintf(file, "  \"log_filepath\": \"%s\",\n", settings_copy.log_filepath);
    fprintf(file, "  \"log_format\": \"%s\",\n", settings_copy.log_format);
    fprintf(file, "  \"render_mode\": \"%s\",\n", settings_copy.render_mode);
    fprintf(file, "  \"render_threshold\": %.3f,\n", settings_copy.render_threshold);
    fprintf(file, "  \"hud_position\": \"%s\",\n", settings_copy.hud_position);
    fprintf(file, "  \"hud_offset_x\": %d,\n", settings_copy.hud_offset_x);
    fprintf(file, "  \"hud_offset_y\": %d,\n", settings_copy.hud_offset_y);
    fprintf(file, "  \"hud_visual_mode\": \"%s\",\n", settings_copy.hud_visual_mode);
    fprintf(file, "  \"hud_compact_text\": %s,\n", settings_copy.hud_compact_text ? "true" : "false");
    fprintf(file, "  \"hud_graph_samples\": %d,\n", settings_copy.hud_graph_samples);
    fprintf(file, "  \"hud_graph_width\": %d,\n", settings_copy.hud_graph_width);
    fprintf(file, "  \"hud_graph_height\": %d,\n", settings_copy.hud_graph_height);
    fprintf(file, "  \"hud_scale_mode\": \"%s\",\n", settings_copy.hud_scale_mode);
    fprintf(file, "  \"hud_scale_fixed_max_ms\": %.3f,\n", settings_copy.hud_scale_fixed_max_ms);
    fprintf(file, "  \"hud_scale_decay\": %.3f,\n", settings_copy.hud_scale_decay);
    fprintf(file, "  \"hud_show_avg\": %s,\n", settings_copy.hud_show_avg ? "true" : "false");
    fprintf(file, "  \"hud_show_minmax\": %s,\n", settings_copy.hud_show_minmax ? "true" : "false");
    fprintf(file, "  \"hud_show_stddev\": %s\n", settings_copy.hud_show_stddev ? "true" : "false");
    fprintf(file, "}\n");

    fclose(file);
}

void save_settings_to_file(const char* path) {
    ts_session_save_settings_to_file(default_session(), path);
}

bool ts_session_load_settings(TimerHUDSession* session, const char* filepath) {
    FILE* file = NULL;
    char* buffer = NULL;
    cJSON* root = NULL;
    TimerHUDSettings loaded_settings;
    long size = 0;
    cJSON* val = NULL;

    if (!session || !filepath) {
        return false;
    }

    file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "[TimeScope] Failed to open settings file: %s\n", filepath);
        return false;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }
    fread(buffer, 1, (size_t)size, file);
    buffer[size] = '\0';
    fclose(file);

    root = cJSON_Parse(buffer);
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

    loaded_settings = session->settings;

    if ((val = cJSON_GetObjectItem(root, "hud_enabled")) && cJSON_IsBool(val)) {
        loaded_settings.hud_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "log_enabled")) && cJSON_IsBool(val)) {
        loaded_settings.log_enabled = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "event_tagging_enabled")) && cJSON_IsBool(val)) {
        loaded_settings.event_tagging_enabled = cJSON_IsTrue(val);
    }

    json_copy_string(loaded_settings.log_filepath, sizeof(loaded_settings.log_filepath), root, "log_filepath");
    json_copy_string(loaded_settings.log_format, sizeof(loaded_settings.log_format), root, "log_format");
    json_copy_string(loaded_settings.render_mode, sizeof(loaded_settings.render_mode), root, "render_mode");

    if ((val = cJSON_GetObjectItem(root, "render_threshold")) && cJSON_IsNumber(val)) {
        loaded_settings.render_threshold = (float)val->valuedouble;
    }

    json_copy_string(loaded_settings.hud_position, sizeof(loaded_settings.hud_position), root, "hud_position");

    if ((val = cJSON_GetObjectItem(root, "hud_offset_x")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_offset_x = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_offset_y")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_offset_y = val->valueint;
    }

    json_copy_string(loaded_settings.hud_visual_mode, sizeof(loaded_settings.hud_visual_mode), root, "hud_visual_mode");

    if ((val = cJSON_GetObjectItem(root, "hud_compact_text")) && cJSON_IsBool(val)) {
        loaded_settings.hud_compact_text = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "hud_graph_samples")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_graph_samples = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_graph_width")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_graph_width = val->valueint;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_graph_height")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_graph_height = val->valueint;
    }

    json_copy_string(loaded_settings.hud_scale_mode, sizeof(loaded_settings.hud_scale_mode), root, "hud_scale_mode");

    if ((val = cJSON_GetObjectItem(root, "hud_scale_fixed_max_ms")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_scale_fixed_max_ms = (float)val->valuedouble;
    }
    if ((val = cJSON_GetObjectItem(root, "hud_scale_decay")) && cJSON_IsNumber(val)) {
        loaded_settings.hud_scale_decay = (float)val->valuedouble;
    }

    if ((val = cJSON_GetObjectItem(root, "hud_show_avg")) && cJSON_IsBool(val)) {
        loaded_settings.hud_show_avg = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "hud_show_minmax")) && cJSON_IsBool(val)) {
        loaded_settings.hud_show_minmax = cJSON_IsTrue(val);
    }
    if ((val = cJSON_GetObjectItem(root, "hud_show_stddev")) && cJSON_IsBool(val)) {
        loaded_settings.hud_show_stddev = cJSON_IsTrue(val);
    }

    sanitize_settings_in_place(&loaded_settings);
    session->settings = loaded_settings;

    cJSON_Delete(root);
    return true;
}

bool ts_load_settings(const char* filepath) {
    return ts_session_load_settings(default_session(), filepath);
}
