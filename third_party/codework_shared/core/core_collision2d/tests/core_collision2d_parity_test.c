#include "core_collision2d.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void require_shape_kind_name(CoreCollision2DShapeKind kind, const char* expected, const char* message) {
    const char* actual = core_collision2d_shape_kind_name(kind);
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s actual=%s expected=%s\n", message, actual, expected);
        exit(1);
    }
}

static void require_manifold_header(
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
    require_true(manifold->body_a_id == body_a_id, "body a id parity");
    require_true(manifold->body_b_id == body_b_id, "body b id parity");
    require_true(manifold->shape_a_kind == shape_a_kind, "shape a kind parity");
    require_true(manifold->shape_b_kind == shape_b_kind, "shape b kind parity");
    require_vec_near(manifold->normal_from_a_to_b, normal_x, normal_y, 1.0e-9, "normal parity");
    require_near(manifold->penetration_depth, depth, 1.0e-9, "depth parity");
    require_true(manifold->contact_point_count == point_count, "point count parity");
}

static void check_shape_contract_parity(void) {
    require_shape_kind_name(CORE_COLLISION2D_SHAPE_CIRCLE, "circle", "circle name");
    require_shape_kind_name(CORE_COLLISION2D_SHAPE_BOX, "box", "box name");
    require_shape_kind_name(CORE_COLLISION2D_SHAPE_CONVEX_POLYGON, "convex_polygon", "convex polygon name");
    require_shape_kind_name((CoreCollision2DShapeKind)99, "unknown", "unknown shape name");

    CoreCollision2DAabb aabb = {0};
    CoreCollision2DShape circle = core_collision2d_shape_circle_offset(1.25, core_collision2d_vec2(2.0, -3.0));
    require_true(core_collision2d_shape_validate(&circle), "circle validates");
    require_true(core_collision2d_shape_aabb(&circle, &aabb), "circle aabb generated");
    require_near(aabb.min_x, 0.75, 1.0e-7, "circle min x");
    require_near(aabb.min_y, -4.25, 1.0e-7, "circle min y");
    require_near(aabb.max_x, 3.25, 1.0e-7, "circle max x");
    require_near(aabb.max_y, -1.75, 1.0e-7, "circle max y");
    require_near(core_collision2d_aabb_width(&aabb), 2.5, 1.0e-7, "circle aabb width");
    require_near(core_collision2d_aabb_height(&aabb), 2.5, 1.0e-7, "circle aabb height");

    require_true(!core_collision2d_shape_validate(&((CoreCollision2DShape){0})), "zero shape rejects");
    CoreCollision2DShape zero_circle = core_collision2d_shape_circle(0.0);
    CoreCollision2DShape negative_circle = core_collision2d_shape_circle(-1.0);
    CoreCollision2DShape bad_circle = core_collision2d_shape_circle(NAN);
    require_true(!core_collision2d_shape_validate(&zero_circle), "zero circle rejects");
    require_true(!core_collision2d_shape_validate(&negative_circle), "negative circle rejects");
    require_true(!core_collision2d_shape_validate(&bad_circle), "nan circle rejects");

    CoreCollision2DShape box = core_collision2d_shape_box_offset(3.0, 0.5, core_collision2d_vec2(-2.0, 4.0));
    require_true(core_collision2d_shape_validate(&box), "box validates");
    require_true(core_collision2d_shape_aabb(&box, &aabb), "box aabb generated");
    require_near(aabb.min_x, -5.0, 1.0e-7, "box min x");
    require_near(aabb.min_y, 3.5, 1.0e-7, "box min y");
    require_near(aabb.max_x, 1.0, 1.0e-7, "box max x");
    require_near(aabb.max_y, 4.5, 1.0e-7, "box max y");
    require_near(core_collision2d_aabb_width(&aabb), 6.0, 1.0e-7, "box aabb width");
    require_near(core_collision2d_aabb_height(&aabb), 1.0, 1.0e-7, "box aabb height");

    CoreCollision2DShape zero_width_box = core_collision2d_shape_box(0.0, 1.0);
    CoreCollision2DShape infinite_height_box = core_collision2d_shape_box(1.0, INFINITY);
    require_true(!core_collision2d_shape_validate(&zero_width_box), "zero width box rejects");
    require_true(!core_collision2d_shape_validate(&infinite_height_box), "infinite height box rejects");

    const CoreCollision2DVec2 triangle[] = {
        {-1.0, 0.0},
        {2.0, -0.5},
        {0.5, 3.0},
    };
    CoreCollision2DShape polygon = core_collision2d_shape_convex_polygon_offset(
        triangle,
        3,
        core_collision2d_vec2(10.0, -2.0));
    require_true(core_collision2d_shape_validate(&polygon), "polygon descriptor validates");
    require_true(core_collision2d_shape_aabb(&polygon, &aabb), "polygon descriptor aabb generated");
    require_near(aabb.min_x, 9.0, 1.0e-7, "polygon min x");
    require_near(aabb.min_y, -2.5, 1.0e-7, "polygon min y");
    require_near(aabb.max_x, 12.0, 1.0e-7, "polygon max x");
    require_near(aabb.max_y, 1.0, 1.0e-7, "polygon max y");
    CoreCollision2DShape short_polygon = core_collision2d_shape_convex_polygon(triangle, 2);
    CoreCollision2DShape null_polygon = core_collision2d_shape_convex_polygon(NULL, 3);
    require_true(!core_collision2d_shape_validate(&short_polygon), "short polygon rejects");
    require_true(!core_collision2d_shape_validate(&null_polygon), "null polygon rejects");
}

