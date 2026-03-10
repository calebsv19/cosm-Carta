#ifndef TIMESCOPE_SETTINGS_LOADER_H
#define TIMESCOPE_SETTINGS_LOADER_H

#include <stdbool.h>

typedef struct TimeScopeSettings {
    bool hud_enabled;
    bool log_enabled;
    bool event_tagging_enabled;

    char log_filepath[256];
    char log_format[16]; // "csv" or "json"

    char render_mode[16];   // "always" or "throttled"
    float render_threshold; // seconds

    char hud_position[32]; // e.g., "top-left"
    int hud_offset_x;      // e.g., 10
    int hud_offset_y;      // e.g., 10

    // HUD visual configuration.
    char hud_visual_mode[24]; // "text_compact", "history_graph", "hybrid"
    bool hud_compact_text;
    int hud_graph_samples;
    int hud_graph_width;
    int hud_graph_height;

    char hud_scale_mode[16]; // "dynamic" or "fixed"
    float hud_scale_fixed_max_ms;
    float hud_scale_decay;

    bool hud_show_avg;
    bool hud_show_minmax;
    bool hud_show_stddev;
} TimeScopeSettings;

// Global access
extern TimeScopeSettings ts_settings;

bool ts_load_settings(const char* filepath);
void save_settings_to_file(const char* path);

#endif // TIMESCOPE_SETTINGS_LOADER_H
