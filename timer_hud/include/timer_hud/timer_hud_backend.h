#ifndef TIMER_HUD_BACKEND_H
#define TIMER_HUD_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TimerHUDColor {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} TimerHUDColor;

// Alignment flags (can be OR'ed)
#define TIMER_HUD_ALIGN_LEFT     0x01
#define TIMER_HUD_ALIGN_CENTER   0x02
#define TIMER_HUD_ALIGN_RIGHT    0x04
#define TIMER_HUD_ALIGN_TOP      0x10
#define TIMER_HUD_ALIGN_MIDDLE   0x20
#define TIMER_HUD_ALIGN_BOTTOM   0x40

typedef struct TimerHUDBackend {
    void (*init)(void);
    void (*shutdown)(void);

    // Return 1 on success, 0 on failure.
    int (*get_screen_size)(int* out_w, int* out_h);

    // Return text width/height in pixels. Return 1 on success.
    int (*measure_text)(const char* text, int* out_w, int* out_h);

    // Optional. Return <= 0 to let the HUD infer a height.
    int (*get_line_height)(void);

    void (*draw_rect)(int x, int y, int w, int h, TimerHUDColor color);
    // Optional. If absent, HUD falls back to 1px bars for graph rendering.
    void (*draw_line)(int x1, int y1, int x2, int y2, TimerHUDColor color);
    void (*draw_text)(const char* text, int x, int y, int align_flags, TimerHUDColor color);

    // Optional visual overrides (0 uses HUD defaults).
    int hud_padding;
    int hud_spacing;
    int hud_bg_alpha;
} TimerHUDBackend;

#ifdef __cplusplus
}
#endif

#endif // TIMER_HUD_BACKEND_H
