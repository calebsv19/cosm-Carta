#include "ui/text_draw.h"
#include "kit_render_native_font.h"
#include "vk_renderer.h"
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Keeps each CPU raster and the physical clip used by its production draw. */
typedef struct Glyph {SDL_Surface *s;int x,y,left,right,top,bottom;} Glyph;
/* Compare the complete GPU frame with CPU glyph blending, including clipped-away pixels. */
static size_t compare(const char *path,Glyph *g,int count) {
    FILE *f=fopen(path,"rb");assert(f);char line[128];int w,h;
    assert(fgets(line,sizeof(line),f)&&!strcmp(line,"P6\n"));assert(fgets(line,sizeof(line),f)&&sscanf(line,"%d %d",&w,&h)==2);assert(fgets(line,sizeof(line),f)&&atoi(line)==255);
    size_t n=(size_t)w*h*3;unsigned char *got=malloc(n),*expected=calloc(n,1);assert(got&&expected&&fread(got,1,n,f)==n);fclose(f);
    for(int i=0;i<count;i++)for(int y=0;y<g[i].s->h;y++)for(int x=0;x<g[i].s->w;x++) {
        int dx=g[i].x+x,dy=g[i].y+y;
        if(dx<0||dy<0||dx>=w||dy>=h||dx<g[i].left||dx>=g[i].right||dy<g[i].top||dy>=g[i].bottom)continue;
        Uint32 p=0;Uint8 r,b,c,a;SDL_Surface *s=g[i].s;memcpy(&p,(char*)s->pixels+y*s->pitch+x*s->format->BytesPerPixel,s->format->BytesPerPixel);SDL_GetRGBA(p,s->format,&r,&b,&c,&a);
        unsigned char *e=expected+((size_t)dy*w+dx)*3;e[0]=(r*a+127)/255;e[1]=(b*a+127)/255;e[2]=(c*a+127)/255;
    }
    for(size_t i=0;i<n;i++)if(abs(got[i]-expected[i])>1){fprintf(stderr,"native text mismatch byte%zu got%d expected%d\n",i,got[i],expected[i]);exit(3);}
    free(got);free(expected);return (size_t)w*h;
}
/* Adapt production draw signatures without bypassing MapForge dispatch. */
static int map_at(Renderer *r,TTF_Font *f,const char *s,int x,int y,SDL_Color ink) {
    SDL_Rect dst={x,y,0,0};return map_forge_text_draw_utf8(r,f,s,ink,&dst);
}
/* Exercise production clipping with the shared native-pixel text cache. */
static int map_clip(Renderer *r,TTF_Font *f,const char *s,int x,int y,SDL_Color ink,int width) {
    SDL_Rect dst={x,y,0,0};return map_forge_text_draw_utf8_clipped(r,f,s,ink,&dst,width);
}
/* Compare actual native drawables with independently rasterized CPU glyphs. */
int main(int argc,char **argv) {
    assert(argc==3 && SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0 && TTF_Init()==0);
    SDL_Window *window=SDL_CreateWindow("Carta native text pixels",0,0,640,840,SDL_WINDOW_VULKAN|SDL_WINDOW_ALLOW_HIGHDPI);assert(window);
    VkRenderer renderer;VkRendererConfig cfg;vk_renderer_config_set_defaults(&cfg);cfg.enable_validation=VK_TRUE;assert(vk_renderer_init(&renderer,window,&cfg)==VK_SUCCESS);
    Renderer wrapped={0};wrapped.backend=RENDERER_BACKEND_VULKAN;wrapped.vk=&renderer;wrapped.window=window;
    int w,h;SDL_Vulkan_GetDrawableSize(window,&w,&h);const float scales[]={1,1.25f,1.5f,2};size_t checked=0;
    char long_text[300];memset(long_text,'W',sizeof(long_text)-1);long_text[sizeof(long_text)-1]=0;
    const char *labels[]={"Transport | Il1 MWmw 012345","Clipped timeline label",long_text,"Nested clip | pin name","水 日本語 🙂","🙂 café"};const int points[]={13,12,13,16,13,14};SDL_Color ink={230,240,250,255};
    for(int z=0;z<4;z++) {
        float scale=scales[z];vk_renderer_set_logical_size(&renderer,w/scale,h/scale);map_forge_text_bind_renderer(&wrapped);
        TTF_Font *base[6];Glyph glyphs[6];
        for(int i=0;i<6;i++) {
            base[i]=TTF_OpenFont(argv[1],points[i]);assert(base[i]);TTF_SetFontHinting(base[i],TTF_HINTING_LIGHT);TTF_SetFontKerning(base[i],1);
            map_forge_text_register_font_source(base[i],argv[1],points[i],points[i],1);
            TTF_Font *cpu=TTF_OpenFont(argv[1],(int)lroundf(points[i]*scale));assert(cpu);TTF_SetFontHinting(cpu,TTF_HINTING_LIGHT);TTF_SetFontKerning(cpu,1);
            SDL_Surface *s=i<4?TTF_RenderUTF8_Blended(cpu,labels[i],ink):kit_render_native_font_rasterize(argv[1],points[i],labels[i],ink,scale);assert(s);TTF_CloseFont(cpu);
            int x=i==3?60:17,y=17+i*70;
            glyphs[i]=(Glyph){s,(int)lroundf(x*scale),(int)lroundf(y*scale),0,w,0,h};
            if(i>0 && i<4) {glyphs[i].left=(int)floorf(x*scale);glyphs[i].right=(int)ceilf((x+(i==1?75:135))*scale);}
            if(i==3) {glyphs[i].left=(int)floorf(85*scale);glyphs[i].right=(int)ceilf(165*scale);}
            int mw,mh;assert(map_forge_text_measure_utf8(NULL,base[i],labels[i],&mw,&mh));assert(mw==(int)ceilf(s->w/scale)&&mh==(int)ceilf(s->h/scale));
        }
        for(int replay=0;replay<3;replay++) {
            char path[4096];snprintf(path,sizeof(path),"%s/scale-%d-%d.ppm",argv[2],z,replay);assert(vk_renderer_request_capture(&renderer,path)==VK_SUCCESS);
            VkCommandBuffer cmd;VkFramebuffer fb;VkExtent2D extent;assert(vk_renderer_begin_frame(&renderer,&cmd,&fb,&extent)==VK_SUCCESS);
            SDL_Rect bg={0,0,(int)ceilf(w/scale),(int)ceilf(h/scale)};vk_renderer_set_draw_color(&renderer,0,0,0,1);vk_renderer_fill_rect(&renderer,&bg);
            if(replay==2) {
                for(int i=0;i<640;i++){char text[80];snprintf(text,sizeof(text),"Dense playback title %d",i);assert(map_clip(&wrapped,base[0],text,5,(int)(h/scale)-20,ink,120));}
                for(int i=0;i<80;i++){char text[80];snprintf(text,sizeof(text),"日本語 fallback %d",i);assert(map_clip(&wrapped,base[4],text,5,(int)(h/scale)-20,ink,120));}
                vk_renderer_set_draw_color(&renderer,0,0,0,1);vk_renderer_fill_rect(&renderer,&bg);
            }
            assert(map_at(&wrapped,base[0],labels[0],17,17,ink));
            assert(map_clip(&wrapped,base[1],labels[1],17,87,ink,75));
            assert(map_clip(&wrapped,base[2],labels[2],17,157,ink,135));
            SDL_Rect outer={85,210,80,80};vk_renderer_set_clip_rect(&renderer,&outer);
            assert(map_clip(&wrapped,base[3],labels[3],60,227,ink,135));
            SDL_Rect restored;vk_renderer_get_clip_rect(&renderer,&restored);assert(vk_renderer_is_clip_enabled(&renderer)&&!memcmp(&outer,&restored,sizeof(outer)));vk_renderer_set_clip_rect(&renderer,NULL);
            assert(map_at(&wrapped,base[4],labels[4],17,297,ink));
            assert(map_at(&wrapped,base[5],labels[5],17,367,ink));
            assert(vk_renderer_end_frame(&renderer,cmd)==VK_SUCCESS);checked+=compare(path,glyphs,6);
        }
        for(int i=0;i<6;i++){SDL_FreeSurface(glyphs[i].s);map_forge_text_unregister_font_source(base[i]);TTF_CloseFont(base[i]);}
    }
    const VkRuntimeCapabilityReport *report=vk_runtime_get_capability_report(&renderer.context.device->runtime);assert(report&&report->validation_enabled&&!report->validation_warning_count&&!report->validation_error_count);
    printf("Carta pixel oracle PASS: %zu full-frame pixels; 4 scales; normal, clipped, long, nested, cold/replay/640-cache-eviction; drawable=%dx%d; validation clean\n",checked,w,h);
    map_forge_text_reset_renderer(&wrapped);vk_renderer_shutdown(&renderer);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return 0;
}
