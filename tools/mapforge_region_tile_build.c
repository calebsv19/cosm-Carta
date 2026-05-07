#include "mapforge_region_internal.h"

static TileOutput *build_context_get_tile(BuildContext *ctx, TileCoord coord) {
    if (!ctx) {
        return NULL;
    }

    for (size_t i = 0; i < ctx->tile_count; ++i) {
        TileOutput *tile = &ctx->tiles[i];
        if (tile->coord.z == coord.z && tile->coord.x == coord.x && tile->coord.y == coord.y) {
            return tile;
        }
    }

    if (ctx->tile_count == ctx->tile_capacity) {
        size_t next = ctx->tile_capacity == 0 ? 64 : ctx->tile_capacity * 2;
        TileOutput *next_tiles = (TileOutput *)realloc(ctx->tiles, next * sizeof(TileOutput));
        if (!next_tiles) {
            return NULL;
        }
        ctx->tiles = next_tiles;
        ctx->tile_capacity = next;
    }

    TileOutput *tile = &ctx->tiles[ctx->tile_count++];
    memset(tile, 0, sizeof(*tile));
    tile->coord = coord;
    return tile;
}

static bool tile_output_add_polyline(TileOutput *tile, RoadClass road_class, const uint16_t *points, uint32_t point_count) {
    if (!tile || !points || point_count < 2) {
        return false;
    }

    if (tile->polyline_count == tile->polyline_capacity) {
        uint32_t next = tile->polyline_capacity == 0 ? 16 : tile->polyline_capacity * 2;
        TilePolyline *next_polylines = (TilePolyline *)realloc(tile->polylines, next * sizeof(TilePolyline));
        if (!next_polylines) {
            return false;
        }
        tile->polylines = next_polylines;
        tile->polyline_capacity = next;
    }

    TilePolyline *polyline = &tile->polylines[tile->polyline_count++];
    polyline->road_class = road_class;
    polyline->point_count = point_count;
    polyline->points = (uint16_t *)malloc(point_count * 2 * sizeof(uint16_t));
    if (!polyline->points) {
        return false;
    }
    memcpy(polyline->points, points, point_count * 2 * sizeof(uint16_t));
    return true;
}

static bool tile_output_add_polygon(TileOutput *tile, PolygonClass polygon_class, const uint16_t *points, uint32_t point_count) {
    if (!tile || !points || point_count < 3) {
        return false;
    }

    if (tile->polygon_count == tile->polygon_capacity) {
        uint32_t next = tile->polygon_capacity == 0 ? 8 : tile->polygon_capacity * 2;
        TilePolygon *next_polygons = (TilePolygon *)realloc(tile->polygons, next * sizeof(TilePolygon));
        if (!next_polygons) {
            return false;
        }
        tile->polygons = next_polygons;
        tile->polygon_capacity = next;
    }

    TilePolygon *polygon = &tile->polygons[tile->polygon_count++];
    polygon->polygon_class = polygon_class;
    polygon->point_count = point_count;
    polygon->points = (uint16_t *)malloc(point_count * 2 * sizeof(uint16_t));
    if (!polygon->points) {
        return false;
    }
    memcpy(polygon->points, points, point_count * 2 * sizeof(uint16_t));
    return true;
}

static uint16_t quantize_tile_coord(double value) {
    long rounded = lround(value * TILE_EXTENT);
    if (rounded < 0) {
        rounded = 0;
    } else if (rounded > (long)TILE_EXTENT) {
        rounded = (long)TILE_EXTENT;
    }
    return (uint16_t)rounded;
}

static bool clip_segment_to_rect(MercatorMeters a,
                                 MercatorMeters b,
                                 double min_x,
                                 double max_x,
                                 double min_y,
                                 double max_y,
                                 MercatorMeters *out_a,
                                 MercatorMeters *out_b) {
    if (!out_a || !out_b) {
        return false;
    }

    double x0 = a.x;
    double y0 = a.y;
    double x1 = b.x;
    double y1 = b.y;
    double dx = x1 - x0;
    double dy = y1 - y0;
    double t0 = 0.0;
    double t1 = 1.0;

    double p[4] = {-dx, dx, -dy, dy};
    double q[4] = {x0 - min_x, max_x - x0, y0 - min_y, max_y - y0};

    for (int i = 0; i < 4; ++i) {
        if (fabs(p[i]) < 1e-12) {
            if (q[i] < 0.0) {
                return false;
            }
            continue;
        }
        double r = q[i] / p[i];
        if (p[i] < 0.0) {
            if (r > t1) {
                return false;
            }
            if (r > t0) {
                t0 = r;
            }
        } else {
            if (r < t0) {
                return false;
            }
            if (r < t1) {
                t1 = r;
            }
        }
    }

    if (t1 < t0) {
        return false;
    }

    out_a->x = x0 + t0 * dx;
    out_a->y = y0 + t0 * dy;
    out_b->x = x0 + t1 * dx;
    out_b->y = y0 + t1 * dy;
    return true;
}

