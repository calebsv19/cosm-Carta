#include "hud_format.h"

#include <stdio.h>

static long hud_round_to_long(double value) {
    return (long)(value >= 0.0 ? value + 0.5 : value - 0.5);
}

void hud_format_duration_ms(double duration_ms, char* out, size_t out_cap) {
    double abs_ms = duration_ms < 0.0 ? -duration_ms : duration_ms;

    if (!out || out_cap == 0) {
        return;
    }

    if (duration_ms != duration_ms || abs_ms > 1000000000000000.0) {
        snprintf(out, out_cap, "n/a");
        return;
    }

    if (abs_ms < 10.0) {
        snprintf(out, out_cap, "%.2fms", duration_ms);
        return;
    }
    if (abs_ms < 100.0) {
        snprintf(out, out_cap, "%.1fms", duration_ms);
        return;
    }
    if (abs_ms < 1000.0) {
        snprintf(out, out_cap, "%.0fms", duration_ms);
        return;
    }
    if (abs_ms < 10000.0) {
        snprintf(out, out_cap, "%.2fs", duration_ms / 1000.0);
        return;
    }
    if (abs_ms < 60000.0) {
        snprintf(out, out_cap, "%.1fs", duration_ms / 1000.0);
        return;
    }
    if (abs_ms < 3600000.0) {
        long total_seconds = hud_round_to_long(duration_ms / 1000.0);
        long minutes = total_seconds / 60;
        long seconds = total_seconds % 60;
        if (seconds < 0) {
            seconds = -seconds;
        }
        snprintf(out, out_cap, "%ldm %02lds", minutes, seconds);
        return;
    }

    {
        long total_minutes = hud_round_to_long(duration_ms / 60000.0);
        long hours = total_minutes / 60;
        long minutes = total_minutes % 60;
        if (minutes < 0) {
            minutes = -minutes;
        }
        snprintf(out, out_cap, "%ldh %02ldm", hours, minutes);
    }
}
