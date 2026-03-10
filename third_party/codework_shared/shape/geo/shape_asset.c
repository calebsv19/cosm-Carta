#include "geo/shape_asset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ShapeLib/shape_flatten.h"

#if __has_include("render/TimerHUD/external/cJSON.h")
#include "render/TimerHUD/external/cJSON.h"
#elif __has_include("external/cjson/cJSON.h")
#include "external/cjson/cJSON.h"
#elif __has_include("../external/cjson/cJSON.h")
#include "../external/cjson/cJSON.h"
#else
#error "cJSON header not found for shape_asset"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static char *dup_string(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

void shape_asset_free(ShapeAsset *asset) {
    if (!asset) return;
    if (asset->paths) {
        for (size_t i = 0; i < asset->path_count; ++i) {
            ShapeAssetPath *p = &asset->paths[i];
            free(p->points);
            p->points = NULL;
            p->point_count = 0;
            p->closed = false;
        }
        free(asset->paths);
        asset->paths = NULL;
        asset->path_count = 0;
    }
    free(asset->name);
    asset->name = NULL;
    asset->schema = 0;
}

static ShapeAssetPoint point_from_json(const cJSON *obj, bool *ok) {
    ShapeAssetPoint p = {0.0f, 0.0f};
    if (!cJSON_IsObject(obj)) {
        *ok = false;
        return p;
    }
    const cJSON *xItem = cJSON_GetObjectItem(obj, "x");
    const cJSON *yItem = cJSON_GetObjectItem(obj, "y");
    if (!cJSON_IsNumber(xItem) || !cJSON_IsNumber(yItem)) {
        *ok = false;
        return p;
    }
    p.x = (float)xItem->valuedouble;
    p.y = (float)yItem->valuedouble;
    *ok = true;
    return p;
}

bool shape_asset_load_file(const char *path, ShapeAsset *out_asset) {
    if (!path || !out_asset) return false;
    memset(out_asset, 0, sizeof(*out_asset));

    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return false; }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return false; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) { free(buf); return false; }
    buf[sz] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return false;

    ShapeAsset asset = {0};
    asset.schema = 1;

    cJSON *schemaItem = cJSON_GetObjectItem(root, "schema");
    if (cJSON_IsNumber(schemaItem)) {
        asset.schema = schemaItem->valueint;
    }
    cJSON *nameItem = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(nameItem) && nameItem->valuestring) {
        asset.name = dup_string(nameItem->valuestring);
        if (!asset.name) { cJSON_Delete(root); shape_asset_free(&asset); return false; }
    }

    cJSON *pathsArr = cJSON_GetObjectItem(root, "paths");
    if (!cJSON_IsArray(pathsArr)) {
        cJSON_Delete(root);
        shape_asset_free(&asset);
        return false;
    }
    int pathCount = cJSON_GetArraySize(pathsArr);
    if (pathCount < 0) pathCount = 0;
    if (pathCount > 0) {
        asset.paths = (ShapeAssetPath *)calloc((size_t)pathCount, sizeof(ShapeAssetPath));
        if (!asset.paths) {
            cJSON_Delete(root);
            shape_asset_free(&asset);
            return false;
        }
        asset.path_count = (size_t)pathCount;
    }

    for (int pi = 0; pi < pathCount; ++pi) {
        cJSON *pObj = cJSON_GetArrayItem(pathsArr, pi);
        if (!cJSON_IsObject(pObj)) {
            cJSON_Delete(root);
            shape_asset_free(&asset);
            return false;
        }
        ShapeAssetPath *p = &asset.paths[pi];
        p->closed = cJSON_IsTrue(cJSON_GetObjectItem(pObj, "closed"));

        cJSON *ptsArr = cJSON_GetObjectItem(pObj, "points");
        if (!cJSON_IsArray(ptsArr)) {
            cJSON_Delete(root);
            shape_asset_free(&asset);
            return false;
        }
        int ptCount = cJSON_GetArraySize(ptsArr);
        if (ptCount < 0) ptCount = 0;
        if (ptCount > 0) {
            p->points = (ShapeAssetPoint *)calloc((size_t)ptCount, sizeof(ShapeAssetPoint));
            if (!p->points) {
                cJSON_Delete(root);
                shape_asset_free(&asset);
                return false;
            }
            p->point_count = (size_t)ptCount;
        }
        for (int vi = 0; vi < ptCount; ++vi) {
            bool ok = true;
            cJSON *ptObj = cJSON_GetArrayItem(ptsArr, vi);
            ShapeAssetPoint pt = point_from_json(ptObj, &ok);
            if (!ok) {
                cJSON_Delete(root);
                shape_asset_free(&asset);
                return false;
            }
            p->points[vi] = pt;
        }
    }

    cJSON_Delete(root);
    *out_asset = asset;
    return true;
}

