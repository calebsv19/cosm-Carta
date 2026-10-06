#include "kit_ui_window_sdl.h"
#include "kit_ui_interaction_sdl.h"
#include <string.h>

int kit_ui_interaction_event_from_sdl(const SDL_Event *event,
                                     KitUiInteractionEvent *out_event) {
    if (!event || !out_event) return 0;
    memset(out_event, 0, sizeof(*out_event));
    switch (event->type) {
        case SDL_MOUSEMOTION:
            out_event->type = KIT_UI_INTERACTION_POINTER_MOVE;
            out_event->x = (float)event->motion.x; out_event->y = (float)event->motion.y;
            return 1;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (event->button.button != SDL_BUTTON_LEFT) return 0;
            out_event->type = event->type == SDL_MOUSEBUTTONDOWN ? KIT_UI_INTERACTION_POINTER_DOWN : KIT_UI_INTERACTION_POINTER_UP;
            out_event->x = (float)event->button.x; out_event->y = (float)event->button.y;
            return 1;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            out_event->type = event->type == SDL_KEYDOWN ? KIT_UI_INTERACTION_KEY_DOWN : KIT_UI_INTERACTION_KEY_UP;
            switch (event->key.keysym.sym) {
                case SDLK_TAB: out_event->key = KIT_UI_INTERACTION_KEY_TAB; break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: out_event->key = KIT_UI_INTERACTION_KEY_ENTER; break;
                case SDLK_SPACE: out_event->key = KIT_UI_INTERACTION_KEY_SPACE; break;
                case SDLK_ESCAPE: out_event->key = KIT_UI_INTERACTION_KEY_ESCAPE; break;
                default: return 0;
            }
            if (event->key.keysym.mod & KMOD_SHIFT) out_event->modifiers |= KIT_UI_INTERACTION_MOD_SHIFT;
            if (event->key.keysym.mod & KMOD_CTRL) out_event->modifiers |= KIT_UI_INTERACTION_MOD_CTRL;
            if (event->key.keysym.mod & KMOD_ALT) out_event->modifiers |= KIT_UI_INTERACTION_MOD_ALT;
            if (event->key.keysym.mod & KMOD_GUI) out_event->modifiers |= KIT_UI_INTERACTION_MOD_GUI;
            out_event->repeat = event->key.repeat != 0;
            return 1;
        case SDL_WINDOWEVENT:
            if (!kit_ui_window_event_invalidates_sdl(event)) return 0;
            out_event->type = KIT_UI_INTERACTION_CANCEL;
            return 1;
        case SDL_QUIT:
            out_event->type = KIT_UI_INTERACTION_CANCEL;
            return 1;
        default: return 0;
    }
}

void kit_ui_interaction_sdl_draw_focus(SDL_Renderer *renderer,
                                       const KitUiInteractionContext *ctx,
                                       const KitUiInteractionControl *controls,
                                       uint32_t count,
                                       KitRenderColor color) {
    KitRenderRect marker;
    if (!renderer || !kit_ui_interaction_focus_marker(ctx, controls, count, &marker)) return;
    Uint8 r, g, b, a;
    SDL_BlendMode blend;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_GetRenderDrawBlendMode(renderer, &blend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = {marker.x, marker.y, marker.width, marker.height};
    SDL_RenderFillRectF(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_SetRenderDrawBlendMode(renderer, blend);
}
