#include "vk_renderer.h"
#include "kit_ui_window_probe_sdl.h"
#include <assert.h>
#include <stdio.h>
static int capture(void *user,const char *path){return vk_renderer_request_capture(user,path)==VK_SUCCESS;}
int main(void){
 assert(SDL_Init(SDL_INIT_VIDEO)==0);SDL_Window *w=SDL_CreateWindow("CodeWork shared fullscreen qualification",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,800,600,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_VULKAN);assert(w);
 VkRenderer r;VkRendererConfig cfg;vk_renderer_config_set_defaults(&cfg);cfg.enable_validation=VK_TRUE;assert(vk_renderer_init(&r,w,&cfg)==VK_SUCCESS);
 const unsigned char pixels[]={240,80,30,255,30,240,80,255,80,30,240,255,240,240,240,255};
 SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom((void*)pixels,2,2,32,8,SDL_PIXELFORMAT_RGBA32);assert(surface);
 VkRendererTexture texture={0};assert(vk_renderer_upload_sdl_surface_with_filter(&r,surface,&texture,VK_FILTER_NEAREST)==VK_SUCCESS);SDL_FreeSurface(surface);
 KitUiWindowProbe probe;kit_ui_window_probe_init_sdl(&probe,"shared");assert(probe.directory);uint64_t frames=0;KitUiWindowState state={0};int status=0;
 while(!(status=kit_ui_window_probe_tick_sdl(&probe,w,frames,capture,&r))){
  SDL_Event e;while(SDL_PollEvent(&e)){}uint32_t changes;assert(kit_ui_window_refresh_sdl(&state,w,&changes).code==CORE_OK);
  if(!state.presentable){SDL_Delay(8);continue;}
  if(state.drawable_width!=(int)r.context.swapchain.extent.width || state.drawable_height!=(int)r.context.swapchain.extent.height)assert(vk_renderer_recreate_swapchain(&r,w)==VK_SUCCESS);
  VkCommandBuffer cmd;VkFramebuffer fb;assert(vk_renderer_begin_frame(&r,&cmd,&fb,NULL)==VK_SUCCESS);vk_renderer_set_logical_size(&r,state.logical_width,state.logical_height);
  vk_renderer_set_draw_color(&r,.08f,.12f,.18f,1);SDL_Rect bg={0,0,state.logical_width,state.logical_height};vk_renderer_fill_rect(&r,&bg);
  SDL_Rect dst={state.logical_width/4,state.logical_height/4,state.logical_width/2,state.logical_height/2};vk_renderer_set_draw_color(&r,1,1,1,1);vk_renderer_draw_texture(&r,&texture,NULL,&dst);
  assert(vk_renderer_end_frame(&r,cmd)==VK_SUCCESS);++frames;SDL_Delay(8);
 }
 const VkRuntimeCapabilityReport *report=vk_runtime_get_capability_report(&r.context.device->runtime);assert(status==1&&report&&report->validation_enabled&&!report->validation_warning_count&&!report->validation_error_count);
 vk_renderer_texture_destroy(&r,&texture);vk_renderer_shutdown(&r);SDL_DestroyWindow(w);SDL_Quit();puts("shared fullscreen/resize/hide/minimize/restore and retained texture: validation warnings=0 errors=0");
}