static void check_polygon_shape_parity(void) {
    const CoreCollision2DVec2 fixture[] = {
        {0.0, -0.5},
        {1.0, -0.25},
        {0.75, 0.55},
        {-0.55, 0.35},
        {-0.8, -0.25},
    };
    CoreCollision2DAabb aabb = {0};
    CoreCollision2DShape polygon = core_collision2d_shape_convex_polygon_offset(
        fixture,
        5,
        core_collision2d_vec2(2.0, -1.0));
    require_true(core_collision2d_shape_validate(&polygon), "asymmetric fixture validates");
    require_true(core_collision2d_shape_aabb(&polygon, &aabb), "asymmetric fixture aabb");
    require_near(aabb.min_x, 1.2, 1.0e-7, "fixture min x");
    require_near(aabb.min_y, -1.5, 1.0e-7, "fixture min y");
    require_near(aabb.max_x, 3.0, 1.0e-7, "fixture max x");
    require_near(aabb.max_y, -0.45, 1.0e-7, "fixture max y");

    const CoreCollision2DVec2 concave[] = {
        {-1.0, -1.0},
        {1.0, -1.0},
        {0.0, 0.0},
        {1.0, 1.0},
        {-1.0, 1.0},
    };
    CoreCollision2DShape concave_shape = core_collision2d_shape_convex_polygon(concave, 5);
    require_true(!core_collision2d_shape_validate(&concave_shape), "concave polygon rejects");

    const CoreCollision2DVec2 collinear[] = {
        {-1.0, 0.0},
        {0.0, 0.0},
        {1.0, 0.0},
    };
    CoreCollision2DShape line_shape = core_collision2d_shape_convex_polygon(collinear, 3);
    require_true(!core_collision2d_shape_validate(&line_shape), "collinear polygon rejects");

    CoreCollision2DVec2 too_many[CORE_COLLISION2D_MAX_POLYGON_VERTICES + 1] = {0};
    for (int i = 0; i < CORE_COLLISION2D_MAX_POLYGON_VERTICES + 1; ++i) {
        too_many[i] = core_collision2d_vec2(cos((double)i), sin((double)i));
    }
    CoreCollision2DShape over_limit = core_collision2d_shape_convex_polygon(
        too_many,
        CORE_COLLISION2D_MAX_POLYGON_VERTICES + 1);
    require_true(!core_collision2d_shape_validate(&over_limit), "over-limit polygon rejects");
}

