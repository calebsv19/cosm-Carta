#include "ui/rollout.h"
#include "app/app_internal.h"
#if defined(MAPFORGE_HAVE_VK)
#include "vk_renderer.h"
#endif
// The opt-in shared probe runs through the actual Carta event/update/render loop.
static int capture(void *user,const char *path) {
    AppState *app=user;
#if defined(MAPFORGE_HAVE_VK)
    if(app->renderer.backend==RENDERER_BACKEND_VULKAN && app->renderer.vk)
        return vk_renderer_request_capture(app->renderer.vk,path)==VK_SUCCESS;
#endif
    if(!app->renderer.sdl)return 0;
    int w,h;if(SDL_GetRendererOutputSize(app->renderer.sdl,&w,&h))return 0;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if(!surface)return 0;
    int ok=SDL_RenderReadPixels(app->renderer.sdl,NULL,surface->format->format,surface->pixels,surface->pitch)==0 && SDL_SaveBMP(surface,path)==0;
    SDL_FreeSurface(surface);return ok;
}
int carta_window_probe_tick(AppState *app) {
    CartaUiRollout *r=&app->ui_state_bridge.rollout;
    int status=kit_ui_window_probe_tick_sdl(&r->probe,app->window,r->presented_frames,capture,app);
    if(r->probe.directory)app->runtime_redraw_requested=true;
    if(status)app->ui_state_bridge.input.quit=true;
    return status;
}