bool shape_asset_bounds(const ShapeAsset *asset, ShapeAssetBounds *out_bounds) {
    if (!asset || !out_bounds) return false;
    float min_x = 0.0f, min_y = 0.0f, max_x = 0.0f, max_y = 0.0f;
    bool has_points = false;
    for (size_t pi = 0; pi < asset->path_count; ++pi) {
        const ShapeAssetPath *p = &asset->paths[pi];
        for (size_t vi = 0; vi < p->point_count; ++vi) {
            ShapeAssetPoint pt = p->points[vi];
            if (!has_points) {
                min_x = max_x = pt.x;
                min_y = max_y = pt.y;
                has_points = true;
            } else {
                if (pt.x < min_x) min_x = pt.x;
                if (pt.x > max_x) max_x = pt.x;
                if (pt.y < min_y) min_y = pt.y;
                if (pt.y > max_y) max_y = pt.y;
            }
        }
    }
    ShapeAssetBounds b = {0};
    if (has_points) {
        b.min_x = min_x;
        b.min_y = min_y;
        b.max_x = max_x;
        b.max_y = max_y;
        b.valid = true;
    }
    *out_bounds = b;
    return has_points;
}

static cJSON *point_to_json(ShapeAssetPoint p) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;
    cJSON_AddNumberToObject(obj, "x", p.x);
    cJSON_AddNumberToObject(obj, "y", p.y);
    return obj;
}

bool shape_asset_save_file(const ShapeAsset *asset, const char *path) {
    if (!asset || !path) return false;
    cJSON *root = cJSON_CreateObject();
    if (!root) return false;
    cJSON_AddNumberToObject(root, "schema", asset->schema > 0 ? asset->schema : 1);
    if (asset->name) {
        cJSON_AddStringToObject(root, "name", asset->name);
    }

    cJSON *pathsArr = cJSON_AddArrayToObject(root, "paths");
    if (!pathsArr) { cJSON_Delete(root); return false; }
    for (size_t pi = 0; pi < asset->path_count; ++pi) {
        const ShapeAssetPath *p = &asset->paths[pi];
        cJSON *pObj = cJSON_CreateObject();
        if (!pObj) { cJSON_Delete(root); return false; }
        cJSON_AddItemToArray(pathsArr, pObj);
        cJSON_AddBoolToObject(pObj, "closed", p->closed);

        cJSON *ptsArr = cJSON_AddArrayToObject(pObj, "points");
        if (!ptsArr) { cJSON_Delete(root); return false; }
        for (size_t vi = 0; vi < p->point_count; ++vi) {
            cJSON *ptObj = point_to_json(p->points[vi]);
            if (!ptObj) { cJSON_Delete(root); return false; }
            cJSON_AddItemToArray(ptsArr, ptObj);
        }
    }

    char *text = cJSON_Print(root);
    if (!text) { cJSON_Delete(root); return false; }

    FILE *f = fopen(path, "wb");
    if (!f) { cJSON_Delete(root); free(text); return false; }
    size_t len = strlen(text);
    size_t n = fwrite(text, 1, len, f);
    fclose(f);
    cJSON_Delete(root);
    free(text);
    return n == len;
}

bool shape_asset_from_shapelib_shape(const Shape *shape,
                                     float max_error,
                                     ShapeAsset *out_asset) {
    if (!shape || !out_asset) return false;
    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, max_error, &set)) {
        return false;
    }

    ShapeAsset asset = {0};
    asset.schema = 1;
    asset.name = dup_string(shape->name);
    if (set.count > 0) {
        asset.paths = (ShapeAssetPath *)calloc(set.count, sizeof(ShapeAssetPath));
        if (!asset.paths) {
            PolylineSet_Free(&set);
            shape_asset_free(&asset);
            return false;
        }
        asset.path_count = set.count;
    }

    for (size_t pi = 0; pi < set.count; ++pi) {
        const Polyline *line = &set.lines[pi];
        ShapeAssetPath *p = &asset.paths[pi];
        p->closed = line->closed;
        if (line->count > 0) {
            p->points = (ShapeAssetPoint *)calloc(line->count, sizeof(ShapeAssetPoint));
            if (!p->points) {
                PolylineSet_Free(&set);
                shape_asset_free(&asset);
                return false;
            }
            p->point_count = line->count;
            for (size_t vi = 0; vi < line->count; ++vi) {
                p->points[vi].x = line->points[vi].x;
                p->points[vi].y = line->points[vi].y;
            }
        }
    }

    PolylineSet_Free(&set);
    *out_asset = asset;
    return true;
}

