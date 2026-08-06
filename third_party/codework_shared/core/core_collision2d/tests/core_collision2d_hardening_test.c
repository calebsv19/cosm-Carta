#include "core_collision2d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void require_true(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void require_near(double actual, double expected, double epsilon, const char* message) {
    if (!isfinite(actual) || fabs(actual - expected) > epsilon) {
        fprintf(stderr, "FAIL: %s actual=%.12f expected=%.12f\n", message, actual, expected);
        exit(1);
    }
}

static void require_vec_near(CoreCollision2DVec2 actual, double x, double y, double epsilon, const char* message) {
    if (!isfinite(actual.x) || !isfinite(actual.y) || fabs(actual.x - x) > epsilon || fabs(actual.y - y) > epsilon) {
        fprintf(
            stderr,
            "FAIL: %s actual=(%.12f,%.12f) expected=(%.12f,%.12f)\n",
            message,
            actual.x,
            actual.y,
            x,
            y);
        exit(1);
    }
}

static void require_manifold(
    const CoreCollision2DManifold* manifold,
    int body_a_id,
    int body_b_id,
    CoreCollision2DShapeKind shape_a_kind,
    CoreCollision2DShapeKind shape_b_kind,
    double normal_x,
    double normal_y,
    double depth,
    int point_count,
    const char* message) {
    require_true(core_collision2d_manifold_validate(manifold), message);
    require_true(manifold->body_a_id == body_a_id, "body a id");
    require_true(manifold->body_b_id == body_b_id, "body b id");
    require_true(manifold->shape_a_kind == shape_a_kind, "shape a kind");
    require_true(manifold->shape_b_kind == shape_b_kind, "shape b kind");
    require_vec_near(manifold->normal_from_a_to_b, normal_x, normal_y, 1.0e-9, "normal");
    require_near(manifold->penetration_depth, depth, 1.0e-9, "depth");
    require_true(manifold->contact_point_count == point_count, "contact point count");
}

static void check_circle_contact_edge_table(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();

    require_true(
        core_collision2d_contact_circle_circle(
            1,
            2,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(3.1, 0.0),
            1.0,
            &manifold) == 0,
        "separated circles have no contact");
    require_true(
        core_collision2d_contact_circle_circle(
            1,
            2,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(2.0, 0.0),
            1.0,
            &manifold) == 1,
        "touching circles contact");
    require_manifold(
        &manifold,
        1,
        2,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        1.0,
        0.0,
        0.0,
        1,
        "touching circles manifold");
    require_vec_near(manifold.contact_points[0], 1.0, 0.0, 1.0e-9, "touching circles point");

    require_true(
        core_collision2d_contact_circle_circle(
            1,
            2,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(1.5, 0.0),
            1.0,
            &manifold) == 1,
        "shallow overlap contact");
    require_manifold(
        &manifold,
        1,
        2,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        1.0,
        0.0,
        0.5,
        1,
        "shallow overlap manifold");
    require_vec_near(manifold.contact_points[0], 0.75, 0.0, 1.0e-9, "shallow overlap point");

    require_true(
        core_collision2d_contact_circle_circle(
            3,
            4,
            core_collision2d_vec2(1.0, 1.0),
            1.0,
            core_collision2d_vec2(2.0, 2.0),
            1.0,
            &manifold) == 1,
        "diagonal overlap contact");
    require_manifold(
        &manifold,
        3,
        4,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        0.7071067811865475,
        0.7071067811865475,
        0.5857864376269049,
        1,
        "diagonal overlap manifold");
    require_vec_near(manifold.contact_points[0], 1.5, 1.5, 1.0e-9, "diagonal overlap point");

    require_true(
        core_collision2d_contact_circle_circle(
            5,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            &manifold) == 1,
        "unequal radii overlap contact");
    require_manifold(
        &manifold,
        5,
        6,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        1.0,
        0.0,
        0.5,
        1,
        "unequal radii manifold");
    require_vec_near(manifold.contact_points[0], 0.25, 0.0, 1.0e-9, "unequal radii point");

    require_true(
        core_collision2d_contact_circle_circle(
            7,
            8,
            core_collision2d_vec2(1.0, 1.0),
            1.0,
            core_collision2d_vec2(1.0, 1.0),
            1.0,
            &manifold) == 0,
        "coincident centers do not choose arbitrary normal");
}

