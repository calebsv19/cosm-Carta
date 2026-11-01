/* Native external-text oracle: every glyph texel must survive GPU placement.
 * Caller provides a real font and an isolated output directory. */
#include "kit_render_external_text.h"
#include "vk_renderer.h"
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef assert
#define assert(x) do {if(!(x)){fprintf(stderr,"text pixel proof line %d: %s\n",__LINE__,#x);exit(2);}}while(0)
typedef struct Glyph {SDL_Surface *surface;int x,y;} Glyph;
static unsigned char *capture_pixels(const char *path,int *w,int *h) {
    FILE *f=fopen(path,"rb");assert(f);char line[128];assert(fgets(line,sizeof(line),f)&&!strcmp(line,"P6\n"));assert(fgets(line,sizeof(line),f)&&sscanf(line,"%d %d",w,h)==2);assert(fgets(line,sizeof(line),f)&&atoi(line)==255);
    size_t n=(size_t)*w**h*3;unsigned char *p=malloc(n);assert(p&&fread(p,1,n,f)==n);fclose(f);return p;
}
static size_t oracle(const char *path,Glyph *glyphs,int count) {
    int w,h;unsigned char *rgb=capture_pixels(path,&w,&h);size_t checked=0;
    for(int g=0;g<count;g++) {SDL_Surface *s=glyphs[g].surface;assert(SDL_LockSurface(s)==0);
        for(int y=0;y<s->h;y++)for(int x=0;x<s->w;x++) {
            Uint8 r,b,c,a;Uint32 px=0;memcpy(&px,(char*)s->pixels+y*s->pitch+x*s->format->BytesPerPixel,s->format->BytesPerPixel);SDL_GetRGBA(px,s->format,&r,&b,&c,&a);
            int dx=glyphs[g].x+x,dy=glyphs[g].y+y;assert(dx>=0&&dy>=0&&dx<w&&dy<h);
            int expected[3]={(r*a+127)/255,(b*a+127)/255,(c*a+127)/255};
            for(int ch=0;ch<3;ch++)if(abs(rgb[((size_t)dy*w+dx)*3+ch]-expected[ch])>1) {
                fprintf(stderr,"pixel mismatch glyph%d (%d,%d) channel%d got%d expected%d\n",g,x,y,ch,rgb[((size_t)dy*w+dx)*3+ch],expected[ch]);exit(3);
            }
            checked++;
        }
        SDL_UnlockSurface(s);
    }
    free(rgb);return checked;
}
int main(int argc,char **argv) {
    assert(argc==3);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0&&TTF_Init()==0);
    SDL_Window *window=SDL_CreateWindow("Native text pixel proof",0,0,640,700,SDL_WINDOW_VULKAN|SDL_WINDOW_ALLOW_HIGHDPI);assert(window);
    VkRenderer renderer;VkRendererConfig cfg;vk_renderer_config_set_defaults(&cfg);cfg.enable_validation=VK_TRUE;
    assert(vk_renderer_init(&renderer,window,&cfg)==VK_SUCCESS);
    int width,height;SDL_Vulkan_GetDrawableSize(window,&width,&height);
    const float scales[]={1.0f,1.25f,1.5f,2.0f};const int points[]={12,13,16};
    const char *labels[]={"Rigid result | Il1 012345","Camera / MWmw","Paused | tick 154 / 180"};
    SDL_Color ink={230,240,250,255};size_t checked=0;
    for(int z=0;z<4;z++) {
        float scale=scales[z];vk_renderer_set_logical_size(&renderer,width/scale,height/scale);
        TTF_Font *base[4];Glyph glyphs[4];memset(glyphs,0,sizeof(glyphs));
        for(int i=0;i<4;i++) {
            int pt=i<3?points[i]:13;int raster=(int)lroundf(pt*scale);
            base[i]=TTF_OpenFont(argv[1],pt);assert(base[i]);TTF_SetFontHinting(base[i],TTF_HINTING_LIGHT);TTF_SetFontKerning(base[i],1);
            if(i==0) { /* Existing registration remains logical-metric compatible. */
                kit_render_external_text_register_font_source(base[i],argv[1],pt,pt,1);
                int w,h,W,H;assert(TTF_SizeUTF8(base[i],labels[i],&W,&H)==0);assert(kit_render_external_text_measure_utf8((SDL_Renderer*)&renderer,base[i],labels[i],&w,&h));assert(w==W&&h==H);
            }
            kit_render_external_text_register_font_source_native_pixels(base[i],argv[1],pt,pt,1);
            TTF_Font *cpu=TTF_OpenFont(argv[1],raster);assert(cpu);TTF_SetFontHinting(cpu,TTF_HINTING_LIGHT);TTF_SetFontKerning(cpu,1);
            glyphs[i].surface=i<3?TTF_RenderUTF8_Blended(cpu,labels[i],ink):TTF_RenderUTF8_Blended_Wrapped(cpu,"Wrapped glyphs must keep native pixel dimensions.",ink,(Uint32)lroundf(170*scale));assert(glyphs[i].surface);
            glyphs[i].x=(int)lroundf(17*scale);glyphs[i].y=(int)lroundf((17+i*65)*scale);
            if(i<3) {int w,h;assert(kit_render_external_text_measure_utf8((SDL_Renderer*)&renderer,base[i],labels[i],&w,&h));assert(w==(int)ceilf(glyphs[i].surface->w/scale)&&h==(int)ceilf(glyphs[i].surface->h/scale));}
            TTF_CloseFont(cpu);
        }
        for(int replay=0;replay<2;replay++) {
            char path[4096];snprintf(path,sizeof(path),"%s/scale-%d-%d.ppm",argv[2],z,replay);
            assert(vk_renderer_request_capture(&renderer,path)==VK_SUCCESS);
            VkCommandBuffer cmd;VkFramebuffer fb;VkExtent2D extent;assert(vk_renderer_begin_frame(&renderer,&cmd,&fb,&extent)==VK_SUCCESS);
            vk_renderer_set_draw_color(&renderer,0,0,0,1);SDL_Rect bg={0,0,(int)ceilf(width/scale),(int)ceilf(height/scale)};vk_renderer_fill_rect(&renderer,&bg);
            if(!replay) {
                SDL_Rect resized={17,17,500,24};
                assert(kit_render_external_text_draw_utf8((SDL_Renderer*)&renderer,base[0],labels[0],ink,&resized));
                assert(resized.w==500&&resized.h==24);
                vk_renderer_set_draw_color(&renderer,0,0,0,1);vk_renderer_fill_rect(&renderer,&bg);
            }
            for(int i=0;i<4;i++) {
                if(i<3)assert(kit_render_external_text_draw_utf8_at((SDL_Renderer*)&renderer,base[i],labels[i],17,17+i*65,ink));
                else {SDL_Rect dst={17,17+i*65,0,0};assert(kit_render_external_text_draw_utf8_wrapped((SDL_Renderer*)&renderer,base[i],"Wrapped glyphs must keep native pixel dimensions.",170,ink,&dst));assert(dst.w==(int)ceilf(glyphs[i].surface->w/scale)&&dst.h==(int)ceilf(glyphs[i].surface->h/scale));}
            }
            assert(vk_renderer_end_frame(&renderer,cmd)==VK_SUCCESS);checked+=oracle(path,glyphs,4);
        }
        for(int i=0;i<4;i++){SDL_FreeSurface(glyphs[i].surface);kit_render_external_text_unregister_font_source(base[i]);TTF_CloseFont(base[i]);}
    }
    const VkRuntimeCapabilityReport *report=vk_runtime_get_capability_report(&renderer.context.device->runtime);assert(report&&report->validation_enabled&&!report->validation_warning_count&&!report->validation_error_count);
    printf("native text oracle PASS: %zu glyph pixels, 4 scales, cached/uncached/wrapped, legacy metrics, validation clean; drawable=%dx%d\n",checked,width,height);
    kit_render_external_text_reset_renderer((SDL_Renderer*)&renderer);kit_render_external_text_reset_font_system();vk_renderer_shutdown(&renderer);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return 0;
}
