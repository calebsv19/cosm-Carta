#include "map/polygon_triangulator.h"

#include <stdlib.h>

static float polygon_signed_area(const uint16_t *points, uint32_t count) {
    if (!points || count < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t j = (i + 1) % count;
        float xi = (float)points[i * 2];
        float yi = (float)points[i * 2 + 1];
        float xj = (float)points[j * 2];
        float yj = (float)points[j * 2 + 1];
        area += xi * yj - xj * yi;
    }
    return area * 0.5f;
}

static bool point_in_triangle(float px, float py,
                              float ax, float ay,
                              float bx, float by,
                              float cx, float cy) {
    float v0x = cx - ax;
    float v0y = cy - ay;
    float v1x = bx - ax;
    float v1y = by - ay;
    float v2x = px - ax;
    float v2y = py - ay;

    float dot00 = v0x * v0x + v0y * v0y;
    float dot01 = v0x * v1x + v0y * v1y;
    float dot02 = v0x * v2x + v0y * v2y;
    float dot11 = v1x * v1x + v1y * v1y;
    float dot12 = v1x * v2x + v1y * v2y;

    float denom = dot00 * dot11 - dot01 * dot01;
    if (denom == 0.0f) {
        return false;
    }
    float inv = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * inv;
    float v = (dot00 * dot12 - dot01 * dot02) * inv;
    return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
}

static bool ear_clip(const uint16_t *points,
                     uint32_t count,
                     int *out_indices,
                     int *out_index_count,
                     int max_indices) {
    if (!points || count < 3 || !out_indices || !out_index_count || max_indices < 3) {
        return false;
    }

    int max_tris = (int)(count - 2);
    if (max_indices < max_tris * 3) {
        return false;
    }

    int *verts = (int *)malloc(sizeof(int) * count);
    if (!verts) {
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        verts[i] = (int)i;
    }

    float area = polygon_signed_area(points, count);
    int winding = area >= 0.0f ? 1 : -1;

    int remaining = (int)count;
    int index_count = 0;
    int guard = 0;

    while (remaining > 2 && guard < remaining * remaining) {
        bool clipped = false;
        for (int i = 0; i < remaining; ++i) {
            int i0 = verts[(i + remaining - 1) % remaining];
            int i1 = verts[i];
            int i2 = verts[(i + 1) % remaining];

            float ax = (float)points[i0 * 2];
            float ay = (float)points[i0 * 2 + 1];
            float bx = (float)points[i1 * 2];
            float by = (float)points[i1 * 2 + 1];
            float cx = (float)points[i2 * 2];
            float cy = (float)points[i2 * 2 + 1];

            float cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if ((cross > 0.0f ? 1 : -1) != winding) {
                continue;
            }

            bool contains = false;
            for (int j = 0; j < remaining; ++j) {
                int vi = verts[j];
                if (vi == i0 || vi == i1 || vi == i2) {
                    continue;
                }

                float px = (float)points[vi * 2];
                float py = (float)points[vi * 2 + 1];
                if (point_in_triangle(px, py, ax, ay, bx, by, cx, cy)) {
                    contains = true;
                    break;
                }
            }

            if (contains) {
                continue;
            }

            out_indices[index_count++] = i0;
            out_indices[index_count++] = i1;
            out_indices[index_count++] = i2;

            for (int k = i; k < remaining - 1; ++k) {
                verts[k] = verts[k + 1];
            }
            remaining -= 1;
            clipped = true;
            break;
        }

        if (!clipped) {
            guard += 1;
        }
    }

    free(verts);

    if (remaining != 2) {
        return false;
    }

    *out_index_count = index_count;
    return index_count >= 3;
}

bool polygon_triangulate(const uint16_t *points,
                         uint32_t count,
                         PolygonTriangulationMode mode,
                         int *out_indices,
                         int *out_index_count,
                         int max_indices) {
    if (!points || count < 3 || !out_indices || !out_index_count) {
        return false;
    }

    if (mode == POLYGON_TRIANGULATION_FAN) {
        int max_tris = (int)(count - 2);
        if (max_indices < max_tris * 3) {
            return false;
        }

        int idx = 0;
        for (uint32_t i = 1; i + 1 < count; ++i) {
            out_indices[idx++] = 0;
            out_indices[idx++] = (int)i;
            out_indices[idx++] = (int)(i + 1);
        }
        *out_index_count = idx;
        return idx >= 3;
    }

    return ear_clip(points, count, out_indices, out_index_count, max_indices);
}
