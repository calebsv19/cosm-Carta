#include "kit_render_fidelity_fixture.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double clamp(double value, double min, double max) {
    return fmin(fmax(value, min), max);
}

static double rounded_distance(KitRenderRect rect, double radius, double x, double y) {
    double r = clamp(radius, 0, fmin(rect.width, rect.height) * .5);
    double dx = fabs(x - rect.x - rect.width * .5) - rect.width * .5 + r;
    double dy = fabs(y - rect.y - rect.height * .5) - rect.height * .5 + r;
    return hypot(fmax(dx, 0), fmax(dy, 0)) + fmin(fmax(dx, dy), 0) - r;
}

static int contains_text(double x, double y, KitRenderTextMetrics metrics) {
    for (size_t i = 0; i < sizeof(fidelity_text_transforms) / sizeof(*fidelity_text_transforms); ++i) {
        const KitRenderTransform *t = fidelity_text_transforms + i;
        double px = (x - t->tx) / t->sx, py = (y - t->ty) / t->sy;
        if (px >= 4 && px < 12 + metrics.width_px &&
            py >= 216 - metrics.height_px * .5 && py < 224 + metrics.height_px * .5) return 1;
    }
    return 0;
}

static int same_rgb(const unsigned char *a, const unsigned char *b) {
    for (unsigned i = 0; i < 3; ++i) if (abs((int)a[i] - b[i]) > 3) return 0;
    return 1;
}

static unsigned char encode_channel(double value, int srgb) {
    value = clamp(value, 0, 1);
    if (srgb) value = value <= .0031308 ? value * 12.92 : 1.055 * pow(value, 1 / 2.4) - .055;
    return (unsigned char)lround(value * 255);
}

static void blend(double out[3], KitRenderColor source, KitRenderColor tint) {
    double alpha = source.a / 255.0 * tint.a / 255.0;
    const unsigned char src[3] = {source.r, source.g, source.b};
    const unsigned char mul[3] = {tint.r, tint.g, tint.b};
    for (unsigned i = 0; i < 3; ++i)
        out[i] = src[i] / 255.0 * mul[i] / 255.0 * alpha + out[i] * (1 - alpha);
}

static int segment(double out[3], double x, double y, unsigned scale,
                    KitRenderVec2 p0, KitRenderVec2 p1, double thickness,
                    KitRenderTransform transform, KitRenderColor color) {
    double px = (x - transform.tx) / transform.sx, py = (y - transform.ty) / transform.sy;
    double dx = p1.x - p0.x, dy = p1.y - p0.y, length = hypot(dx, dy);
    double along = ((px - p0.x) * dx + (py - p0.y) * dy) / length;
    double across = fabs(((px - p0.x) * dy - (py - p0.y) * dx) / length);
    double distance = fmax(fmax(-along, along - length), across - thickness * .5);
    double pixels = distance * fmin(fabs(transform.sx), fabs(transform.sy)) * scale;
    if (fabs(pixels) < .8) return 1;
    if (distance < 0) blend(out, color, (KitRenderColor){255, 255, 255, 255});
    return 0;
}

/* Uses inverse command coordinates, analytic edges, nearest texel sampling and
 * straight-alpha composition. It never reads generated vertices or shader output. */
