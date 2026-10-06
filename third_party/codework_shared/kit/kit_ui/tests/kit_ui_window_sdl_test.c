#include "kit_ui_window_sdl.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_Window *w=SDL_CreateWindow("window metrics",0,0,800,600,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);assert(w);
 KitUiWindowState s={0};uint32_t bits;
 assert(kit_ui_window_refresh_sdl(&s,w,&bits).code==CORE_OK && s.presentable && bits&KIT_UI_WINDOW_GEOMETRY);
 uint64_t generation=s.generation;assert(kit_ui_window_refresh_sdl(&s,w,&bits).code==CORE_OK && !bits && generation==s.generation);
 int x,y;assert(kit_ui_window_map_point_sdl(&s,1600,1200,400,300,&x,&y)&&x==800&&y==600);
 assert(kit_ui_window_map_point_sdl(&s,800,600,-4,602,&x,&y)&&x==-4&&y==602);
 s.logical_width=2500;s.logical_height=720;assert(kit_ui_window_map_point_sdl(&s,4096,1440,1250,360,&x,&y)&&x==2048&&y==720);
 SDL_SetWindowSize(w,1024,768);assert(kit_ui_window_refresh_sdl(&s,w,&bits).code==CORE_OK && bits&KIT_UI_WINDOW_GEOMETRY);
 SDL_HideWindow(w);assert(kit_ui_window_refresh_sdl(&s,w,&bits).code==CORE_OK && !s.presentable && bits&KIT_UI_WINDOW_VISIBILITY);
 SDL_ShowWindow(w);assert(kit_ui_window_refresh_sdl(&s,w,&bits).code==CORE_OK && s.presentable);
 Uint8 events[]={SDL_WINDOWEVENT_RESIZED,SDL_WINDOWEVENT_SIZE_CHANGED,SDL_WINDOWEVENT_DISPLAY_CHANGED,SDL_WINDOWEVENT_MOVED,SDL_WINDOWEVENT_MINIMIZED,SDL_WINDOWEVENT_MAXIMIZED,SDL_WINDOWEVENT_RESTORED,SDL_WINDOWEVENT_HIDDEN,SDL_WINDOWEVENT_SHOWN,SDL_WINDOWEVENT_FOCUS_LOST};
 for(unsigned i=0;i<sizeof(events);i++){SDL_Event e={0};e.type=SDL_WINDOWEVENT;e.window.event=events[i];assert(kit_ui_window_event_invalidates_sdl(&e));}
 SDL_Event e={0};e.type=SDL_MOUSEMOTION;assert(!kit_ui_window_event_invalidates_sdl(&e));
 SDL_DestroyWindow(w);SDL_Quit();puts("window state: dimensions/generation, explicit 1x/2x mapping, hide/show suspension and lifecycle invalidation pass");
}