static void mask_set(uint8_t *mask, int w, int h, int x, int y) {
    if (!mask) return;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    mask[(size_t)y * (size_t)w + (size_t)x] = 1;
}

static ShapeAssetPoint rotate_about(ShapeAssetPoint p,
                                    float cx, float cy,
                                    float cos_a, float sin_a) {
    float dx = p.x - cx;
    float dy = p.y - cy;
    ShapeAssetPoint r;
    r.x = dx * cos_a - dy * sin_a + cx;
    r.y = dx * sin_a + dy * cos_a + cy;
    return r;
}

static void world_to_grid(float px, float py,
                          float scale, float ox, float oy,
                          int *out_x, int *out_y) {
    float gx = px * scale + ox;
    float gy = py * scale + oy;
    if (out_x) *out_x = (int)lroundf(gx);
    if (out_y) *out_y = (int)lroundf(gy);
}

static void rasterize_segment(uint8_t *mask,
                              int w, int h,
                              ShapeAssetPoint a,
                              ShapeAssetPoint b,
                              float center_x, float center_y,
                              float cos_a, float sin_a,
                              float scale, float ox, float oy,
                              float stroke) {
    ShapeAssetPoint ra = rotate_about(a, center_x, center_y, cos_a, sin_a);
    ShapeAssetPoint rb = rotate_about(b, center_x, center_y, cos_a, sin_a);
    int ax, ay, bx, by;
    world_to_grid(ra.x, ra.y, scale, ox, oy, &ax, &ay);
    world_to_grid(rb.x, rb.y, scale, ox, oy, &bx, &by);
    int dx = bx - ax;
    int dy = by - ay;
    int steps = (int)ceilf(fmaxf(fabsf((float)dx), fabsf((float)dy)));
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float gx = (1.0f - t) * (float)ax + t * (float)bx;
        float gy = (1.0f - t) * (float)ay + t * (float)by;
        int cx = (int)lroundf(gx);
        int cy = (int)lroundf(gy);
        int half = (int)ceilf(fmaxf(stroke * 0.5f, 0.5f));
        for (int yy = cy - half; yy <= cy + half; ++yy) {
            for (int xx = cx - half; xx <= cx + half; ++xx) {
                mask_set(mask, w, h, xx, yy);
            }
        }
    }
}

static bool point_in_polygon(const ShapeAssetPoint *pts, size_t count, float x, float y) {
    bool inside = false;
    if (!pts || count < 3) return false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        float xi = pts[i].x, yi = pts[i].y;
        float xj = pts[j].x, yj = pts[j].y;
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / ((yj - yi) + 1e-6f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

static void fill_polygon(uint8_t *mask,
                         int w, int h,
                         const ShapeAssetPath *path,
                         float center_x, float center_y,
                         float cos_a, float sin_a,
                         float scale, float ox, float oy) {
    if (!path || !path->closed || path->point_count < 3) return;
    float inv_scale = (scale > 1e-6f) ? (1.0f / scale) : 1.0f;
    const ShapeAssetPoint *pts = path->points;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float wx = ((float)x - ox) * inv_scale;
            float wy = ((float)y - oy) * inv_scale;
            float dx = wx - center_x;
            float dy = wy - center_y;
            float lx = dx * cos_a + dy * sin_a + center_x;
            float ly = -dx * sin_a + dy * cos_a + center_y;
            if (point_in_polygon(pts, path->point_count, lx, ly)) {
                mask_set(mask, w, h, x, y);
            }
        }
    }
}