static void check_geometry_contract_parity(void) {
    const CoreCollision2DVec2 polygon[5] = {
        core_collision2d_vec2(0.0, -0.5),
        core_collision2d_vec2(1.0, -0.25),
        core_collision2d_vec2(0.75, 0.55),
        core_collision2d_vec2(-0.55, 0.35),
        core_collision2d_vec2(-0.8, -0.25),
    };
    CoreCollision2DVec2 centroid = {0};
    CoreCollision2DVec2 normal = {0};
    CoreCollision2DProjection projection = {0};
    CoreCollision2DVec2 world[5] = {0};

    require_near(core_collision2d_polygon_signed_area(polygon, 5), 1.31, 1.0e-9, "signed area");
    require_near(core_collision2d_polygon_area(polygon, 5), 1.31, 1.0e-9, "absolute area");
    require_true(core_collision2d_polygon_centroid(polygon, 5, &centroid), "centroid computes");
    require_near(centroid.x, 0.12977099236641224, 1.0e-9, "centroid x");
    require_near(centroid.y, 0.012277353689567427, 1.0e-9, "centroid y");
    require_true(core_collision2d_polygon_is_convex(polygon, 5), "polygon is convex");
    require_true(core_collision2d_polygon_edge_left_normal(polygon, 5, 0, &normal), "edge normal computes");
    require_near(normal.x, -0.24253562503633297, 1.0e-9, "normal x");
    require_near(normal.y, 0.9701425001453319, 1.0e-9, "normal y");
    require_true(core_collision2d_project_points(polygon, 5, normal, &projection), "projection computes");
    require_near(projection.min, -0.48507125007266594, 1.0e-9, "projection min");
    require_near(projection.max, 0.4729444688208493, 1.0e-9, "projection max");
    require_true(
        core_collision2d_transform_polygon(
            polygon,
            5,
            core_collision2d_vec2(2.0, 3.0),
            0.52359877559829887308,
            world,
            5) == 5,
        "world transform count");
    require_near(world[0].x, 2.25, 1.0e-9, "world vertex 0 x");
    require_near(world[0].y, 2.566987298107781, 1.0e-9, "world vertex 0 y");
    require_near(world[3].x, 1.3486860279185588, 1.0e-9, "world vertex 3 x");
    require_near(world[3].y, 3.0281088913245537, 1.0e-9, "world vertex 3 y");

    const CoreCollision2DVec2 concave[5] = {
        core_collision2d_vec2(0.0, 0.0),
        core_collision2d_vec2(1.0, 0.0),
        core_collision2d_vec2(0.25, 0.25),
        core_collision2d_vec2(1.0, 1.0),
        core_collision2d_vec2(0.0, 1.0),
    };
    const CoreCollision2DVec2 collinear[3] = {
        core_collision2d_vec2(0.0, 0.0),
        core_collision2d_vec2(1.0, 0.0),
        core_collision2d_vec2(2.0, 0.0),
    };
    require_true(!core_collision2d_polygon_is_convex(concave, 5), "concave polygon rejected");
    require_true(!core_collision2d_polygon_is_convex(collinear, 3), "collinear polygon rejected");
    require_true(!core_collision2d_polygon_centroid(collinear, 3, &centroid), "collinear centroid rejected");
    require_true(
        !core_collision2d_project_points(concave, 5, core_collision2d_vec2(0.0, 0.0), &projection),
        "zero axis projection rejected");
}

