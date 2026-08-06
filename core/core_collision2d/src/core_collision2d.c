#include "core_collision2d.h"

#include <math.h>
#include <stddef.h>

static bool core_collision2d_is_finite_vec2(CoreCollision2DVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool core_collision2d_valid_vertices(const CoreCollision2DVec2* vertices, int vertex_count) {
    if (vertices == NULL ||
        vertex_count < 3 ||
        vertex_count > CORE_COLLISION2D_MAX_POLYGON_VERTICES) {
        return false;
    }
    for (int i = 0; i < vertex_count; ++i) {
        if (!core_collision2d_is_finite_vec2(vertices[i])) {
            return false;
        }
    }
    return true;
}

static bool core_collision2d_shape_kind_validate(CoreCollision2DShapeKind kind) {
    return kind == CORE_COLLISION2D_SHAPE_CIRCLE ||
           kind == CORE_COLLISION2D_SHAPE_BOX ||
           kind == CORE_COLLISION2D_SHAPE_CONVEX_POLYGON;
}

static bool core_collision2d_normal_validate(CoreCollision2DVec2 normal) {
    if (!core_collision2d_is_finite_vec2(normal)) {
        return false;
    }

    const double length_sq = normal.x * normal.x + normal.y * normal.y;
    if (!isfinite(length_sq)) {
        return false;
    }

    return fabs(length_sq - 1.0) <= 0.000001;
}

static CoreCollision2DVec2 core_collision2d_vec2_negate(CoreCollision2DVec2 value) {
    return core_collision2d_vec2(-value.x, -value.y);
}

static CoreCollision2DVec2 core_collision2d_polygon_centroid_average(
    const CoreCollision2DVec2* vertices,
    int vertex_count) {
    CoreCollision2DVec2 sum = core_collision2d_vec2(0.0, 0.0);
    if (vertices == NULL || vertex_count <= 0) {
        return sum;
    }

    for (int i = 0; i < vertex_count; ++i) {
        sum.x += vertices[i].x;
        sum.y += vertices[i].y;
    }
    return core_collision2d_vec2(sum.x / (double)vertex_count, sum.y / (double)vertex_count);
}

CoreCollision2DVec2 core_collision2d_vec2(double x, double y) {
    CoreCollision2DVec2 value = {x, y};
    return value;
}

CoreCollision2DVec2 core_collision2d_vec2_add(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs) {
    return core_collision2d_vec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

CoreCollision2DVec2 core_collision2d_vec2_sub(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs) {
    return core_collision2d_vec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

CoreCollision2DVec2 core_collision2d_vec2_scale(CoreCollision2DVec2 value, double scale) {
    return core_collision2d_vec2(value.x * scale, value.y * scale);
}

double core_collision2d_vec2_dot(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

double core_collision2d_vec2_cross(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

double core_collision2d_vec2_length(CoreCollision2DVec2 value) {
    return sqrt(core_collision2d_vec2_dot(value, value));
}

bool core_collision2d_vec2_normalize(CoreCollision2DVec2 value, CoreCollision2DVec2* out_normalized) {
    if (out_normalized == NULL || !core_collision2d_is_finite_vec2(value)) {
        return false;
    }
    const double length = core_collision2d_vec2_length(value);
    if (!isfinite(length) || length <= 0.0) {
        return false;
    }
    *out_normalized = core_collision2d_vec2_scale(value, 1.0 / length);
    return true;
}

CoreCollision2DShape core_collision2d_shape_circle(double radius) {
    return core_collision2d_shape_circle_offset(radius, core_collision2d_vec2(0.0, 0.0));
}

CoreCollision2DShape core_collision2d_shape_circle_offset(double radius, CoreCollision2DVec2 offset) {
    CoreCollision2DShape shape = {0};
    shape.kind = CORE_COLLISION2D_SHAPE_CIRCLE;
    shape.offset = offset;
    shape.data.circle.radius = radius;
    return shape;
}

CoreCollision2DShape core_collision2d_shape_box(double half_width, double half_height) {
    return core_collision2d_shape_box_offset(half_width, half_height, core_collision2d_vec2(0.0, 0.0));
}

CoreCollision2DShape core_collision2d_shape_box_offset(
    double half_width,
    double half_height,
    CoreCollision2DVec2 offset) {
    CoreCollision2DShape shape = {0};
    shape.kind = CORE_COLLISION2D_SHAPE_BOX;
    shape.offset = offset;
    shape.data.box.half_width = half_width;
    shape.data.box.half_height = half_height;
    return shape;
}

CoreCollision2DShape core_collision2d_shape_convex_polygon(
    const CoreCollision2DVec2* vertices,
    int vertex_count) {
    return core_collision2d_shape_convex_polygon_offset(
        vertices,
        vertex_count,
        core_collision2d_vec2(0.0, 0.0));
}

CoreCollision2DShape core_collision2d_shape_convex_polygon_offset(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2 offset) {
    CoreCollision2DShape shape = {0};
    shape.kind = CORE_COLLISION2D_SHAPE_CONVEX_POLYGON;
    shape.offset = offset;

    if (!core_collision2d_valid_vertices(vertices, vertex_count)) {
        shape.data.polygon.vertex_count = 0;
        return shape;
    }

    shape.data.polygon.vertex_count = vertex_count;
    for (int i = 0; i < vertex_count; ++i) {
        shape.data.polygon.vertices[i] = vertices[i];
    }
    return shape;
}

const char* core_collision2d_shape_kind_name(CoreCollision2DShapeKind kind) {
    switch (kind) {
        case CORE_COLLISION2D_SHAPE_CIRCLE:
            return "circle";
        case CORE_COLLISION2D_SHAPE_BOX:
            return "box";
        case CORE_COLLISION2D_SHAPE_CONVEX_POLYGON:
            return "convex_polygon";
        case CORE_COLLISION2D_SHAPE_INVALID:
        default:
            return "unknown";
    }
}

bool core_collision2d_shape_validate(const CoreCollision2DShape* shape) {
    if (shape == NULL || !core_collision2d_is_finite_vec2(shape->offset)) {
        return false;
    }

    switch (shape->kind) {
        case CORE_COLLISION2D_SHAPE_CIRCLE:
            return isfinite(shape->data.circle.radius) && shape->data.circle.radius > 0.0;

        case CORE_COLLISION2D_SHAPE_BOX:
            return isfinite(shape->data.box.half_width) &&
                   shape->data.box.half_width > 0.0 &&
                   isfinite(shape->data.box.half_height) &&
                   shape->data.box.half_height > 0.0;

        case CORE_COLLISION2D_SHAPE_CONVEX_POLYGON:
            return core_collision2d_polygon_is_convex(
                shape->data.polygon.vertices,
                shape->data.polygon.vertex_count);

        case CORE_COLLISION2D_SHAPE_INVALID:
        default:
            return false;
    }
}

bool core_collision2d_shape_aabb(const CoreCollision2DShape* shape, CoreCollision2DAabb* out_aabb) {
    if (out_aabb == NULL || !core_collision2d_shape_validate(shape)) {
        return false;
    }

    switch (shape->kind) {
        case CORE_COLLISION2D_SHAPE_CIRCLE: {
            const double radius = shape->data.circle.radius;
            out_aabb->min_x = shape->offset.x - radius;
            out_aabb->min_y = shape->offset.y - radius;
            out_aabb->max_x = shape->offset.x + radius;
            out_aabb->max_y = shape->offset.y + radius;
            return true;
        }

        case CORE_COLLISION2D_SHAPE_BOX:
            out_aabb->min_x = shape->offset.x - shape->data.box.half_width;
            out_aabb->min_y = shape->offset.y - shape->data.box.half_height;
            out_aabb->max_x = shape->offset.x + shape->data.box.half_width;
            out_aabb->max_y = shape->offset.y + shape->data.box.half_height;
            return true;

        case CORE_COLLISION2D_SHAPE_CONVEX_POLYGON: {
            const int count = shape->data.polygon.vertex_count;
            double min_x = shape->data.polygon.vertices[0].x;
            double min_y = shape->data.polygon.vertices[0].y;
            double max_x = shape->data.polygon.vertices[0].x;
            double max_y = shape->data.polygon.vertices[0].y;
            for (int i = 1; i < count; ++i) {
                const CoreCollision2DVec2 vertex = shape->data.polygon.vertices[i];
                if (vertex.x < min_x) {
                    min_x = vertex.x;
                }
                if (vertex.y < min_y) {
                    min_y = vertex.y;
                }
                if (vertex.x > max_x) {
                    max_x = vertex.x;
                }
                if (vertex.y > max_y) {
                    max_y = vertex.y;
                }
            }
            out_aabb->min_x = min_x + shape->offset.x;
            out_aabb->min_y = min_y + shape->offset.y;
            out_aabb->max_x = max_x + shape->offset.x;
            out_aabb->max_y = max_y + shape->offset.y;
            return true;
        }

        case CORE_COLLISION2D_SHAPE_INVALID:
        default:
            return false;
    }
}

bool core_collision2d_aabb_validate(const CoreCollision2DAabb* aabb) {
    return aabb != NULL &&
           isfinite(aabb->min_x) &&
           isfinite(aabb->min_y) &&
           isfinite(aabb->max_x) &&
           isfinite(aabb->max_y) &&
           aabb->min_x <= aabb->max_x &&
           aabb->min_y <= aabb->max_y;
}

double core_collision2d_aabb_width(const CoreCollision2DAabb* aabb) {
    if (!core_collision2d_aabb_validate(aabb)) {
        return 0.0;
    }
    return aabb->max_x - aabb->min_x;
}

double core_collision2d_aabb_height(const CoreCollision2DAabb* aabb) {
    if (!core_collision2d_aabb_validate(aabb)) {
        return 0.0;
    }
    return aabb->max_y - aabb->min_y;
}

CoreCollision2DVec2 core_collision2d_rotate(CoreCollision2DVec2 point, double angle_rad) {
    if (!isfinite(angle_rad)) {
        return core_collision2d_vec2(0.0, 0.0);
    }
    const double c = cos(angle_rad);
    const double s = sin(angle_rad);
    return core_collision2d_vec2(point.x * c - point.y * s, point.x * s + point.y * c);
}

CoreCollision2DVec2 core_collision2d_transform_point(
    CoreCollision2DVec2 local_point,
    CoreCollision2DVec2 position,
    double angle_rad) {
    return core_collision2d_vec2_add(core_collision2d_rotate(local_point, angle_rad), position);
}

int core_collision2d_transform_polygon(
    const CoreCollision2DVec2* local_vertices,
    int vertex_count,
    CoreCollision2DVec2 position,
    double angle_rad,
    CoreCollision2DVec2* out_world_vertices,
    int world_vertex_capacity) {
    if (!core_collision2d_valid_vertices(local_vertices, vertex_count) ||
        !core_collision2d_is_finite_vec2(position) ||
        !isfinite(angle_rad) ||
        out_world_vertices == NULL ||
        world_vertex_capacity < vertex_count) {
        return -1;
    }

    for (int i = 0; i < vertex_count; ++i) {
        out_world_vertices[i] = core_collision2d_transform_point(local_vertices[i], position, angle_rad);
    }
    return vertex_count;
}

double core_collision2d_polygon_signed_area(const CoreCollision2DVec2* vertices, int vertex_count) {
    if (!core_collision2d_valid_vertices(vertices, vertex_count)) {
        return 0.0;
    }

    double area_twice = 0.0;
    for (int i = 0; i < vertex_count; ++i) {
        const CoreCollision2DVec2 a = vertices[i];
        const CoreCollision2DVec2 b = vertices[(i + 1) % vertex_count];
        area_twice += core_collision2d_vec2_cross(a, b);
    }
    return area_twice * 0.5;
}

double core_collision2d_polygon_area(const CoreCollision2DVec2* vertices, int vertex_count) {
    return fabs(core_collision2d_polygon_signed_area(vertices, vertex_count));
}

bool core_collision2d_polygon_centroid(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2* out_centroid) {
    if (out_centroid == NULL || !core_collision2d_valid_vertices(vertices, vertex_count)) {
        return false;
    }

    double area_twice = 0.0;
    CoreCollision2DVec2 weighted = core_collision2d_vec2(0.0, 0.0);
    for (int i = 0; i < vertex_count; ++i) {
        const CoreCollision2DVec2 a = vertices[i];
        const CoreCollision2DVec2 b = vertices[(i + 1) % vertex_count];
        const double cross = core_collision2d_vec2_cross(a, b);
        area_twice += cross;
        weighted.x += (a.x + b.x) * cross;
        weighted.y += (a.y + b.y) * cross;
    }

    if (!isfinite(area_twice) || fabs(area_twice) <= 1.0e-9) {
        return false;
    }

    const double scale = 1.0 / (3.0 * area_twice);
    *out_centroid = core_collision2d_vec2(weighted.x * scale, weighted.y * scale);
    return core_collision2d_is_finite_vec2(*out_centroid);
}

bool core_collision2d_polygon_is_convex(const CoreCollision2DVec2* vertices, int vertex_count) {
    if (!core_collision2d_valid_vertices(vertices, vertex_count) ||
        core_collision2d_polygon_area(vertices, vertex_count) <= 1.0e-9) {
        return false;
    }

    double sign = 0.0;
    for (int i = 0; i < vertex_count; ++i) {
        const CoreCollision2DVec2 a = vertices[i];
        const CoreCollision2DVec2 b = vertices[(i + 1) % vertex_count];
        const CoreCollision2DVec2 c = vertices[(i + 2) % vertex_count];
        const double cross = core_collision2d_vec2_cross(
            core_collision2d_vec2_sub(b, a),
            core_collision2d_vec2_sub(c, b));
        if (fabs(cross) <= 1.0e-9) {
            return false;
        }
        if (sign == 0.0) {
            sign = cross > 0.0 ? 1.0 : -1.0;
        } else if ((cross > 0.0 && sign < 0.0) || (cross < 0.0 && sign > 0.0)) {
            return false;
        }
    }
    return true;
}

bool core_collision2d_polygon_edge_left_normal(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    int edge_index,
    CoreCollision2DVec2* out_normal) {
    if (out_normal == NULL ||
        !core_collision2d_valid_vertices(vertices, vertex_count) ||
        edge_index < 0 ||
        edge_index >= vertex_count) {
        return false;
    }

    const CoreCollision2DVec2 a = vertices[edge_index];
    const CoreCollision2DVec2 b = vertices[(edge_index + 1) % vertex_count];
    const CoreCollision2DVec2 edge = core_collision2d_vec2_sub(b, a);
    return core_collision2d_vec2_normalize(core_collision2d_vec2(-edge.y, edge.x), out_normal);
}

bool core_collision2d_project_points(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2 axis,
    CoreCollision2DProjection* out_projection) {
    if (out_projection == NULL ||
        !core_collision2d_valid_vertices(vertices, vertex_count) ||
        !core_collision2d_is_finite_vec2(axis)) {
        return false;
    }

    CoreCollision2DVec2 normalized_axis = {0};
    if (!core_collision2d_vec2_normalize(axis, &normalized_axis)) {
        return false;
    }

    double min_projection = core_collision2d_vec2_dot(vertices[0], normalized_axis);
    double max_projection = min_projection;
    for (int i = 1; i < vertex_count; ++i) {
        const double projection = core_collision2d_vec2_dot(vertices[i], normalized_axis);
        if (projection < min_projection) {
            min_projection = projection;
        }
        if (projection > max_projection) {
            max_projection = projection;
        }
    }

    out_projection->min = min_projection;
    out_projection->max = max_projection;
    return isfinite(out_projection->min) &&
           isfinite(out_projection->max) &&
           out_projection->min <= out_projection->max;
}

CoreCollision2DManifold core_collision2d_manifold_zero(void) {
    CoreCollision2DManifold manifold = {0};
    manifold.shape_a_kind = CORE_COLLISION2D_SHAPE_INVALID;
    manifold.shape_b_kind = CORE_COLLISION2D_SHAPE_INVALID;
    return manifold;
}

bool core_collision2d_manifold_init(
    CoreCollision2DManifold* out_manifold,
    int body_a_id,
    int body_b_id,
    CoreCollision2DShapeKind shape_a_kind,
    CoreCollision2DShapeKind shape_b_kind,
    CoreCollision2DVec2 normal_from_a_to_b,
    double penetration_depth,
    const CoreCollision2DVec2* contact_points,
    int contact_point_count) {
    if (out_manifold == NULL ||
        contact_points == NULL ||
        body_a_id < 0 ||
        body_b_id < 0 ||
        body_a_id == body_b_id ||
        !core_collision2d_shape_kind_validate(shape_a_kind) ||
        !core_collision2d_shape_kind_validate(shape_b_kind) ||
        !core_collision2d_normal_validate(normal_from_a_to_b) ||
        !isfinite(penetration_depth) ||
        penetration_depth < 0.0 ||
        contact_point_count <= 0 ||
        contact_point_count > CORE_COLLISION2D_MAX_CONTACT_POINTS) {
        return false;
    }

    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
    manifold.body_a_id = body_a_id;
    manifold.body_b_id = body_b_id;
    manifold.shape_a_kind = shape_a_kind;
    manifold.shape_b_kind = shape_b_kind;
    manifold.normal_from_a_to_b = normal_from_a_to_b;
    manifold.penetration_depth = penetration_depth;
    manifold.contact_point_count = contact_point_count;
    for (int i = 0; i < contact_point_count; ++i) {
        if (!core_collision2d_is_finite_vec2(contact_points[i])) {
            return false;
        }
        manifold.contact_points[i] = contact_points[i];
    }

    *out_manifold = manifold;
    return true;
}

bool core_collision2d_manifold_validate(const CoreCollision2DManifold* manifold) {
    if (manifold == NULL ||
        manifold->body_a_id < 0 ||
        manifold->body_b_id < 0 ||
        manifold->body_a_id == manifold->body_b_id ||
        !core_collision2d_shape_kind_validate(manifold->shape_a_kind) ||
        !core_collision2d_shape_kind_validate(manifold->shape_b_kind) ||
        !core_collision2d_normal_validate(manifold->normal_from_a_to_b) ||
        !isfinite(manifold->penetration_depth) ||
        manifold->penetration_depth < 0.0 ||
        manifold->contact_point_count <= 0 ||
        manifold->contact_point_count > CORE_COLLISION2D_MAX_CONTACT_POINTS) {
        return false;
    }

    for (int i = 0; i < manifold->contact_point_count; ++i) {
        if (!core_collision2d_is_finite_vec2(manifold->contact_points[i])) {
            return false;
        }
    }

    return true;
}

int core_collision2d_contact_circle_circle(
    int body_a_id,
    int body_b_id,
    CoreCollision2DVec2 center_a,
    double radius_a,
    CoreCollision2DVec2 center_b,
    double radius_b,
    CoreCollision2DManifold* out_manifold) {
    if (body_a_id < 0 ||
        body_b_id < 0 ||
        body_a_id == body_b_id ||
        !core_collision2d_is_finite_vec2(center_a) ||
        !core_collision2d_is_finite_vec2(center_b) ||
        !isfinite(radius_a) ||
        radius_a <= 0.0 ||
        !isfinite(radius_b) ||
        radius_b <= 0.0 ||
        out_manifold == NULL) {
        return -1;
    }

    const double delta_x = center_b.x - center_a.x;
    const double delta_y = center_b.y - center_a.y;
    const double distance_sq = delta_x * delta_x + delta_y * delta_y;
    const double combined_radius = radius_a + radius_b;
    const double combined_radius_sq = combined_radius * combined_radius;
    if (distance_sq > combined_radius_sq || distance_sq <= 0.0) {
        return 0;
    }

    const double distance = sqrt(distance_sq);
    const CoreCollision2DVec2 normal = core_collision2d_vec2(delta_x / distance, delta_y / distance);
    const double depth = combined_radius - distance;
    const CoreCollision2DVec2 surface_a = core_collision2d_vec2(
        center_a.x + normal.x * radius_a,
        center_a.y + normal.y * radius_a);
    const CoreCollision2DVec2 surface_b = core_collision2d_vec2(
        center_b.x - normal.x * radius_b,
        center_b.y - normal.y * radius_b);
    const CoreCollision2DVec2 contact_point = core_collision2d_vec2(
        (surface_a.x + surface_b.x) * 0.5,
        (surface_a.y + surface_b.y) * 0.5);

    if (!core_collision2d_manifold_init(
            out_manifold,
            body_a_id,
            body_b_id,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            normal,
            depth,
            &contact_point,
            1)) {
        return -1;
    }
    return 1;
}

int core_collision2d_contact_box_box_axis_aligned(
    int body_a_id,
    int body_b_id,
    CoreCollision2DVec2 center_a,
    double half_width_a,
    double half_height_a,
    CoreCollision2DVec2 center_b,
    double half_width_b,
    double half_height_b,
    CoreCollision2DManifold* out_manifold) {
    if (body_a_id < 0 ||
        body_b_id < 0 ||
        body_a_id == body_b_id ||
        !core_collision2d_is_finite_vec2(center_a) ||
        !core_collision2d_is_finite_vec2(center_b) ||
        !isfinite(half_width_a) ||
        half_width_a <= 0.0 ||
        !isfinite(half_height_a) ||
        half_height_a <= 0.0 ||
        !isfinite(half_width_b) ||
        half_width_b <= 0.0 ||
        !isfinite(half_height_b) ||
        half_height_b <= 0.0 ||
        out_manifold == NULL) {
        return -1;
    }

    const double a_min_x = center_a.x - half_width_a;
    const double a_max_x = center_a.x + half_width_a;
    const double a_min_y = center_a.y - half_height_a;
    const double a_max_y = center_a.y + half_height_a;
    const double b_min_x = center_b.x - half_width_b;
    const double b_max_x = center_b.x + half_width_b;
    const double b_min_y = center_b.y - half_height_b;
    const double b_max_y = center_b.y + half_height_b;

    const double overlap_x = fmin(a_max_x, b_max_x) - fmax(a_min_x, b_min_x);
    const double overlap_y = fmin(a_max_y, b_max_y) - fmax(a_min_y, b_min_y);
    if (overlap_x < 0.0 || overlap_y < 0.0) {
        return 0;
    }

    CoreCollision2DVec2 normal = core_collision2d_vec2(0.0, 0.0);
    CoreCollision2DVec2 contact_points[2];
    double depth = 0.0;

    if (overlap_x <= overlap_y) {
        const double contact_x = center_b.x >= center_a.x
            ? ((a_max_x + b_min_x) * 0.5)
            : ((a_min_x + b_max_x) * 0.5);
        const double min_y = fmax(a_min_y, b_min_y);
        const double max_y = fmin(a_max_y, b_max_y);
        normal = center_b.x >= center_a.x
            ? core_collision2d_vec2(1.0, 0.0)
            : core_collision2d_vec2(-1.0, 0.0);
        depth = overlap_x;
        contact_points[0] = core_collision2d_vec2(contact_x, min_y);
        contact_points[1] = core_collision2d_vec2(contact_x, max_y);
    } else {
        const double contact_y = center_b.y >= center_a.y
            ? ((a_max_y + b_min_y) * 0.5)
            : ((a_min_y + b_max_y) * 0.5);
        const double min_x = fmax(a_min_x, b_min_x);
        const double max_x = fmin(a_max_x, b_max_x);
        normal = center_b.y >= center_a.y
            ? core_collision2d_vec2(0.0, 1.0)
            : core_collision2d_vec2(0.0, -1.0);
        depth = overlap_y;
        contact_points[0] = core_collision2d_vec2(min_x, contact_y);
        contact_points[1] = core_collision2d_vec2(max_x, contact_y);
    }

    if (!core_collision2d_manifold_init(
            out_manifold,
            body_a_id,
            body_b_id,
            CORE_COLLISION2D_SHAPE_BOX,
            CORE_COLLISION2D_SHAPE_BOX,
            normal,
            depth,
            contact_points,
            2)) {
        return -1;
    }
    return 1;
}

static double core_collision2d_projection_overlap(
    CoreCollision2DProjection projection_a,
    CoreCollision2DProjection projection_b) {
    return fmin(projection_a.max, projection_b.max) - fmax(projection_a.min, projection_b.min);
}

static bool core_collision2d_polygon_pair_axis_overlap(
    const CoreCollision2DVec2* world_vertices_a,
    int vertex_count_a,
    const CoreCollision2DVec2* world_vertices_b,
    int vertex_count_b,
    CoreCollision2DVec2 centroid_delta,
    CoreCollision2DVec2 axis,
    double* out_overlap,
    CoreCollision2DVec2* out_axis_from_a_to_b) {
    CoreCollision2DVec2 oriented_axis = axis;
    CoreCollision2DProjection projection_a = {0};
    CoreCollision2DProjection projection_b = {0};

    if (out_overlap == NULL || out_axis_from_a_to_b == NULL) {
        return false;
    }
    if (core_collision2d_vec2_dot(centroid_delta, oriented_axis) < 0.0) {
        oriented_axis = core_collision2d_vec2_negate(oriented_axis);
    }
    if (!core_collision2d_project_points(world_vertices_a, vertex_count_a, oriented_axis, &projection_a) ||
        !core_collision2d_project_points(world_vertices_b, vertex_count_b, oriented_axis, &projection_b)) {
        return false;
    }

    *out_overlap = core_collision2d_projection_overlap(projection_a, projection_b);
    *out_axis_from_a_to_b = oriented_axis;
    return true;
}

static int core_collision2d_polygon_support_points(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2 direction,
    CoreCollision2DVec2 tangent,
    CoreCollision2DVec2* out_points) {
    const double epsilon = 1.0e-9;
    double best_projection = 0.0;
    int candidate_count = 0;

    if (vertices == NULL || vertex_count <= 0 || out_points == NULL) {
        return 0;
    }

    best_projection = core_collision2d_vec2_dot(vertices[0], direction);
    for (int i = 1; i < vertex_count; ++i) {
        const double projection = core_collision2d_vec2_dot(vertices[i], direction);
        if (projection > best_projection) {
            best_projection = projection;
        }
    }

    for (int i = 0; i < vertex_count; ++i) {
        const double projection = core_collision2d_vec2_dot(vertices[i], direction);
        if (fabs(projection - best_projection) <= epsilon &&
            candidate_count < CORE_COLLISION2D_MAX_CONTACT_POINTS) {
            out_points[candidate_count++] = vertices[i];
        }
    }

    if (candidate_count == 2 &&
        core_collision2d_vec2_dot(out_points[1], tangent) <
            core_collision2d_vec2_dot(out_points[0], tangent)) {
        const CoreCollision2DVec2 swap = out_points[0];
        out_points[0] = out_points[1];
        out_points[1] = swap;
    }

    return candidate_count;
}

static int core_collision2d_polygon_pair_points(
    const CoreCollision2DVec2* world_vertices_a,
    int vertex_count_a,
    const CoreCollision2DVec2* world_vertices_b,
    int vertex_count_b,
    CoreCollision2DVec2 normal_from_a_to_b,
    CoreCollision2DVec2* out_points) {
    CoreCollision2DVec2 support_a[CORE_COLLISION2D_MAX_CONTACT_POINTS];
    CoreCollision2DVec2 support_b[CORE_COLLISION2D_MAX_CONTACT_POINTS];
    const CoreCollision2DVec2 tangent = core_collision2d_vec2(-normal_from_a_to_b.y, normal_from_a_to_b.x);
    const int count_a = core_collision2d_polygon_support_points(
        world_vertices_a,
        vertex_count_a,
        normal_from_a_to_b,
        tangent,
        support_a);
    const int count_b = core_collision2d_polygon_support_points(
        world_vertices_b,
        vertex_count_b,
        core_collision2d_vec2_negate(normal_from_a_to_b),
        tangent,
        support_b);
    int count = count_a < count_b ? count_a : count_b;

    if (out_points == NULL || count <= 0) {
        return 0;
    }
    if (count > CORE_COLLISION2D_MAX_CONTACT_POINTS) {
        count = CORE_COLLISION2D_MAX_CONTACT_POINTS;
    }

    for (int i = 0; i < count; ++i) {
        out_points[i] = core_collision2d_vec2(
            (support_a[i].x + support_b[i].x) * 0.5,
            (support_a[i].y + support_b[i].y) * 0.5);
    }
    return count;
}

int core_collision2d_contact_convex_polygon_polygon(
    int body_a_id,
    int body_b_id,
    const CoreCollision2DVec2* local_vertices_a,
    int vertex_count_a,
    CoreCollision2DVec2 position_a,
    double angle_a_rad,
    const CoreCollision2DVec2* local_vertices_b,
    int vertex_count_b,
    CoreCollision2DVec2 position_b,
    double angle_b_rad,
    CoreCollision2DManifold* out_manifold) {
    CoreCollision2DVec2 world_vertices_a[CORE_COLLISION2D_MAX_POLYGON_VERTICES];
    CoreCollision2DVec2 world_vertices_b[CORE_COLLISION2D_MAX_POLYGON_VERTICES];
    CoreCollision2DVec2 best_axis = core_collision2d_vec2(0.0, 0.0);
    double best_overlap = 0.0;
    bool best_axis_set = false;

    if (body_a_id < 0 ||
        body_b_id < 0 ||
        body_a_id == body_b_id ||
        !core_collision2d_valid_vertices(local_vertices_a, vertex_count_a) ||
        !core_collision2d_valid_vertices(local_vertices_b, vertex_count_b) ||
        !core_collision2d_is_finite_vec2(position_a) ||
        !core_collision2d_is_finite_vec2(position_b) ||
        !isfinite(angle_a_rad) ||
        !isfinite(angle_b_rad) ||
        out_manifold == NULL ||
        !core_collision2d_polygon_is_convex(local_vertices_a, vertex_count_a) ||
        !core_collision2d_polygon_is_convex(local_vertices_b, vertex_count_b)) {
        return -1;
    }

    if (core_collision2d_transform_polygon(
            local_vertices_a,
            vertex_count_a,
            position_a,
            angle_a_rad,
            world_vertices_a,
            CORE_COLLISION2D_MAX_POLYGON_VERTICES) != vertex_count_a ||
        core_collision2d_transform_polygon(
            local_vertices_b,
            vertex_count_b,
            position_b,
            angle_b_rad,
            world_vertices_b,
            CORE_COLLISION2D_MAX_POLYGON_VERTICES) != vertex_count_b) {
        return -1;
    }

    const CoreCollision2DVec2 centroid_a = core_collision2d_polygon_centroid_average(
        world_vertices_a,
        vertex_count_a);
    const CoreCollision2DVec2 centroid_b = core_collision2d_polygon_centroid_average(
        world_vertices_b,
        vertex_count_b);
    const CoreCollision2DVec2 centroid_delta = core_collision2d_vec2_sub(centroid_b, centroid_a);

    for (int polygon_index = 0; polygon_index < 2; ++polygon_index) {
        const CoreCollision2DVec2* vertices = polygon_index == 0 ? world_vertices_a : world_vertices_b;
        const int vertex_count = polygon_index == 0 ? vertex_count_a : vertex_count_b;
        for (int edge_index = 0; edge_index < vertex_count; ++edge_index) {
            CoreCollision2DVec2 axis = core_collision2d_vec2(0.0, 0.0);
            CoreCollision2DVec2 oriented_axis = core_collision2d_vec2(0.0, 0.0);
            double overlap = 0.0;

            if (!core_collision2d_polygon_edge_left_normal(vertices, vertex_count, edge_index, &axis) ||
                !core_collision2d_polygon_pair_axis_overlap(
                    world_vertices_a,
                    vertex_count_a,
                    world_vertices_b,
                    vertex_count_b,
                    centroid_delta,
                    axis,
                    &overlap,
                    &oriented_axis)) {
                return -1;
            }
            if (overlap < 0.0) {
                return 0;
            }
            if (!best_axis_set || overlap < best_overlap) {
                best_axis = oriented_axis;
                best_overlap = overlap;
                best_axis_set = true;
            }
        }
    }

    if (!best_axis_set) {
        return -1;
    }

    CoreCollision2DVec2 contact_points[CORE_COLLISION2D_MAX_CONTACT_POINTS];
    const int contact_point_count = core_collision2d_polygon_pair_points(
        world_vertices_a,
        vertex_count_a,
        world_vertices_b,
        vertex_count_b,
        best_axis,
        contact_points);
    if (contact_point_count <= 0) {
        return -1;
    }

    if (!core_collision2d_manifold_init(
            out_manifold,
            body_a_id,
            body_b_id,
            CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
            CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
            best_axis,
            best_overlap,
            contact_points,
            contact_point_count)) {
        return -1;
    }
    return 1;
}
