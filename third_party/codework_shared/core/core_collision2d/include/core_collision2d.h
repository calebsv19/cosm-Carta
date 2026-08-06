#ifndef CORE_COLLISION2D_H
#define CORE_COLLISION2D_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CORE_COLLISION2D_MAX_POLYGON_VERTICES = 16,
    CORE_COLLISION2D_MAX_CONTACT_POINTS = 2,
    CORE_COLLISION2D_MAX_COMPOUND_PARTS = 8
};

typedef enum CoreCollision2DShapeKind {
    CORE_COLLISION2D_SHAPE_INVALID = 0,
    CORE_COLLISION2D_SHAPE_CIRCLE = 1,
    CORE_COLLISION2D_SHAPE_BOX = 2,
    CORE_COLLISION2D_SHAPE_CONVEX_POLYGON = 3
} CoreCollision2DShapeKind;

typedef struct CoreCollision2DVec2 {
    double x;
    double y;
} CoreCollision2DVec2;

typedef struct CoreCollision2DAabb {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} CoreCollision2DAabb;

typedef struct CoreCollision2DProjection {
    double min;
    double max;
} CoreCollision2DProjection;

typedef struct CoreCollision2DShape {
    CoreCollision2DShapeKind kind;
    CoreCollision2DVec2 offset;
    union {
        struct {
            double radius;
        } circle;
        struct {
            double half_width;
            double half_height;
        } box;
        struct {
            int vertex_count;
            CoreCollision2DVec2 vertices[CORE_COLLISION2D_MAX_POLYGON_VERTICES];
        } polygon;
    } data;
} CoreCollision2DShape;

typedef struct CoreCollision2DManifold {
    int body_a_id;
    int body_b_id;
    CoreCollision2DShapeKind shape_a_kind;
    CoreCollision2DShapeKind shape_b_kind;
    CoreCollision2DVec2 normal_from_a_to_b;
    double penetration_depth;
    int contact_point_count;
    CoreCollision2DVec2 contact_points[CORE_COLLISION2D_MAX_CONTACT_POINTS];
} CoreCollision2DManifold;

typedef struct CoreCollision2DCompoundPart {
    CoreCollision2DShape shape;
} CoreCollision2DCompoundPart;

typedef struct CoreCollision2DCompoundShape {
    int part_count;
    CoreCollision2DCompoundPart parts[CORE_COLLISION2D_MAX_COMPOUND_PARTS];
} CoreCollision2DCompoundShape;

typedef struct CoreCollision2DCompoundMassProperties {
    double density;
    double total_area;
    double total_mass;
    CoreCollision2DVec2 center_of_mass;
    double inertia;
    double inverse_mass;
    double inverse_inertia;
    CoreCollision2DAabb local_aabb;
} CoreCollision2DCompoundMassProperties;

CoreCollision2DVec2 core_collision2d_vec2(double x, double y);
CoreCollision2DVec2 core_collision2d_vec2_add(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs);
CoreCollision2DVec2 core_collision2d_vec2_sub(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs);
CoreCollision2DVec2 core_collision2d_vec2_scale(CoreCollision2DVec2 value, double scale);
double core_collision2d_vec2_dot(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs);
double core_collision2d_vec2_cross(CoreCollision2DVec2 lhs, CoreCollision2DVec2 rhs);
double core_collision2d_vec2_length(CoreCollision2DVec2 value);
bool core_collision2d_vec2_normalize(CoreCollision2DVec2 value, CoreCollision2DVec2* out_normalized);

CoreCollision2DShape core_collision2d_shape_circle(double radius);
CoreCollision2DShape core_collision2d_shape_circle_offset(double radius, CoreCollision2DVec2 offset);
CoreCollision2DShape core_collision2d_shape_box(double half_width, double half_height);
CoreCollision2DShape core_collision2d_shape_box_offset(
    double half_width,
    double half_height,
    CoreCollision2DVec2 offset);
CoreCollision2DShape core_collision2d_shape_convex_polygon(
    const CoreCollision2DVec2* vertices,
    int vertex_count);
CoreCollision2DShape core_collision2d_shape_convex_polygon_offset(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2 offset);