static void check_manifold_contract_parity(void) {
    const CoreCollision2DManifold zero = core_collision2d_manifold_zero();
    require_true(!core_collision2d_manifold_validate(&zero), "zero manifold rejects");

    CoreCollision2DManifold manifold = {0};
    const CoreCollision2DVec2 single_point = core_collision2d_vec2(1.25, 2.50);
    require_true(
        core_collision2d_manifold_init(
            &manifold,
            3,
            8,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.125,
            &single_point,
            1),
        "single point init");
    require_manifold_header(
        &manifold,
        3,
        8,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_BOX,
        0.0,
        1.0,
        0.125,
        1,
        "single point validates");
    require_vec_near(manifold.contact_points[0], 1.25, 2.50, 1.0e-9, "single point stable");

    const CoreCollision2DVec2 points[] = {
        core_collision2d_vec2(-0.45, 0.25),
        core_collision2d_vec2(0.45, 0.25),
    };
    require_true(
        core_collision2d_manifold_init(
            &manifold,
            5,
            12,
            CORE_COLLISION2D_SHAPE_BOX,
            CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
            core_collision2d_vec2(1.0, 0.0),
            0.250,
            points,
            2),
        "two point init");
    require_manifold_header(
        &manifold,
        5,
        12,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        0.0,
        0.250,
        2,
        "two point validates");
    require_vec_near(manifold.contact_points[0], -0.45, 0.25, 1.0e-9, "two point p0 stable");
    require_vec_near(manifold.contact_points[1], 0.45, 0.25, 1.0e-9, "two point p1 stable");

    const CoreCollision2DVec2 too_many_points[] = {
        core_collision2d_vec2(0.0, 0.0),
        core_collision2d_vec2(1.0, 0.0),
        core_collision2d_vec2(2.0, 0.0),
    };
    const CoreCollision2DVec2 bad_point = core_collision2d_vec2(NAN, 0.0);
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            -1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.0,
            &single_point,
            1),
        "negative body id rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            2,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.0,
            &single_point,
            1),
        "same body id rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_INVALID,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.0,
            &single_point,
            1),
        "invalid shape kind rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 2.0),
            0.0,
            &single_point,
            1),
        "non-unit normal rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            -0.001,
            &single_point,
            1),
        "negative penetration rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.0,
            &single_point,
            0),
        "zero contact points rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.0,
            too_many_points,
            3),
        "too many contact points rejects");
    require_true(
        !core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.0,
            &bad_point,
            1),
        "non-finite contact point rejects");
}

static void check_circle_contact_parity(void) {
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
        "touching circles produce contact");
    require_manifold_header(
        &manifold,
        1,
        2,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        1.0,
        0.0,
        0.0,
        1,
        "touching circles");
    require_vec_near(manifold.contact_points[0], 1.0, 0.0, 1.0e-9, "touching circle point");

    require_true(
        core_collision2d_contact_circle_circle(
            3,
            4,
            core_collision2d_vec2(1.0, 1.0),
            1.0,
            core_collision2d_vec2(2.0, 2.0),
            1.0,
            &manifold) == 1,
        "deep diagonal overlap count");
    require_manifold_header(
        &manifold,
        3,
        4,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        0.7071067811865475,
        0.7071067811865475,
        0.5857864376269049,
        1,
        "deep diagonal overlap");
    require_vec_near(manifold.contact_points[0], 1.5, 1.5, 1.0e-9, "deep diagonal point");

    require_true(
        core_collision2d_contact_circle_circle(
            5,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            &manifold) == 1,
        "unequal radii count");
    require_manifold_header(
        &manifold,
        5,
        6,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        1.0,
        0.0,
        0.5,
        1,
        "unequal radii overlap");
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
        "coincident centers guardrail");
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
        "zero radius rejects");
}

