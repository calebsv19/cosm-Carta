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
    SDL_Surface *cpu=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);assert(cpu);
    SDL_FillRect(cpu,NULL,SDL_MapRGBA(cpu->format,0,0,0,255));
    for(int i=0;i<count;i++) {
        SDL_Rect clip={g[i].left,g[i].top,g[i].right-g[i].left,g[i].bottom-g[i].top};
        SDL_Rect dst={g[i].x,g[i].y,0,0};SDL_SetClipRect(cpu,&clip);
        assert(SDL_BlitSurface(g[i].s,NULL,cpu,&dst)==0);
    }
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
        Uint32 p;Uint8 rr,gg,bb,aa;memcpy(&p,(char*)cpu->pixels+y*cpu->pitch+x*4,4);SDL_GetRGBA(p,cpu->format,&rr,&gg,&bb,&aa);
        unsigned char *e=expected+((size_t)y*w+x)*3;e[0]=rr;e[1]=gg;e[2]=bb;
    }
    SDL_FreeSurface(cpu);
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
/* Compare production SDL drawing against independent native glyph rasters. */
int main(int argc,char **argv) {
    assert(argc==3 && SDL_Init(SDL_INIT_TIMER)==0 && TTF_Init()==0);
    SDL_Surface *frame=SDL_CreateRGBSurfaceWithFormat(0,1280,1680,32,SDL_PIXELFORMAT_RGBA32);assert(frame);
    SDL_Renderer *sdl=SDL_CreateSoftwareRenderer(frame);assert(sdl);
    Renderer wrapped={0};wrapped.backend=RENDERER_BACKEND_SDL;wrapped.sdl=sdl;
    const float scales[]={1,1.25f,1.5f,2,1.25f};const float vertical[]={1,1.25f,1.5f,2,1.5f};size_t checked=0;
    char long_text[300];memset(long_text,'W',sizeof(long_text)-1);long_text[sizeof(long_text)-1]=0;
    const char *labels[]={"Transport | Il1 MWmw 012345","Clipped timeline label",long_text,"Nested clip | pin name","水 日本語 🙂","🙂 café"};const int points[]={13,12,13,16,13,14};SDL_Color ink={230,240,250,255};
    for(int z=0;z<5;z++) {
        float sx=scales[z],sy=vertical[z],scale=fmaxf(sx,sy);SDL_RenderSetScale(sdl,sx,sy);
        SDL_Rect viewport={z==4?7:0,z==4?9:0,700,500};
        SDL_RenderSetViewport(sdl,z==4?&viewport:NULL);SDL_RenderGetViewport(sdl,&viewport);map_forge_text_bind_renderer(&wrapped);
        TTF_Font *base[6];Glyph glyphs[6];
        for(int i=0;i<6;i++) {
            base[i]=TTF_OpenFont(argv[1],points[i]);assert(base[i]);TTF_SetFontHinting(base[i],TTF_HINTING_LIGHT);TTF_SetFontKerning(base[i],1);
            map_forge_text_register_font_source(base[i],argv[1],points[i],points[i],1);
            TTF_Font *cpu=TTF_OpenFont(argv[1],(int)lroundf(points[i]*scale));assert(cpu);TTF_SetFontHinting(cpu,TTF_HINTING_LIGHT);TTF_SetFontKerning(cpu,1);
            SDL_Surface *s=i<4?TTF_RenderUTF8_Blended(cpu,labels[i],ink):kit_render_native_font_rasterize(argv[1],points[i],labels[i],ink,scale);assert(s);TTF_CloseFont(cpu);
            int x=i==3?60:17,y=17+i*70;
            glyphs[i]=(Glyph){s,(int)lroundf((x+viewport.x)*sx),(int)lroundf((y+viewport.y)*sy),0,1280,0,1680};
            if(i>0 && i<4) {glyphs[i].left=(int)((x+viewport.x)*sx);glyphs[i].right=glyphs[i].left+(int)((i==1?75:135)*sx);}
            if(i==3) {glyphs[i].left=(int)((85+viewport.x)*sx);glyphs[i].right=glyphs[i].left+(int)(80*sx);}
            int mw,mh;assert(map_forge_text_measure_utf8(NULL,base[i],labels[i],&mw,&mh));assert(mw==(int)ceilf(s->w/sx)&&mh==(int)ceilf(s->h/sy));
        }
        for(int replay=0;replay<3;replay++) {
            SDL_SetRenderDrawColor(sdl,0,0,0,255);SDL_RenderClear(sdl);
            assert(map_at(&wrapped,base[0],labels[0],17,17,ink));
            assert(map_clip(&wrapped,base[1],labels[1],17,87,ink,75));
            assert(map_clip(&wrapped,base[2],labels[2],17,157,ink,135));
            SDL_Rect outer={85,210,80,80};SDL_RenderSetClipRect(sdl,&outer);
            assert(map_clip(&wrapped,base[3],labels[3],60,227,ink,135));
            SDL_Rect restored;SDL_RenderGetClipRect(sdl,&restored);assert(SDL_RenderIsClipEnabled(sdl)&&!memcmp(&outer,&restored,sizeof(outer)));SDL_RenderSetClipRect(sdl,NULL);
            assert(map_at(&wrapped,base[4],labels[4],17,297,ink));assert(map_at(&wrapped,base[5],labels[5],17,367,ink));SDL_RenderPresent(sdl);
            char path[4096];snprintf(path,sizeof(path),"%s/sdl-%d-%d.ppm",argv[2],z,replay);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",frame->w,frame->h);
            for(int y=0;y<frame->h;y++)for(int x=0;x<frame->w;x++){Uint32 p;Uint8 rr,gg,bb,aa;memcpy(&p,(char*)frame->pixels+y*frame->pitch+x*4,4);SDL_GetRGBA(p,frame->format,&rr,&gg,&bb,&aa);fputc(rr,f);fputc(gg,f);fputc(bb,f);}fclose(f);checked+=compare(path,glyphs,6);
        }
        for(int i=0;i<6;i++){SDL_FreeSurface(glyphs[i].s);map_forge_text_unregister_font_source(base[i]);TTF_CloseFont(base[i]);}
    }
    printf("Carta SDL pixel oracle PASS: %zu full-frame pixels; four scales plus asymmetric scale/nonzero viewport; normal, clipped, long, nested, CJK/emoji, replay\n",checked);
    map_forge_text_reset_renderer(&wrapped);SDL_DestroyRenderer(sdl);SDL_FreeSurface(frame);TTF_Quit();SDL_Quit();return 0;
}
