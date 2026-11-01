#include "ui/font_bridge.h"
#include "ui/text_draw.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

/* Retains a font's requested layout scale independently of rounded point size. */
typedef struct MapForgeFontEntry { int point_size; float scale; TTF_Font *font; } MapForgeFontEntry;
/* Owns one complete active or pinned font working set. */
typedef struct MapForgeFontBank { char path[256]; int base; int count; int scale_count; float scales[64]; MapForgeFontEntry entries[16]; } MapForgeFontBank;
static MapForgeFontBank active = { .path="assets/fonts/Montserrat-Regular.ttf", .base=10 };
static MapForgeFontBank baseline;
static bool preview, pinned;

/* Resolve one requested UI scale under the existing point-size policy. */
static int point_size(int base, float scale) {
    if (!isfinite(scale) || scale <= 0) scale=1;
    float pixels=base*scale;
    if (!isfinite(pixels) || pixels>512) return 0;
    int size=(int)lroundf(pixels);return size<4?4:size;
}
/* Release only this bank's fonts and their registered text resources. */
static void release(MapForgeFontBank *bank, bool registered) {
    for(int i=0;i<bank->count;i++) if(bank->entries[i].font) {
        if(registered)map_forge_text_unregister_font_source(bank->entries[i].font);
        TTF_CloseFont(bank->entries[i].font);
    }
    memset(bank,0,sizeof(*bank));
}
/* Retain distinct requested scales even when they currently round to the same size. */
static bool track_scale(MapForgeFontBank *bank,float scale) {
    if(!isfinite(scale)||scale<=0)scale=1;
    for(int i=0;i<bank->scale_count;i++)if(bank->scales[i]==scale)return true;
    if(bank->scale_count==64)return false;
    bank->scales[bank->scale_count++]=scale;return true;
}
/* Load into a candidate before evicting any last-good font. */
static bool load(MapForgeFontBank *bank,float scale) {
    int size=point_size(bank->base,scale);
    if(size<1 || !track_scale(bank,scale))return false;
    for(int i=0;i<bank->count;i++)if(bank->entries[i].point_size==size)return true;
    if(bank->count==16)return false;
    TTF_Font *font=TTF_OpenFont(bank->path,size);if(!font)return false;
    TTF_SetFontStyle(font,TTF_STYLE_NORMAL);TTF_SetFontHinting(font,TTF_HINTING_LIGHT);TTF_SetFontKerning(font,1);
    bank->entries[bank->count++]=(MapForgeFontEntry){size,scale,font};return true;
}
/* Register a successfully loaded bank only after old registrations are released. */
static void register_bank(MapForgeFontBank *bank) {
    for(int i=0;i<bank->count;i++)map_forge_text_register_font_source(bank->entries[i].font,
        bank->path,bank->entries[i].point_size,bank->entries[i].point_size,1);
}
/* Prepare the base and every currently used scale, then exchange ownership atomically. */
bool map_forge_font_bridge_set_active(const char *path,int base) {
    if(!path || !path[0] || strlen(path)>=sizeof(active.path) || base<=0 || base>512)return false;
    if(active.count && active.base==base && !strcmp(active.path,path))return true;
    MapForgeFontBank next={0};strcpy(next.path,path);next.base=base;
    if(!load(&next,1))return false;
    for(int i=0;i<active.scale_count;i++)if(!load(&next,active.scales[i])) {release(&next,false);return false;}
    if(preview && !pinned) {baseline=active;pinned=true;}
    else release(&active,true);
    active=next;register_bank(&active);return true;
}
/* Pin lazily on the first accepted preview replacement. */
void map_forge_font_bridge_preview_begin(void) {preview=true;}
/* Restore the exact entry handles on Cancel, or release the pin on Apply. */
void map_forge_font_bridge_preview_end(bool accept) {
    if(pinned) {
        if(accept)release(&baseline,true);
        else {release(&active,true);active=baseline;memset(&baseline,0,sizeof(baseline));}
    }
    preview=false;pinned=false;
}
/* Release both ownership banks during app shutdown. */
void map_forge_font_bridge_shutdown(Renderer *renderer) {
    map_forge_font_bridge_reset_renderer(renderer);
    release(&active,true);release(&baseline,true);preview=false;pinned=false;
    strcpy(active.path,"assets/fonts/Montserrat-Regular.ttf");active.base=10;
}
/* Acquire a font without destroying cached last-good resources on load failure. */
bool map_forge_font_bridge_acquire(float scale,MapForgeResolvedFont *out) {
    if(!out)return false;
    memset(out,0,sizeof(*out));
    int size=point_size(active.base,scale);
    if(size<1)return false;
    for(int i=0;i<active.count;i++)if(active.entries[i].point_size==size) {
        if(!track_scale(&active,scale))return false;
        *out=(MapForgeResolvedFont){active.entries[i].font,active.path,size,1};return true;
    }
    if(!track_scale(&active,scale))return false;
    /* Build the new handle before evicting the oldest slot. */
    TTF_Font *font=TTF_OpenFont(active.path,size);if(!font)return false;
    TTF_SetFontStyle(font,TTF_STYLE_NORMAL);TTF_SetFontHinting(font,TTF_HINTING_LIGHT);TTF_SetFontKerning(font,1);
    if(active.count==16) {
        map_forge_text_unregister_font_source(active.entries[0].font);TTF_CloseFont(active.entries[0].font);
        memmove(active.entries,active.entries+1,15*sizeof(active.entries[0]));--active.count;
    }
    active.entries[active.count++]=(MapForgeFontEntry){size,scale,font};
    map_forge_text_register_font_source(font,active.path,size,size,1);
    *out=(MapForgeResolvedFont){font,active.path,size,1};return true;
}

/* Retire renderer-local resources before its backend is destroyed. */
void map_forge_font_bridge_reset_renderer(Renderer *renderer) {
    map_forge_text_reset_renderer(renderer);
}