static void fit_transform(const ShapeAssetBounds *b,
                          int grid_w,
                          int grid_h,
                          float margin,
                          float *out_scale,
                          float *out_offset_x,
                          float *out_offset_y,
                          float *out_center_x,
                          float *out_center_y) {
    float width = b->max_x - b->min_x;
    float height = b->max_y - b->min_y;
    float avail_w = (float)(grid_w - 1) - margin * 2.0f;
    float avail_h = (float)(grid_h - 1) - margin * 2.0f;
    if (avail_w <= 0.0f) avail_w = (float)(grid_w - 1);
    if (avail_h <= 0.0f) avail_h = (float)(grid_h - 1);

    float scale_x = width > 1e-5f ? (avail_w / width) : avail_w;
    float scale_y = height > 1e-5f ? (avail_h / height) : avail_h;
    float scale = fminf(scale_x, scale_y);
    if (scale <= 0.0f) scale = 1.0f;

    float center_x = 0.5f * (b->min_x + b->max_x);
    float center_y = 0.5f * (b->min_y + b->max_y);
    float offset_x = 0.5f * (float)(grid_w - 1) - center_x * scale;
    float offset_y = 0.5f * (float)(grid_h - 1) - center_y * scale;

    if (out_scale) *out_scale = scale;
    if (out_offset_x) *out_offset_x = offset_x;
    if (out_offset_y) *out_offset_y = offset_y;
    if (out_center_x) *out_center_x = center_x;
    if (out_center_y) *out_center_y = center_y;
}

bool shape_asset_rasterize(const ShapeAsset *asset,
                           int grid_w,
                           int grid_h,
                           const ShapeAssetRasterOptions *opts,
                           uint8_t *mask_out) {
    if (!asset || grid_w <= 0 || grid_h <= 0 || !mask_out) return false;
    memset(mask_out, 0, (size_t)grid_w * (size_t)grid_h);

    float margin = 2.0f;
    float stroke = 1.0f;
    float pos_x = 0.5f;
    float pos_y = 0.5f;
    float rotation_deg = 0.0f;
    float user_scale = 1.0f;
    bool center_fit = true;
    if (opts) {
        if (opts->margin_cells >= 0.0f) margin = opts->margin_cells;
        if (opts->stroke > 0.0f) stroke = opts->stroke;
        if (opts->position_x_norm >= 0.0f && opts->position_x_norm <= 1.0f) pos_x = opts->position_x_norm;
        if (opts->position_y_norm >= 0.0f && opts->position_y_norm <= 1.0f) pos_y = opts->position_y_norm;
        rotation_deg = opts->rotation_deg;
        if (opts->scale > 0.0f) user_scale = opts->scale;
        center_fit = opts->center_fit;
    }

    ShapeAssetBounds bounds = {0};
    if (!shape_asset_bounds(asset, &bounds) || !bounds.valid) {
        return false;
    }

    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    float center_x = 0.5f * (bounds.min_x + bounds.max_x);
    float center_y = 0.5f * (bounds.min_y + bounds.max_y);

    if (center_fit) {
        fit_transform(&bounds, grid_w, grid_h, margin, &scale, &ox, &oy, &center_x, &center_y);
    } else {
        scale = user_scale;
        float target_x = pos_x * (float)(grid_w - 1);
        float target_y = pos_y * (float)(grid_h - 1);
        ox = target_x - center_x * scale;
        oy = target_y - center_y * scale;
    }

    float radians = rotation_deg * (float)M_PI / 180.0f;
    float cos_a = cosf(radians);
    float sin_a = sinf(radians);

    for (size_t i = 0; i < asset->path_count; ++i) {
        const ShapeAssetPath *path = &asset->paths[i];
        if (!path || path->point_count < 2) continue;
        for (size_t j = 1; j < path->point_count; ++j) {
            ShapeAssetPoint a = path->points[j - 1];
            ShapeAssetPoint b = path->points[j];
            rasterize_segment(mask_out, grid_w, grid_h, a, b,
                              center_x, center_y, cos_a, sin_a,
                              scale, ox, oy, stroke);
        }
        if (path->closed) {
            ShapeAssetPoint a = path->points[path->point_count - 1];
            ShapeAssetPoint b = path->points[0];
            rasterize_segment(mask_out, grid_w, grid_h, a, b,
                              center_x, center_y, cos_a, sin_a,
                              scale, ox, oy, stroke);
            fill_polygon(mask_out, grid_w, grid_h, path,
                         center_x, center_y, cos_a, sin_a,
                         scale, ox, oy);
        }
    }
    return true;
}