static void check_circle_contact_invalid_inputs(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();

    require_true(
        core_collision2d_contact_circle_circle(
            -1,
            2,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            &manifold) < 0,
        "negative circle body id rejects");
    require_true(
        core_collision2d_contact_circle_circle(
            1,
            1,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            &manifold) < 0,
        "same circle body id rejects");
    require_true(
        core_collision2d_contact_circle_circle(
            1,
            2,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            &manifold) < 0,
        "zero circle radius rejects");
    require_true(
        core_collision2d_contact_circle_circle(
            1,
            2,
            core_collision2d_vec2(NAN, 0.0),
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            &manifold) < 0,
        "non-finite circle center rejects");
    require_true(
        core_collision2d_contact_circle_circle(
            1,
            2,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            NULL) < 0,
        "null circle manifold rejects");
}

static void check_circle_contact_metamorphic(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
    const CoreCollision2DVec2 center_a = core_collision2d_vec2(0.0, 0.0);
    const CoreCollision2DVec2 center_b = core_collision2d_vec2(3.0, 4.0);
    const double radius_a = 2.0;
    const double radius_b = 4.0;

    require_true(
        core_collision2d_contact_circle_circle(10, 11, center_a, radius_a, center_b, radius_b, &manifold) == 1,
        "base circle metamorphic contact");
    require_manifold(
        &manifold,
        10,
        11,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        0.6,
        0.8,
        1.0,
        1,
        "base circle metamorphic manifold");
    require_vec_near(manifold.contact_points[0], 0.9, 1.2, 1.0e-9, "base circle contact point");

    const CoreCollision2DVec2 translation = core_collision2d_vec2(10.0, -2.0);
    require_true(
        core_collision2d_contact_circle_circle(
            10,
            11,
            core_collision2d_vec2_add(center_a, translation),
            radius_a,
            core_collision2d_vec2_add(center_b, translation),
            radius_b,
            &manifold) == 1,
        "translated circle contact");
    require_manifold(
        &manifold,
        10,
        11,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        0.6,
        0.8,
        1.0,
        1,
        "translated circle manifold");
    require_vec_near(manifold.contact_points[0], 10.9, -0.8, 1.0e-9, "translated circle point");

    require_true(
        core_collision2d_contact_circle_circle(
            10,
            11,
            core_collision2d_vec2_scale(center_a, 2.0),
            radius_a * 2.0,
            core_collision2d_vec2_scale(center_b, 2.0),
            radius_b * 2.0,
            &manifold) == 1,
        "scaled circle contact");
    require_manifold(
        &manifold,
        10,
        11,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        0.6,
        0.8,
        2.0,
        1,
        "scaled circle manifold");
    require_vec_near(manifold.contact_points[0], 1.8, 2.4, 1.0e-9, "scaled circle point");

    require_true(
        core_collision2d_contact_circle_circle(11, 10, center_b, radius_b, center_a, radius_a, &manifold) == 1,
        "swapped circle contact");
    require_manifold(
        &manifold,
        11,
        10,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        -0.6,
        -0.8,
        1.0,
        1,
        "swapped circle manifold");
    require_vec_near(manifold.contact_points[0], 0.9, 1.2, 1.0e-9, "swapped circle point");
}

static void check_box_contact_primitive_edges(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(2.0, 0.0),
            1.0,
            1.0,
            &manifold) == 1,
        "touching boxes contact");
    require_manifold(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        1.0,
        0.0,
        0.0,
        2,
        "touching boxes manifold");
    require_vec_near(manifold.contact_points[0], 1.0, -1.0, 1.0e-9, "touching boxes p0");
    require_vec_near(manifold.contact_points[1], 1.0, 1.0, 1.0e-9, "touching boxes p1");

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(2.1, 0.0),
            1.0,
            1.0,
            &manifold) == 0,
        "separated boxes miss");

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(5.0, -3.0),
            1.0,
            1.0,
            core_collision2d_vec2(6.5, -3.0),
            1.0,
            1.0,
            &manifold) == 1,
        "translated boxes contact");
    require_manifold(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        1.0,
        0.0,
        0.5,
        2,
        "translated boxes manifold");
    require_vec_near(manifold.contact_points[0], 5.75, -4.0, 1.0e-9, "translated boxes p0");
    require_vec_near(manifold.contact_points[1], 5.75, -2.0, 1.0e-9, "translated boxes p1");
}

