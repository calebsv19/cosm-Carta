#include "ShapeLib/shape_flatten.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ShapeVec2* points;
    size_t     count;
    size_t     capacity;
} PolylineBuilder;

static bool PolylineBuilder_Append(PolylineBuilder* b, ShapeVec2 p) {
    if (!b) return false;
    if (b->count > 0) {
        ShapeVec2 prev = b->points[b->count - 1];
        if (fabsf(prev.x - p.x) < 1e-6f && fabsf(prev.y - p.y) < 1e-6f) {
            return true;
        }
    }
    if (b->count >= b->capacity) {
        size_t nc = b->capacity ? b->capacity * 2 : 32;
        ShapeVec2* tmp = (ShapeVec2*)realloc(b->points, nc * sizeof(ShapeVec2));
        if (!tmp) return false;
        b->points = tmp;
        b->capacity = nc;
    }
    b->points[b->count++] = p;
    return true;
}

static void PolylineBuilder_Free(PolylineBuilder* b) {
    if (!b) return;
    free(b->points);
    b->points = NULL;
    b->count = 0;
    b->capacity = 0;
}

static float ShapeVec2_Length(ShapeVec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static bool ShapeVec2_NearlyEqual(ShapeVec2 a, ShapeVec2 b, float eps) {
    return fabsf(a.x - b.x) <= eps && fabsf(a.y - b.y) <= eps;
}

static ShapeVec2 CubicEval(ShapeVec2 p0,
                           ShapeVec2 c1,
                           ShapeVec2 c2,
                           ShapeVec2 p1,
                           float t) {
    float u = 1.0f - t;
    float uu = u * u;
    float uuu = uu * u;
    float tt = t * t;
    float ttt = tt * t;

    ShapeVec2 r;
    r.x = uuu * p0.x +
          3.0f * uu * t * c1.x +
          3.0f * u * tt * c2.x +
          ttt * p1.x;
    r.y = uuu * p0.y +
          3.0f * uu * t * c1.y +
          3.0f * u * tt * c2.y +
          ttt * p1.y;
    return r;
}

static float CubicEstimateLength(ShapeVec2 p0,
                                 ShapeVec2 c1,
                                 ShapeVec2 c2,
                                 ShapeVec2 p1) {
    const int samples = 8;
    ShapeVec2 prev = p0;
    float length = 0.0f;
    for (int i = 1; i <= samples; ++i) {
        float t = (float)i / (float)samples;
        ShapeVec2 cur = CubicEval(p0, c1, c2, p1, t);
        ShapeVec2 d = { cur.x - prev.x, cur.y - prev.y };
        length += ShapeVec2_Length(d);
        prev = cur;
    }
    return length;
}

static bool FlattenPath(const ShapePath* path, float maxError, Polyline* out) {
    if (!path || !out) return false;
    PolylineBuilder b = {0};

    for (size_t si = 0; si < path->segmentCount; ++si) {
        const ShapeSegment* seg = &path->segments[si];
        if (!seg) continue;
        if (seg->type == SHAPE_SEGMENT_LINE) {
            if (b.count == 0 && !PolylineBuilder_Append(&b, seg->p0)) {
                PolylineBuilder_Free(&b);
                return false;
            }
            if (!PolylineBuilder_Append(&b, seg->p1)) {
                PolylineBuilder_Free(&b);
                return false;
            }
        } else if (seg->type == SHAPE_SEGMENT_CUBIC_BEZIER) {
            if (b.count == 0 && !PolylineBuilder_Append(&b, seg->p0)) {
                PolylineBuilder_Free(&b);
                return false;
            }
            float approxLen = CubicEstimateLength(seg->p0, seg->c1, seg->c2, seg->p1);
            float spacing = fmaxf(maxError, 0.01f);
            int steps = (int)ceilf(approxLen / spacing);
            const int minSteps = 4;
            const int maxSteps = 96;
            if (steps < minSteps) steps = minSteps;
            if (steps > maxSteps) steps = maxSteps;
            for (int s = 1; s <= steps; ++s) {
                float t = (float)s / (float)steps;
                ShapeVec2 p = CubicEval(seg->p0, seg->c1, seg->c2, seg->p1, t);
                if (!PolylineBuilder_Append(&b, p)) {
                    PolylineBuilder_Free(&b);
                    return false;
                }
            }
        }
    }

    bool shouldClose = path->closed;
    if (!shouldClose && b.count > 1) {
        ShapeVec2 first = b.points[0];
        ShapeVec2 last  = b.points[b.count - 1];
        if (ShapeVec2_NearlyEqual(first, last, fmaxf(maxError, 0.001f))) {
            shouldClose = true;
        }
    }
    if (shouldClose && b.count > 1) {
        b.points[b.count - 1] = b.points[0];
    }

    out->points = b.points;
    out->count  = b.count;
    out->closed = shouldClose;
    return true;
}

bool Shape_FlattenToPolylines(const Shape* shape,
                              float maxError,
                              PolylineSet* out) {
    if (!shape || !out) return false;
    memset(out, 0, sizeof(*out));
    if (shape->pathCount == 0) return true;

    Polyline* lines = (Polyline*)calloc(shape->pathCount, sizeof(Polyline));
    if (!lines) return false;

    for (size_t pi = 0; pi < shape->pathCount; ++pi) {
        if (!FlattenPath(&shape->paths[pi], maxError, &lines[pi])) {
            for (size_t j = 0; j < pi; ++j) {
                free(lines[j].points);
            }
            free(lines);
            return false;
        }
    }

    out->lines = lines;
    out->count = shape->pathCount;
    return true;
}

void PolylineSet_Free(PolylineSet* set) {
    if (!set || !set->lines) return;
    for (size_t i = 0; i < set->count; ++i) {
        free(set->lines[i].points);
        set->lines[i].points = NULL;
        set->lines[i].count = 0;
        set->lines[i].closed = false;
    }
    free(set->lines);
    set->lines = NULL;
    set->count = 0;
}
