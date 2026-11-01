#include "kit_render_native_font.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#endif
int kit_render_native_font_needs_fallback(TTF_Font* font, const char* utf8) {
    if (!font || !utf8) return 0;
    const unsigned char* p = (const unsigned char*)utf8;
    while (*p) {
        uint32_t scalar = *p++;
        unsigned count = 0;
        if (scalar >= 0xc2 && scalar <= 0xdf) { scalar &= 0x1f; count = 1; }
        else if (scalar >= 0xe0 && scalar <= 0xef) { scalar &= 0x0f; count = 2; }
        else if (scalar >= 0xf0 && scalar <= 0xf4) { scalar &= 0x07; count = 3; }
        else if (scalar >= 0x80) return 0;
        while (count--) {
            if ((*p & 0xc0) != 0x80) return 0;
            scalar = (scalar << 6) | (*p++ & 0x3f);
        }
        if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) return 0;
        if (scalar > 0x20 && !TTF_GlyphIsProvided32(font, scalar)) return 1;
    }
    return 0;
}
#ifdef __APPLE__
typedef struct NativeLine { CTLineRef line; double ascent, descent, leading, width; } NativeLine;
static NativeLine make_line(const char* path, float points, const char* text, SDL_Color color) {
    NativeLine result = {0};
    if (!path || !text || !isfinite(points) || points <= 0 || points > 512 || strlen(text) > 8192) return result;
    CGDataProviderRef provider = CGDataProviderCreateWithFilename(path);
    CGFontRef graphics = provider ? CGFontCreateWithDataProvider(provider) : NULL;
    if (provider) CGDataProviderRelease(provider);
    CTFontRef font = graphics ? CTFontCreateWithGraphicsFont(graphics, points, NULL, NULL) : NULL;
    if (graphics) CGFontRelease(graphics);
    CFStringRef string = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
    if (!font || !string) { if (font) CFRelease(font); if (string) CFRelease(string); return result; }
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGFloat rgba[4] = {color.r/255.0, color.g/255.0, color.b/255.0, color.a/255.0};
    CGColorRef ink = CGColorCreate(space, rgba);
    const void* keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
    const void* values[] = {font, ink};
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef attributed = CFAttributedStringCreate(NULL, string, attrs);
    result.line = CTLineCreateWithAttributedString(attributed);
    if (result.line) result.width = CTLineGetTypographicBounds(result.line, &result.ascent, &result.descent, &result.leading);
    CFRelease(attributed); CFRelease(attrs); CGColorRelease(ink); CGColorSpaceRelease(space); CFRelease(string); CFRelease(font);
    return result;
}
#endif
int kit_render_native_font_measure(const char* path, float points, const char* utf8, int* width, int* height) {
#ifdef __APPLE__
    NativeLine line = make_line(path, points, utf8, (SDL_Color){255,255,255,255});
    if (!line.line) return 0;
    if (width) *width = (int)ceil(line.width);
    if (height) *height = (int)ceil(line.ascent + line.descent + line.leading);
    CFRelease(line.line); return 1;
#else
    (void)path; (void)points; (void)utf8; (void)width; (void)height; return 0;
#endif
}
SDL_Surface* kit_render_native_font_rasterize(const char* path, float points, const char* utf8, SDL_Color color, float scale) {
#ifdef __APPLE__
    if (!isfinite(scale) || scale < 1 || scale > 8) return NULL;
    NativeLine line = make_line(path, points, utf8, color);
    if (!line.line) return NULL;
    int width = (int)ceil(line.width), height = (int)ceil(line.ascent + line.descent + line.leading);
    int rw = (int)ceil(fmax(width,1)*scale), rh = (int)ceil(fmax(height,1)*scale);
    if (rw > 65536 || rh > 8192 || (size_t)rw*(size_t)rh > (16u<<20)) { CFRelease(line.line); return NULL; }
    unsigned char* bytes = calloc((size_t)rw*(size_t)rh,4);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = bytes ? CGBitmapContextCreate(bytes,rw,rh,8,(size_t)rw*4,space,kCGBitmapByteOrder32Big|kCGImageAlphaPremultipliedLast) : NULL;
    CGColorSpaceRelease(space);
    SDL_Surface* surface = NULL;
    if (context) {
        CGContextScaleCTM(context,scale,scale);
        CGContextSetTextPosition(context,0,line.descent + line.leading);
        CTLineDraw(line.line,context);
        surface = SDL_CreateRGBSurfaceWithFormat(0,rw,rh,32,SDL_PIXELFORMAT_RGBA32);
        if (surface) for (int y=0;y<rh;++y) {
            unsigned char* dst = (unsigned char*)surface->pixels + y*surface->pitch;
            const unsigned char* src = bytes + (size_t)y*rw*4;
            for (int x=0;x<rw;++x) {
                unsigned alpha=src[x*4+3];
                for (int c=0;c<3;++c) dst[x*4+c]=(unsigned char)(alpha ? fmin(255,(src[x*4+c]*255u+alpha/2)/alpha) : 0);
                dst[x*4+3]=(unsigned char)alpha;
            }
        }
        CGContextRelease(context);
    }
    free(bytes); CFRelease(line.line); return surface;
#else
    (void)path; (void)points; (void)utf8; (void)color; (void)scale; return NULL;
#endif
}