static void check_box_contact_parity(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            10,
            11,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            0.5,
            core_collision2d_vec2(1.1, 0.0),
            0.5,
            0.5,
            &manifold) == 0,
        "separated boxes have no contact");
    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            10,
            11,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            0.5,
            core_collision2d_vec2(1.0, 0.0),
            0.5,
            0.5,
            &manifold) == 1,
        "touching boxes produce contact");
    require_manifold_header(
        &manifold,
        10,
        11,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        1.0,
        0.0,
        0.0,
        2,
        "touching boxes");
    require_vec_near(manifold.contact_points[0], 0.5, -0.5, 1.0e-9, "touching box p0");
    require_vec_near(manifold.contact_points[1], 0.5, 0.5, 1.0e-9, "touching box p1");

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            10,
            11,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            0.5,
            core_collision2d_vec2(0.5, 0.0),
            0.5,
            0.5,
            &manifold) == 1,
        "horizontal overlap contact count");
    require_manifold_header(
        &manifold,
        10,
        11,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        1.0,
        0.0,
        0.5,
        2,
        "horizontal overlap");
    require_vec_near(manifold.contact_points[0], 0.25, -0.5, 1.0e-9, "horizontal box p0");
    require_vec_near(manifold.contact_points[1], 0.25, 0.5, 1.0e-9, "horizontal box p1");

    require_true(
        core_collision2d_contact_box_box_axis_aligned(
            12,
            13,
            core_collision2d_vec2(0.0, 0.0),
            0.5,
            0.5,
            core_collision2d_vec2(0.0, -0.75),
            0.5,
            0.5,
            &manifold) == 1,
        "vertical overlap contact count");
    require_manifold_header(
        &manifold,
        12,
        13,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        0.0,
        -1.0,
        0.25,
        2,
        "vertical overlap");
    require_vec_near(manifold.contact_points[0], -0.5, -0.375, 1.0e-9, "vertical box p0");
    require_vec_near(manifold.contact_points[1], 0.5, -0.375, 1.0e-9, "vertical box p1");
}

static void check_polygon_contact_parity(void) {
    const CoreCollision2DVec2 hex_vertices[6] = {
        {0.8660254037844386, -0.5},
        {0.8660254037844386, 0.5},
        {0.0, 1.0},
        {-0.8660254037844386, 0.5},
        {-0.8660254037844386, -0.5},
        {0.0, -1.0},
    };
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
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
        "separated hexagons have no contact");
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
        "touching hexagons produce contact");
    require_manifold_header(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        -0.0,
        0.0,
        2,
        "touching hexagons");
    require_vec_near(manifold.contact_points[0], 0.8660254037844386, -0.5, 1.0e-9, "touching hex p0");
    require_vec_near(manifold.contact_points[1], 0.8660254037844386, 0.5, 1.0e-9, "touching hex p1");

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
        "horizontal hex overlap count");
    require_manifold_header(
        &manifold,
        20,
        21,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        CORE_COLLISION2D_SHAPE_CONVEX_POLYGON,
        1.0,
        -0.0,
        0.5,
        2,
        "horizontal hex overlap");
    require_vec_near(manifold.contact_points[0], 0.6160254037844386, -0.5, 1.0e-9, "horizontal hex p0");
    require_vec_near(manifold.contact_points[1], 0.6160254037844386, 0.5, 1.0e-9, "horizontal hex p1");

    const CoreCollision2DVec2 concave[5] = {
        {0.0, 0.0},
        {1.0, 0.0},
        {0.25, 0.25},
        {1.0, 1.0},
        {0.0, 1.0},
    };
    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            -1,
            21,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(1.0, 0.0),
            0.0,
            &manifold) < 0,
        "negative polygon body id rejects");
    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            20,
            hex_vertices,
            6,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(1.0, 0.0),
            0.0,
            &manifold) < 0,
        "same polygon body id rejects");
    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            20,
            21,
            concave,
            5,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            hex_vertices,
            6,
            core_collision2d_vec2(1.0, 0.0),
            0.0,
            &manifold) < 0,
        "concave polygon contact rejects");
}

int main(void) {
    check_shape_contract_parity();
    check_polygon_shape_parity();
    check_geometry_contract_parity();
    check_manifold_contract_parity();
    check_circle_contact_parity();
    check_box_contact_parity();
    check_polygon_contact_parity();
    puts("core_collision2d parity tests passed");
    return 0;
}