static bool polygon_point_inside(MercatorMeters point, int edge, double min_x, double max_x, double min_y, double max_y) {
    switch (edge) {
        case 0:
            return point.x >= min_x;
        case 1:
            return point.x <= max_x;
        case 2:
            return point.y <= max_y;
        case 3:
            return point.y >= min_y;
        default:
            return true;
    }
}

static MercatorMeters polygon_edge_intersection(MercatorMeters a,
                                                MercatorMeters b,
                                                int edge,
                                                double min_x,
                                                double max_x,
                                                double min_y,
                                                double max_y) {
    MercatorMeters out = a;
    double t = 0.0;
    double denom = 0.0;

    switch (edge) {
        case 0:
            denom = b.x - a.x;
            t = fabs(denom) > 1e-9 ? (min_x - a.x) / denom : 0.0;
            out.x = min_x;
            out.y = a.y + (b.y - a.y) * t;
            break;
        case 1:
            denom = b.x - a.x;
            t = fabs(denom) > 1e-9 ? (max_x - a.x) / denom : 0.0;
            out.x = max_x;
            out.y = a.y + (b.y - a.y) * t;
            break;
        case 2:
            denom = b.y - a.y;
            t = fabs(denom) > 1e-9 ? (max_y - a.y) / denom : 0.0;
            out.y = max_y;
            out.x = a.x + (b.x - a.x) * t;
            break;
        case 3:
            denom = b.y - a.y;
            t = fabs(denom) > 1e-9 ? (min_y - a.y) / denom : 0.0;
            out.y = min_y;
            out.x = a.x + (b.x - a.x) * t;
            break;
        default:
            break;
    }

    return out;
}

static bool clip_polygon_to_rect(const MercatorMeters *points,
                                 size_t count,
                                 double min_x,
                                 double max_x,
                                 double min_y,
                                 double max_y,
                                 MercatorMeters **out_points,
                                 size_t *out_count) {
    if (!points || count < 3 || !out_points || !out_count) {
        return false;
    }

    size_t capacity = count * 2 + 4;
    MercatorMeters *scratch_a = (MercatorMeters *)malloc(capacity * sizeof(MercatorMeters));
    MercatorMeters *scratch_b = (MercatorMeters *)malloc(capacity * sizeof(MercatorMeters));
    if (!scratch_a || !scratch_b) {
        free(scratch_a);
        free(scratch_b);
        return false;
    }

    memcpy(scratch_a, points, count * sizeof(MercatorMeters));
    size_t input_count = count;

    for (int edge = 0; edge < 4; ++edge) {
        if (input_count < 3) {
            break;
        }

        size_t output_count = 0;
        MercatorMeters prev = scratch_a[input_count - 1];
        bool prev_inside = polygon_point_inside(prev, edge, min_x, max_x, min_y, max_y);

        for (size_t i = 0; i < input_count; ++i) {
            MercatorMeters curr = scratch_a[i];
            bool curr_inside = polygon_point_inside(curr, edge, min_x, max_x, min_y, max_y);

            if (curr_inside) {
                if (!prev_inside) {
                    scratch_b[output_count++] = polygon_edge_intersection(prev, curr, edge, min_x, max_x, min_y, max_y);
                }
                scratch_b[output_count++] = curr;
            } else if (prev_inside) {
                scratch_b[output_count++] = polygon_edge_intersection(prev, curr, edge, min_x, max_x, min_y, max_y);
            }

            prev = curr;
            prev_inside = curr_inside;
        }

        MercatorMeters *swap = scratch_a;
        scratch_a = scratch_b;
        scratch_b = swap;
        input_count = output_count;
    }

    if (input_count < 3) {
        free(scratch_a);
        free(scratch_b);
        return false;
    }

    *out_points = (MercatorMeters *)malloc(input_count * sizeof(MercatorMeters));
    if (!*out_points) {
        free(scratch_a);
        free(scratch_b);
        return false;
    }

    memcpy(*out_points, scratch_a, input_count * sizeof(MercatorMeters));
    *out_count = input_count;

    free(scratch_a);
    free(scratch_b);
    return true;
}