const char* core_collision2d_shape_kind_name(CoreCollision2DShapeKind kind);
bool core_collision2d_shape_validate(const CoreCollision2DShape* shape);
bool core_collision2d_shape_aabb(const CoreCollision2DShape* shape, CoreCollision2DAabb* out_aabb);
bool core_collision2d_aabb_validate(const CoreCollision2DAabb* aabb);
double core_collision2d_aabb_width(const CoreCollision2DAabb* aabb);
double core_collision2d_aabb_height(const CoreCollision2DAabb* aabb);

CoreCollision2DCompoundShape core_collision2d_compound_shape_zero(void);
bool core_collision2d_compound_shape_init(
    CoreCollision2DCompoundShape* out_shape,
    const CoreCollision2DShape* parts,
    int part_count);
bool core_collision2d_compound_shape_validate(const CoreCollision2DCompoundShape* shape);
bool core_collision2d_compound_shape_aabb(
    const CoreCollision2DCompoundShape* shape,
    CoreCollision2DAabb* out_aabb);
bool core_collision2d_compound_shape_area(
    const CoreCollision2DCompoundShape* shape,
    double* out_area);
bool core_collision2d_compound_shape_mass(
    const CoreCollision2DCompoundShape* shape,
    double density,
    double* out_mass);
bool core_collision2d_compound_shape_center_of_mass(
    const CoreCollision2DCompoundShape* shape,
    CoreCollision2DVec2* out_center_of_mass);
bool core_collision2d_compound_shape_inertia(
    const CoreCollision2DCompoundShape* shape,
    double density,
    double* out_inertia);
bool core_collision2d_compound_shape_mass_properties(
    const CoreCollision2DCompoundShape* shape,
    double density,
    CoreCollision2DCompoundMassProperties* out_properties);

CoreCollision2DVec2 core_collision2d_rotate(CoreCollision2DVec2 point, double angle_rad);
CoreCollision2DVec2 core_collision2d_transform_point(
    CoreCollision2DVec2 local_point,
    CoreCollision2DVec2 position,
    double angle_rad);
int core_collision2d_transform_polygon(
    const CoreCollision2DVec2* local_vertices,
    int vertex_count,
    CoreCollision2DVec2 position,
    double angle_rad,
    CoreCollision2DVec2* out_world_vertices,
    int world_vertex_capacity);

double core_collision2d_polygon_signed_area(const CoreCollision2DVec2* vertices, int vertex_count);
double core_collision2d_polygon_area(const CoreCollision2DVec2* vertices, int vertex_count);
bool core_collision2d_polygon_centroid(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2* out_centroid);
bool core_collision2d_polygon_is_convex(const CoreCollision2DVec2* vertices, int vertex_count);
bool core_collision2d_polygon_edge_left_normal(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    int edge_index,
    CoreCollision2DVec2* out_normal);
bool core_collision2d_project_points(
    const CoreCollision2DVec2* vertices,
    int vertex_count,
    CoreCollision2DVec2 axis,
    CoreCollision2DProjection* out_projection);

CoreCollision2DManifold core_collision2d_manifold_zero(void);
bool core_collision2d_manifold_init(
    CoreCollision2DManifold* out_manifold,
    int body_a_id,
    int body_b_id,
    CoreCollision2DShapeKind shape_a_kind,
    CoreCollision2DShapeKind shape_b_kind,
    CoreCollision2DVec2 normal_from_a_to_b,
    double penetration_depth,
    const CoreCollision2DVec2* contact_points,
    int contact_point_count);
bool core_collision2d_manifold_validate(const CoreCollision2DManifold* manifold);

int core_collision2d_contact_circle_circle(
    int body_a_id,
    int body_b_id,
    CoreCollision2DVec2 center_a,
    double radius_a,
    CoreCollision2DVec2 center_b,
    double radius_b,
    CoreCollision2DManifold* out_manifold);
int core_collision2d_contact_box_box_axis_aligned(
    int body_a_id,
    int body_b_id,
    CoreCollision2DVec2 center_a,
    double half_width_a,
    double half_height_a,
    CoreCollision2DVec2 center_b,
    double half_width_b,
    double half_height_b,
    CoreCollision2DManifold* out_manifold);
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
    CoreCollision2DManifold* out_manifold);

#ifdef __cplusplus
}
#endif

#endif