static void check_box_contact_tie_swapped_and_containment(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            22,
            23,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(0.5, 0.5),
            1.0,
            1.0,
            &manifold) == 1,
        "axis-tie boxes contact");
    require_manifold(
        &manifold,
        22,
        23,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        1.0,
        0.0,
        1.5,
        2,
        "axis-tie boxes prefer x axis");
    require_vec_near(manifold.contact_points[0], 0.25, -0.5, 1.0e-9, "axis-tie boxes p0");
    require_vec_near(manifold.contact_points[1], 0.25, 1.0, 1.0e-9, "axis-tie boxes p1");

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            11,
            10,
            core_collision2d_vec2(0.5, 0.0),
            0.5,
            0.5,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            0.5,
            &manifold) == 1,
        "swapped boxes contact");
    require_manifold(
        &manifold,
        11,
        10,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        -1.0,
        0.0,
        0.5,
        2,
        "swapped boxes manifold");
    require_vec_near(manifold.contact_points[0], 0.25, -0.5, 1.0e-9, "swapped boxes p0");
    require_vec_near(manifold.contact_points[1], 0.25, 0.5, 1.0e-9, "swapped boxes p1");

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            24,
            25,
            core_collision2d_vec2(0.0, 0.0),
            2.0,
            1.5,
            core_collision2d_vec2(0.25, 0.0),
            0.5,
            0.25,
            &manifold) == 1,
        "contained boxes contact");
    require_manifold(
        &manifold,
        24,
        25,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        0.0,
        1.0,
        0.5,
        2,
        "contained boxes min-axis manifold");
    require_vec_near(manifold.contact_points[0], -0.25, 0.625, 1.0e-9, "contained boxes p0");
    require_vec_near(manifold.contact_points[1], 0.75, 0.625, 1.0e-9, "contained boxes p1");
}

static void check_box_contact_invalid_inputs(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            -1,
            21,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            1.0,
            &manifold) < 0,
        "negative box body id rejects");
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            20,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            1.0,
            &manifold) < 0,
        "same box body id rejects");
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            1.0,
            &manifold) < 0,
        "zero box half width rejects");
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            -1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            1.0,
            &manifold) < 0,
        "negative box half height rejects");
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(NAN, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            1.0,
            &manifold) < 0,
        "non-finite box center rejects");
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            20,
            21,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            1.0,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            1.0,
            NULL) < 0,
        "null box manifold rejects");
}

