#ifndef CORE_AUTHORED_SURFACE_MAPPING_H
#define CORE_AUTHORED_SURFACE_MAPPING_H
#include <stdbool.h>
#include <stdint.h>

/* Additive, JSON-free meaning. Hosts own transforms, IO and texture sampling.
 * v1 is planar; v2 is axial height. No implicit UV/geometry inference. */
#define CORE_AUTHORED_SURFACE_MAPPING_VERSION 1u
#define CORE_AUTHORED_SURFACE_AXIAL_VERSION 2u
typedef enum CoreAuthoredSurfaceSpace {
    CORE_AUTHORED_SURFACE_OBJECT_REST = 1,
    CORE_AUTHORED_SURFACE_WORLD = 2
} CoreAuthoredSurfaceSpace;
typedef struct CoreAuthoredSurfaceMapping {
    uint32_t version;
    CoreAuthoredSurfaceSpace space;
    double origin_m[3], axis_u[3], axis_v[3];
    double tile_m[2], offset_m[2], pivot_m[2], rotation_rad;
    uint32_t seed;
    /* v2 axial_height: axis_u is seam reference, axis_v is height direction.
     * Integer circumference repeats; explicit fade-to-base near the axis. */
    double reference_radius_m, seam_rad, pole_radius_m;
    double height_range_m[2]; /* viewport acceleration window, not a clamp */
} CoreAuthoredSurfaceMapping;
typedef struct CoreAuthoredSurfaceCoordinates {
    double uv_tiles[2]; /* unwrapped; addressing belongs to the source */
    bool valid;
    bool singular;
    double source_weight; /* axial pole fade, one elsewhere */
    uint32_t repeats_u;
    double effective_tile_width_m;
    /* Reserved data are absent, never implicitly zero-valued attributes. */
    bool has_tangent, has_footprint, has_authored_uv;
} CoreAuthoredSurfaceCoordinates;

bool core_authored_surface_mapping_validate(const CoreAuthoredSurfaceMapping* mapping);
/* Pure mapping from a point already converted to the declared meter space.
 * v1 planar remains unchanged; v2 is axial height with integer repeats. */
bool core_authored_surface_coordinates(const CoreAuthoredSurfaceMapping* mapping,
                                      const double point_m[3],
                                      CoreAuthoredSurfaceCoordinates* out);
#endif