bool mapforge_region_add_way_to_tiles(BuildContext *ctx,
                                      const MercatorMeters *points,
                                      size_t count,
                                      RoadClass road_class,
                                      uint16_t z) {
    if (!ctx || !points || count < 2) {
        return false;
    }

    double tile_size = tile_size_meters(z);
    for (size_t i = 1; i < count; ++i) {
        MercatorMeters a = points[i - 1];
        MercatorMeters b = points[i];
        if (fabs(b.x - a.x) < 1e-12 && fabs(b.y - a.y) < 1e-12) {
            continue;
        }

        double seg_min_x = a.x < b.x ? a.x : b.x;
        double seg_max_x = a.x > b.x ? a.x : b.x;
        double seg_min_y = a.y < b.y ? a.y : b.y;
        double seg_max_y = a.y > b.y ? a.y : b.y;

        TileCoord top_left = tile_from_meters(z, (MercatorMeters){seg_min_x, seg_max_y});
        TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){seg_max_x, seg_min_y});

        for (uint32_t ty = top_left.y; ty <= bottom_right.y; ++ty) {
            for (uint32_t tx = top_left.x; tx <= bottom_right.x; ++tx) {
                TileCoord coord = {z, tx, ty};
                MercatorMeters origin = tile_origin_meters(coord);
                double tile_min_x = origin.x;
                double tile_max_x = origin.x + tile_size;
                double tile_max_y = origin.y;
                double tile_min_y = origin.y - tile_size;

                MercatorMeters clipped_a = {0};
                MercatorMeters clipped_b = {0};
                if (!clip_segment_to_rect(a, b, tile_min_x, tile_max_x, tile_min_y, tile_max_y,
                                          &clipped_a, &clipped_b)) {
                    continue;
                }

                uint16_t quantized[4];
                double u0 = (clipped_a.x - tile_min_x) / tile_size;
                double v0 = (tile_max_y - clipped_a.y) / tile_size;
                double u1 = (clipped_b.x - tile_min_x) / tile_size;
                double v1 = (tile_max_y - clipped_b.y) / tile_size;
                quantized[0] = quantize_tile_coord(u0);
                quantized[1] = quantize_tile_coord(v0);
                quantized[2] = quantize_tile_coord(u1);
                quantized[3] = quantize_tile_coord(v1);

                TileOutput *output = build_context_get_tile(ctx, coord);
                if (!output || !tile_output_add_polyline(output, road_class, quantized, 2u)) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool mapforge_region_add_polygon_to_tiles(BuildContext *ctx,
                                          const MercatorMeters *points,
                                          size_t count,
                                          PolygonClass polygon_class,
                                          uint16_t z) {
    if (!ctx || !points || count < 3) {
        return false;
    }

    double min_x = points[0].x;
    double max_x = points[0].x;
    double min_y = points[0].y;
    double max_y = points[0].y;
    for (size_t i = 1; i < count; ++i) {
        if (points[i].x < min_x) {
            min_x = points[i].x;
        }
        if (points[i].x > max_x) {
            max_x = points[i].x;
        }
        if (points[i].y < min_y) {
            min_y = points[i].y;
        }
        if (points[i].y > max_y) {
            max_y = points[i].y;
        }
    }

    TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_x, max_y});
    TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_x, min_y});

    double tile_size = tile_size_meters(z);

    for (uint32_t ty = top_left.y; ty <= bottom_right.y; ++ty) {
        for (uint32_t tx = top_left.x; tx <= bottom_right.x; ++tx) {
            TileCoord coord = {z, tx, ty};
            MercatorMeters origin = tile_origin_meters(coord);
            double tile_min_x = origin.x;
            double tile_max_x = origin.x + tile_size;
            double tile_max_y = origin.y;
            double tile_min_y = origin.y - tile_size;

            MercatorMeters *clipped = NULL;
            size_t clipped_count = 0;
            if (!clip_polygon_to_rect(points, count, tile_min_x, tile_max_x, tile_min_y, tile_max_y, &clipped, &clipped_count)) {
                continue;
            }

            uint16_t *quantized = (uint16_t *)malloc(clipped_count * 2 * sizeof(uint16_t));
            if (!quantized) {
                free(clipped);
                continue;
            }

            for (size_t i = 0; i < clipped_count; ++i) {
                double u = (clipped[i].x - tile_min_x) / tile_size;
                double v = (tile_max_y - clipped[i].y) / tile_size;
                quantized[i * 2] = quantize_tile_coord(u);
                quantized[i * 2 + 1] = quantize_tile_coord(v);
            }

            TileOutput *output = build_context_get_tile(ctx, coord);
            if (output) {
                tile_output_add_polygon(output, polygon_class, quantized, (uint32_t)clipped_count);
            }

            free(quantized);
            free(clipped);
        }
    }

    return true;
}

void mapforge_region_ensure_tiles_for_bounds(BuildContext *ctx, const BuildOptions *options) {
    if (!ctx || !options || !ctx->has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){ctx->min_lat, ctx->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){ctx->max_lat, ctx->max_lon});

    for (uint16_t z = options->min_z; z <= options->max_z; ++z) {
        TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_m.x, max_m.y});
        TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_m.x, min_m.y});

        for (uint32_t y = top_left.y; y <= bottom_right.y; ++y) {
            for (uint32_t x = top_left.x; x <= bottom_right.x; ++x) {
                TileCoord coord = {z, x, y};
                build_context_get_tile(ctx, coord);
            }
        }
    }
}

static int tile_coord_compare(const void *a, const void *b) {
    const TileOutput *left = (const TileOutput *)a;
    const TileOutput *right = (const TileOutput *)b;

    if (left->coord.z != right->coord.z) {
        return (left->coord.z < right->coord.z) ? -1 : 1;
    }
    if (left->coord.x != right->coord.x) {
        return (left->coord.x < right->coord.x) ? -1 : 1;
    }
    if (left->coord.y != right->coord.y) {
        return (left->coord.y < right->coord.y) ? -1 : 1;
    }
    return 0;
}

void mapforge_region_sort_tiles(BuildContext *ctx) {
    if (!ctx || !ctx->tiles || ctx->tile_count == 0u) {
        return;
    }
    qsort(ctx->tiles, ctx->tile_count, sizeof(TileOutput), tile_coord_compare);
}