static void check_polygon_degenerate_and_winding_edges(void) {
    const CoreCollision2DVec2 hex_vertices[6] = {
        {0.8660254037844386, -0.5},
        {0.8660254037844386, 0.5},
        {0.0, 1.0},
        {-0.8660254037844386, 0.5},
        {-0.8660254037844386, -0.5},
        {0.0, -1.0},
    };
    const CoreCollision2DVec2 square_ccw[4] = {
        {-1.0, -1.0},
        {1.0, -1.0},
        {1.0, 1.0},
        {-1.0, 1.0},
    };
    const CoreCollision2DVec2 square_cw[4] = {
        {-1.0, -1.0},
        {-1.0, 1.0},
        {1.0, 1.0},
        {1.0, -1.0},
    };
    const CoreCollision2DVec2 duplicate_vertex[4] = {
        {-1.0, -1.0},
        {1.0, -1.0},
        {1.0, -1.0},
        {-1.0, 1.0},
    };
    const CoreCollision2DVec2 collinear[3] = {
        {-1.0, 0.0},
        {0.0, 0.0},
        {1.0, 0.0},
    };
    CoreCollision2DVec2 centroid = core_collision2d_vec2(0.0, 0.0);
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();

    require_true(core_collision2d_polygon_is_convex(square_ccw, 4), "ccw square convex");
    require_true(core_collision2d_polygon_is_convex(square_cw, 4), "cw square convex");
    require_true(core_collision2d_polygon_centroid(square_ccw, 4, &centroid), "ccw square centroid");
    require_vec_near(centroid, 0.0, 0.0, 1.0e-9, "ccw centroid value");
    require_true(core_collision2d_polygon_centroid(square_cw, 4, &centroid), "cw square centroid");
    require_vec_near(centroid, 0.0, 0.0, 1.0e-9, "cw centroid value");
    require_true(!core_collision2d_polygon_is_convex(duplicate_vertex, 4), "duplicate vertex rejects");
    require_true(!core_collision2d_polygon_is_convex(collinear, 3), "collinear polygon rejects");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            21,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(2.0, 0.0),
            0.0,
            &manifold) == 0,
        "separated hex polygons miss");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            21,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(1.7320508075688772 + 1.0e-6, 0.0),
            0.0,
            &manifold) == 0,
        "near-separated hex polygons miss");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            21,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(1.7320508075688772, 0.0),
            0.0,
            &manifold) == 1,
        "touching hex polygons contact");
    require_manifold(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        -0.0,
        0.0,
        2,
        "touching hex polygon manifold");
    require_vec_near(manifold.contact_points[0], 0.8660254037844386, -0.5, 1.0e-9, "touching hex polygon p0");
    require_vec_near(manifold.contact_points[1], 0.8660254037844386, 0.5, 1.0e-9, "touching hex polygon p1");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            21,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(1.2320508075688772, 0.0),
            0.0,
            &manifold) == 1,
        "overlapping hex polygons contact");
    require_manifold(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        -0.0,
        0.5,
        2,
        "overlapping hex polygon manifold");
    require_vec_near(manifold.contact_points[0], 0.6160254037844386, -0.5, 1.0e-9, "overlapping hex polygon p0");
    require_vec_near(manifold.contact_points[1], 0.6160254037844386, 0.5, 1.0e-9, "overlapping hex polygon p1");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            30,
            31,
            square_ccw,
            4,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            square_cw,
            4,
            core_collision2d_vec2(1.5, 0.0),
            0.0,
            &manifold) == 1,
        "mixed winding polygon contact");
    require_manifold(
        &manifold,
        30,
        31,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        -0.0,
        0.5,
        2,
        "mixed winding polygon manifold");
    require_vec_near(manifold.contact_points[0], 0.75, -1.0, 1.0e-9, "mixed winding polygon p0");
    require_vec_near(manifold.contact_points[1], 0.75, 1.0, 1.0e-9, "mixed winding polygon p1");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            21,
            hex_vertices,
            6,
            core_collision2d_vec2(3.0, -2.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(4.2320508075688772, -2.0),
            0.0,
            &manifold) == 1,
        "translated hex polygons contact");
    require_manifold(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        -0.0,
        0.5,
        2,
        "translated hex polygon manifold");
    require_vec_near(manifold.contact_points[0], 3.6160254037844386, -2.5, 1.0e-9, "translated hex polygon p0");
    require_vec_near(manifold.contact_points[1], 3.6160254037844386, -1.5, 1.0e-9, "translated hex polygon p1");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            21,
            20,
            hex_vertices,
            6,
            core_collision2d_vec2(1.2320508075688772, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            &manifold) == 1,
        "swapped hex polygons contact");
    require_manifold(
        &manifold,
        21,
        20,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        -1.0,
        0.0,
        0.5,
        2,
        "swapped hex polygon manifold");
    require_vec_near(manifold.contact_points[0], 0.6160254037844386, 0.5, 1.0e-9, "swapped hex polygon p0");
    require_vec_near(manifold.contact_points[1], 0.6160254037844386, -0.5, 1.0e-9, "swapped hex polygon p1");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            40,
            41,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.25,
            hex_vertices,
            6,
            core_collision2d_vec2(1.15, 0.05),
            -0.15,
            &manifold) == 1,
        "rotated hex polygons contact");
    require_manifold(
        &manifold,
        40,
        41,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        0.9887710779360422,
        -0.14943813247359922,
        0.7287819611767538,
        1,
        "rotated hex polygon manifold");
    require_vec_near(manifold.contact_points[0], 0.5908924063071473, -0.2925832085479343, 1.0e-9, "rotated hex polygon p0");

    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            30,
            31,
            square_ccw,
            4,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            duplicate_vertex,
            4,
            core_collision2d_vec2(1.5, 0.0),
            0.0,
            &manifold) < 0,
        "duplicate vertex contact rejects");
}

int main(void) {
    check_circle_contact_edge_table();
    check_circle_contact_invalid_inputs();
    check_circle_contact_metamorphic();
    check_box_contact_primitive_edges();
    check_box_contact_tie_swapped_and_containment();
    check_box_contact_invalid_inputs();
    check_polygon_degenerate_and_winding_edges();
    puts("core_collision2d hardening tests passed");
    return 0;
}