static int expected_pixel(double x, double y, unsigned scale, unsigned char expected[3],
                           int srgb, KitRenderTextMetrics metrics) {
    if (contains_text(x, y, metrics)) return 0;
    double color[3] = {fidelity_background.r / 255.0, fidelity_background.g / 255.0,
                        fidelity_background.b / 255.0};
    for (size_t i = 0; i < sizeof(fidelity_rects) / sizeof(*fidelity_rects); ++i) {
        const FidelityRect *rect = fidelity_rects + i;
        const KitRenderTransform *t = &rect->transform;
        if (t->sx == 0 || t->sy == 0) continue;
        double distance = rounded_distance(rect->bounds, rect->radius,
                                            (x - t->tx) / t->sx, (y - t->ty) / t->sy);
        if (fabs(distance * fmin(fabs(t->sx), fabs(t->sy)) * scale) < 1.3) return 0;
        if (distance < 0) blend(color, rect->color, (KitRenderColor){255, 255, 255, 255});
    }
    if (segment(color, x, y, scale, (KitRenderVec2){0, 0}, (KitRenderVec2){30, 0}, 4,
                  (KitRenderTransform){8, 65, 2, 1.5f}, (KitRenderColor){230, 80, 60, 255}) ||
        segment(color, x, y, scale, (KitRenderVec2){0, 0}, (KitRenderVec2){0, 30}, 4,
                  (KitRenderTransform){110, 55, -1.5f, 1.5f}, (KitRenderColor){100, 140, 230, 255}))
        return 0;
    for (size_t i = 1; i < sizeof(fidelity_polyline) / sizeof(*fidelity_polyline); ++i)
        if (segment(color, x, y, scale, fidelity_polyline[i - 1], fidelity_polyline[i], 3,
                      (KitRenderTransform){150, 55, -1.5f, 1.5f},
                      (KitRenderColor){190, 110, 220, 255})) return 0;
    for (size_t i = 0; i < sizeof(fidelity_textures) / sizeof(*fidelity_textures); ++i) {
        const FidelityTexture *texture = fidelity_textures + i;
        const KitRenderTransform *t = &texture->transform;
        double px = (x - t->tx) / t->sx, py = (y - t->ty) / t->sy;
        double distance = rounded_distance((KitRenderRect){0, 0, 32, 32}, 0, px, py);
        if (fabs(distance * fmin(fabs(t->sx), fabs(t->sy)) * scale) < .8) return 0;
        if (distance >= 0) continue;
        if (texture->clipped) {
            double clip_distance = rounded_distance(fidelity_clip, 0, x, y);
            if (fabs(clip_distance * scale) < .8) return 0;
            if (clip_distance >= 0) continue;
        }
        double u = texture->uv_min.x + px / 32 * (texture->uv_max.x - texture->uv_min.x);
        double v = texture->uv_min.y + py / 32 * (texture->uv_max.y - texture->uv_min.y);
        /* Nearest sampling ties may choose either adjoining texel. */
        if ((u > 0 && u < 1 && fabs(u * 8 - round(u * 8)) < .0001) ||
            (v > 0 && v < 1 && fabs(v * 8 - round(v * 8)) < .0001)) return 0;
        unsigned sx = (unsigned)clamp(floor(u * 8), 0, 7);
        unsigned sy = (unsigned)clamp(floor(v * 8), 0, 7);
        blend(color, fidelity_texel(sx, sy), texture->tint);
    }
    for (unsigned i = 0; i < 3; ++i) expected[i] = encode_channel(color[i], srgb);
    return 1;
}

typedef struct TextInk {
    double min_x, min_y, max_x, max_y, center_x, center_y, weight;
    size_t count;
} TextInk;

static int measure_ink(const unsigned char *pixels, unsigned width, unsigned height,
                        unsigned scale, KitRenderTextMetrics metrics, KitRenderTransform t,
                        TextInk *ink, size_t *checked) {
    double x0 = 4 * t.sx + t.tx, x1 = (12 + metrics.width_px) * t.sx + t.tx;
    double y0 = (216 - metrics.height_px * .5) * t.sy + t.ty;
    double y1 = (224 + metrics.height_px * .5) * t.sy + t.ty;
    *ink = (TextInk){1e9, 1e9, -1e9, -1e9, 0, 0, 0, 0};
    for (int y = (int)floor(fmin(y0, y1) * scale); y < ceil(fmax(y0, y1) * scale); ++y) {
        for (int x = (int)floor(fmin(x0, x1) * scale); x < ceil(fmax(x0, x1) * scale); ++x) {
            if (x < 0 || y < 0 || x >= (int)width || y >= (int)height) return 1;
            const unsigned char *p = pixels + ((size_t)y * width + x) * 3;
            double weight = 0;
            for (unsigned c = 0; c < 3; ++c) weight += abs((int)p[c] - pixels[c]);
            ++*checked;
            if (weight <= 9) continue;
            double px = (x + .5) / scale, py = (y + .5) / scale;
            ink->min_x = fmin(ink->min_x, px); ink->max_x = fmax(ink->max_x, px);
            ink->min_y = fmin(ink->min_y, py); ink->max_y = fmax(ink->max_y, py);
            ink->center_x += px * weight; ink->center_y += py * weight;
            ink->weight += weight;
            ++ink->count;
        }
    }
    if (!ink->weight) return 1;
    ink->center_x /= ink->weight; ink->center_y /= ink->weight;
    return 0;
}

