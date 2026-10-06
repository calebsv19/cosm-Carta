#include "kit_ui_sdl.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct TextProbe {
    int calls;
    int scale;
    int width;
    int height;
    int x;
    int y;
    SDL_Rect clip;
    KitRenderColor color;
    const char *label;
} TextProbe;

static int measure(void *user, const char *text, int scale, int *w, int *h) {
    TextProbe *probe = user;
    assert(text == probe->label);
    assert(scale == probe->scale);
    *w = probe->width;
    *h = probe->height;
    return 1;
}

static int line_height(void *user, int scale) {
    TextProbe *probe = user;
    assert(scale == probe->scale);
    return probe->height;
}

static void draw_text(void *user, SDL_Renderer *renderer, const SDL_Rect *clip,
                       int x, int y, const char *text, int scale, KitRenderColor color) {
    TextProbe *probe = user;
    assert(renderer && text == probe->label && scale == probe->scale);
    probe->calls++;
    probe->x = x;
    probe->y = y;
    probe->clip = *clip;
    probe->color = color;
}

static KitRenderColor pixel(SDL_Surface *surface, int x, int y) {
    KitRenderColor color;
    Uint32 value = *(Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch + x * 4);
    SDL_GetRGBA(value, surface->format, &color.r, &color.g, &color.b, &color.a);
    return color;
}

static void expect_color(KitRenderColor actual, CoreThemeColor expected) {
    assert(actual.r == expected.r && actual.g == expected.g &&
           actual.b == expected.b && actual.a == expected.a);
}

static void reset(SDL_Renderer *renderer, TextProbe *probe) {
    SDL_SetRenderDrawColor(renderer, 3, 5, 9, 255);
    assert(SDL_RenderClear(renderer) == 0);
    probe->calls = 0;
}

static void run_scale(int scale) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 160 * scale, 70 * scale,
                                                          32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    SDL_Rect rect = {10 * scale, 10 * scale, 100 * scale, 30 * scale};
    KitUiButtonTheme theme = {
        .idle_fill = {30, 40, 50, 255}, .selected_fill = {65, 75, 85, 255},
        .hover_fill = {100, 110, 120, 255}, .positive_fill = {40, 180, 50, 255},
        .outline_idle = {130, 140, 150, 255}, .outline_highlight = {190, 200, 210, 255},
        .text_primary = {220, 230, 240, 255}, .text_muted = {80, 90, 100, 255}
    };
    KitUiButtonAppearance appearance;
    KitUiButtonSpec spec;
    KitUiButtonStyle style;
    TextProbe probe = {.scale = scale, .width = 42 * scale, .height = 12 * scale,
                        .label = "Caption"};
    KitUiSdlTextApi text = {&probe, scale, 4 * scale, measure, line_height, draw_text};
    assert(surface && renderer);
    assert(kit_ui_button_appearance_preset(KIT_UI_BUTTON_APPEARANCE_COMPACT_ROUNDED,
                                           &appearance));
    appearance.corner_radius *= scale;
    appearance.border_thickness *= scale;
    kit_ui_button_spec_init(&spec, probe.label);
    reset(renderer, &probe);
    kit_ui_sdl_draw_button_spec_appearance(renderer, &rect, &spec, &theme, &appearance, &text);
    SDL_RenderPresent(renderer);
    expect_color(pixel(surface, rect.x, rect.y), (CoreThemeColor){3, 5, 9, 255});
    expect_color(pixel(surface, rect.x + rect.w / 2, rect.y), theme.outline_idle);
    expect_color(pixel(surface, rect.x + rect.w / 2, rect.y + rect.h / 2), theme.idle_fill);
    assert(probe.calls == 1 && probe.x == rect.x + (rect.w - probe.width) / 2);
    assert(probe.y == rect.y + (rect.h - probe.height) / 2);
    assert(probe.clip.x == rect.x + 4 * scale && probe.clip.w == rect.w - 8 * scale);
    expect_color(probe.color, theme.text_primary);

    /* Every state uses the shared resolver, including disabled precedence. */
    for (int state = 0; state < 5; ++state) {
        kit_ui_button_state_init(&spec.state);
        spec.state.hovered = state == 0 || state == 4;
        spec.state.selected = state == 1 || state == 4;
        spec.state.pressed = state == 2 || state == 4;
        spec.state.focused = state == 3 || state == 4;
        spec.state.disabled = state == 4;
        assert(kit_ui_button_style_resolve(&theme, &spec, &style));
        reset(renderer, &probe);
        kit_ui_sdl_draw_button_spec_appearance(renderer, &rect, &spec, &theme, &appearance, &text);
        SDL_RenderPresent(renderer);
        expect_color(pixel(surface, rect.x + rect.w / 2, rect.y), style.outline);
        expect_color(pixel(surface, rect.x + rect.w / 2, rect.y + rect.h / 2), style.fill);
        expect_color(probe.color, style.text);
    }

    /* Long captions clamp to a positive clip; narrow controls skip text. */
    probe.width = rect.w * 2;
    reset(renderer, &probe);
    kit_ui_sdl_draw_button_spec_appearance(renderer, &rect, &spec, &theme, &appearance, &text);
    assert(probe.calls == 1 && probe.x == probe.clip.x);
    SDL_Rect narrow = {rect.x, rect.y, 6 * scale, rect.h};
    reset(renderer, &probe);
    kit_ui_sdl_draw_button_spec_appearance(renderer, &narrow, &spec, &theme, &appearance, &text);
    assert(probe.calls == 0);

    /* Oversized radii clamp, and the caller's existing renderer clip survives. */
    appearance.corner_radius = 9999.0f;
    SDL_Rect clip = {rect.x + rect.w / 2, rect.y, rect.w / 2, rect.h};
    reset(renderer, &probe);
    SDL_RenderSetClipRect(renderer, &clip);
    kit_ui_sdl_draw_button_spec_appearance(renderer, &rect, &spec, &theme, &appearance, NULL);
    SDL_RenderPresent(renderer);
    expect_color(pixel(surface, rect.x + rect.w / 4, rect.y + rect.h / 2),
                   (CoreThemeColor){3, 5, 9, 255});
    expect_color(pixel(surface, rect.x + rect.w * 3 / 4, rect.y + rect.h / 2), style.fill);
    SDL_Rect after;
    SDL_RenderGetClipRect(renderer, &after);
    assert(SDL_RenderIsClipEnabled(renderer) && after.x == clip.x && after.w == clip.w);
    SDL_RenderSetClipRect(renderer, NULL);

    /* Borderless appearance and invalid input do not produce stray outlines. */
    appearance.corner_radius = 0;
    appearance.border_thickness = 0;
    reset(renderer, &probe);
    kit_ui_sdl_draw_button_spec_appearance(renderer, &rect, &spec, &theme, &appearance, NULL);
    SDL_RenderPresent(renderer);
    expect_color(pixel(surface, rect.x, rect.y), style.fill);
    appearance.corner_radius = NAN;
    reset(renderer, &probe);
    kit_ui_sdl_draw_button_spec_appearance(renderer, &rect, &spec, &theme, &appearance, &text);
    SDL_RenderPresent(renderer);
    expect_color(pixel(surface, rect.x + rect.w / 2, rect.y + rect.h / 2),
                   (CoreThemeColor){3, 5, 9, 255});
    assert(probe.calls == 0);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
}

int main(void) {
    assert(SDL_Init(0) == 0);
    run_scale(1);
    run_scale(2);
    SDL_Quit();
    puts("kit_ui SDL appearance: 1x/2x pixels, state, captions, clipping and invalid inputs passed");
    return 0;
}
