#include "ShapeLib/shape_draw_sdl.h"
#include "ShapeLib/shape_flatten.h"

#include <float.h>
#include <math.h>

static void WorldToScreen(ShapeVec2 pt,
                          float centerX, float centerY,
                          float scale,
                          int screenW, int screenH,
                          int* outX, int* outY) {
    float sx = (pt.x - centerX) * scale + (float)screenW * 0.5f;
    float sy = (pt.y - centerY) * scale + (float)screenH * 0.5f;
    if (outX) *outX = (int)lroundf(sx);
    if (outY) *outY = (int)lroundf(sy);
}

bool Shape_DrawToSDL(SDL_Renderer* renderer,
                     const Shape* shape,
                     int screenW, int screenH,
                     float maxError) {
    if (!renderer || !shape || screenW <= 0 || screenH <= 0) return false;

    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, maxError, &set)) {
        return false;
    }

    bool hasPoints = false;
    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;

    for (size_t i = 0; i < set.count; ++i) {
        Polyline* line = &set.lines[i];
        for (size_t j = 0; j < line->count; ++j) {
            ShapeVec2 p = line->points[j];
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
            hasPoints = true;
        }
    }

    if (!hasPoints) {
        PolylineSet_Free(&set);
        return true;
    }

    float width  = maxX - minX;
    float height = maxY - minY;
    float margin = 0.1f;
    float availW = screenW * (1.0f - margin * 2.0f);
    float availH = screenH * (1.0f - margin * 2.0f);
    if (availW <= 0.0f) availW = (float)screenW;
    if (availH <= 0.0f) availH = (float)screenH;

    float scaleX = (width  > 1e-6f) ? (availW / width)  : availW;
    float scaleY = (height > 1e-6f) ? (availH / height) : availH;
    float scale  = fminf(scaleX, scaleY);
    if (scale <= 0.0f) scale = 1.0f;

    float centerX = 0.5f * (minX + maxX);
    float centerY = 0.5f * (minY + maxY);

    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);

    for (size_t i = 0; i < set.count; ++i) {
        Polyline* line = &set.lines[i];
        if (line->count < 2) continue;

        ShapeVec2 prev = line->points[0];
        int px = 0, py = 0;
        WorldToScreen(prev, centerX, centerY, scale, screenW, screenH, &px, &py);

        for (size_t j = 1; j < line->count; ++j) {
            ShapeVec2 cur = line->points[j];
            int cx = 0, cy = 0;
            WorldToScreen(cur, centerX, centerY, scale, screenW, screenH, &cx, &cy);
            SDL_RenderDrawLine(renderer, px, py, cx, cy);
            px = cx; py = cy;
        }

        if (line->closed && line->count > 1) {
            ShapeVec2 first = line->points[0];
            int fx = 0, fy = 0;
            WorldToScreen(first, centerX, centerY, scale, screenW, screenH, &fx, &fy);
            SDL_RenderDrawLine(renderer, px, py, fx, fy);
        }
    }

    PolylineSet_Free(&set);
    return true;
}