static int verify_text(const unsigned char *pixels, unsigned width, unsigned height,
                        unsigned scale, KitRenderTextMetrics metrics, size_t *checked) {
    TextInk base;
    if (measure_ink(pixels, width, height, scale, metrics, fidelity_text_transforms[0], &base, checked) ||
        base.count < 10 * scale * scale) return 1;
    for (size_t i = 1; i < sizeof(fidelity_text_transforms) / sizeof(*fidelity_text_transforms); ++i) {
        const KitRenderTransform *t = fidelity_text_transforms + i;
        TextInk actual;
        if (measure_ink(pixels, width, height, scale, metrics, *t, &actual, checked)) return 1;
        double min_x = fmin(base.min_x * t->sx, base.max_x * t->sx) + t->tx;
        double max_x = fmax(base.min_x * t->sx, base.max_x * t->sx) + t->tx;
        double min_y = fmin(base.min_y * t->sy, base.max_y * t->sy) + t->ty;
        double max_y = fmax(base.min_y * t->sy, base.max_y * t->sy) + t->ty;
        /* Font policy uses linear filtering at 1x and nearest above 1x. Scaling
         * may soften an edge; geometry and weighted placement must still agree. */
        double tolerance = 1.6 / scale;
        double expected_area = base.count * fabs(t->sx * t->sy);
        if (fabs(actual.min_x - min_x) > tolerance || fabs(actual.max_x - max_x) > tolerance ||
            fabs(actual.min_y - min_y) > tolerance || fabs(actual.max_y - max_y) > tolerance ||
            fabs(actual.center_x - (base.center_x * t->sx + t->tx)) > tolerance ||
            fabs(actual.center_y - (base.center_y * t->sy + t->ty)) > tolerance ||
            actual.count < expected_area * .5 || actual.count > expected_area * 1.8) {
            fprintf(stderr, "text transform bounds/placement mismatch: scale=%u case=%zu\n", scale, i);
            return 1;
        }
        if (t->sx == 1 && t->sy == 1) {
            for (int y = (int)((216 - metrics.height_px * .5) * scale);
                 y < (224 + metrics.height_px * .5) * scale; ++y) {
                for (int x = 4 * (int)scale; x < (12 + metrics.width_px) * scale; ++x) {
                    size_t target = ((size_t)(y + t->ty * scale) * width + x + t->tx * scale) * 3;
                    if (!same_rgb(pixels + ((size_t)y * width + x) * 3, pixels + target)) return 1;
                    ++*checked;
                }
            }
        }
    }
    return 0;
}

int kit_render_fidelity_verify_capture(const char *path, VkExtent2D extent, unsigned scale,
                                      VkFormat format, KitRenderTextMetrics metrics) {
    FILE *file = fopen(path, "rb");
    unsigned width, height, max_value;
    char magic[3] = {0};
    if (!file) return 1;
    if (fscanf(file, "%2s %u %u %u", magic, &width, &height, &max_value) != 4 ||
        strcmp(magic, "P6") || max_value != 255 || width != extent.width || height != extent.height ||
        fgetc(file) == EOF) { fclose(file); return 1; }
    size_t bytes = (size_t)width * height * 3;
    unsigned char *pixels = malloc(bytes);
    if (!pixels) { fclose(file); return 1; }
    int failed = fread(pixels, 1, bytes, file) != bytes || fgetc(file) != EOF;
    fclose(file);
    int srgb = format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
    size_t checked = 0, text_checked = 0;
    for (unsigned y = 0; y < height && !failed; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            unsigned char expected[3];
            if (!expected_pixel((x + .5) / scale, (y + .5) / scale, scale, expected, srgb, metrics)) continue;
            const unsigned char *actual = pixels + ((size_t)y * width + x) * 3;
            if (!same_rgb(actual, expected)) {
                fprintf(stderr, "fidelity pixel mismatch: scale=%u pixel=%u,%u expected=%u,%u,%u actual=%u,%u,%u\n",
                        scale, x, y, expected[0], expected[1], expected[2], actual[0], actual[1], actual[2]);
                failed = 1; break;
            }
            ++checked;
        }
    }
    if (!failed && (checked < 20000 || verify_text(pixels, width, height, scale, metrics, &text_checked))) failed = 1;
    if (!failed) printf("render fidelity image proof: scale=%u pixels=%zu text_pixels=%zu capture=%s\n",
                         scale, checked, text_checked, path);
    free(pixels);
    return failed;
}
