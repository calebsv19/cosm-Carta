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
    if (fabs(actual - expected) > epsilon) {
        fprintf(stderr, "FAIL: %s actual=%.12f expected=%.12f\n", message, actual, expected);
        exit(1);
    }
}

static void require_vec_near(CoreCollision2DVec2 actual, double x, double y, double epsilon, const char* message) {
    if (fabs(actual.x - x) > epsilon || fabs(actual.y - y) > epsilon) {
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

static void test_vectors_and_geometry(void) {
    CoreCollision2DVec2 normalized = {0};
    const CoreCollision2DVec2 a = core_collision2d_vec2(3.0, 4.0);
    const CoreCollision2DVec2 b = core_collision2d_vec2(-1.0, 2.0);
    const CoreCollision2DVec2 tri[3] = {
        {0.0, 0.0},
        {2.0, 0.0},
        {0.0, 2.0},
    };
    CoreCollision2DVec2 centroid = {0};
    CoreCollision2DVec2 normal = {0};
    CoreCollision2DProjection projection = {0};
    CoreCollision2DVec2 transformed[3];

    require_vec_near(core_collision2d_vec2_add(a, b), 2.0, 6.0, 1.0e-9, "vec add");
    require_vec_near(core_collision2d_vec2_sub(a, b), 4.0, 2.0, 1.0e-9, "vec sub");
    require_vec_near(core_collision2d_vec2_scale(a, 2.0), 6.0, 8.0, 1.0e-9, "vec scale");
    require_near(core_collision2d_vec2_dot(a, b), 5.0, 1.0e-9, "vec dot");
    require_near(core_collision2d_vec2_cross(a, b), 10.0, 1.0e-9, "vec cross");
    require_near(core_collision2d_vec2_length(a), 5.0, 1.0e-9, "vec length");
    require_true(core_collision2d_vec2_normalize(a, &normalized), "vec normalize succeeds");
    require_vec_near(normalized, 0.6, 0.8, 1.0e-9, "vec normalized value");

    require_near(core_collision2d_polygon_signed_area(tri, 3), 2.0, 1.0e-9, "triangle signed area");
    require_near(core_collision2d_polygon_area(tri, 3), 2.0, 1.0e-9, "triangle area");
    require_true(core_collision2d_polygon_centroid(tri, 3, &centroid), "triangle centroid succeeds");
    require_vec_near(centroid, 2.0 / 3.0, 2.0 / 3.0, 1.0e-9, "triangle centroid");
    require_true(core_collision2d_polygon_is_convex(tri, 3), "triangle convex");
    require_true(core_collision2d_polygon_edge_left_normal(tri, 3, 0, &normal), "edge normal succeeds");
    require_vec_near(normal, 0.0, 1.0, 1.0e-9, "edge normal");
    require_true(
        core_collision2d_project_points(tri, 3, core_collision2d_vec2(1.0, 0.0), &projection),
        "project points succeeds");
    require_near(projection.min, 0.0, 1.0e-9, "projection min");
    require_near(projection.max, 2.0, 1.0e-9, "projection max");
    require_true(
        core_collision2d_transform_polygon(
            tri,
            3,
            core_collision2d_vec2(1.0, 1.0),
            0.0,
            transformed,
            3) == 3,
        "transform polygon count");
    require_vec_near(transformed[1], 3.0, 1.0, 1.0e-9, "transform polygon point");
}

static void test_shapes_and_aabb(void) {
    const CoreCollision2DVec2 tri[3] = {
        {-1.0, 0.0},
        {1.0, 0.0},
        {0.0, 2.0},
    };
    CoreCollision2DAabb aabb = {0};

    CoreCollision2DShape circle = core_collision2d_shape_circle_offset(
        2.0,
        core_collision2d_vec2(3.0, 4.0));
    require_true(core_collision2d_shape_validate(&circle), "circle shape validates");
    require_true(core_collision2d_shape_aabb(&circle, &aabb), "circle aabb succeeds");
    require_near(aabb.min_x, 1.0, 1.0e-9, "circle aabb min x");
    require_near(aabb.max_y, 6.0, 1.0e-9, "circle aabb max y");
    require_near(core_collision2d_aabb_width(&aabb), 4.0, 1.0e-9, "circle aabb width");

    CoreCollision2DShape box = core_collision2d_shape_box_offset(
        1.5,
        2.0,
        core_collision2d_vec2(-2.0, 5.0));
    require_true(core_collision2d_shape_validate(&box), "box shape validates");
    require_true(core_collision2d_shape_aabb(&box, &aabb), "box aabb succeeds");
    require_near(aabb.min_x, -3.5, 1.0e-9, "box aabb min x");
    require_near(aabb.max_y, 7.0, 1.0e-9, "box aabb max y");
    require_near(core_collision2d_aabb_height(&aabb), 4.0, 1.0e-9, "box aabb height");

    CoreCollision2DShape polygon = core_collision2d_shape_convex_polygon_offset(
        tri,
        3,
        core_collision2d_vec2(10.0, -1.0));
    require_true(core_collision2d_shape_validate(&polygon), "polygon shape validates");
    require_true(core_collision2d_shape_aabb(&polygon, &aabb), "polygon aabb succeeds");
    require_near(aabb.min_x, 9.0, 1.0e-9, "polygon aabb min x");
    require_near(aabb.max_y, 1.0, 1.0e-9, "polygon aabb max y");
    require_true(!core_collision2d_shape_validate(NULL), "null shape rejected");
}

static void test_compound_shape_scaffold(void) {
    const CoreCollision2DVec2 top_band[4] = {
        {-0.6, 0.3},
        {0.6, 0.3},
        {0.6, 0.5},
        {-0.6, 0.5},
    };
    CoreCollision2DShape parts[3];
    CoreCollision2DCompoundShape compound = core_collision2d_compound_shape_zero();
    CoreCollision2DCompoundMassProperties properties = {0};
    CoreCollision2DAabb aabb = {0};
    CoreCollision2DVec2 center_of_mass = {0};
    double area = 0.0;
    double mass = 0.0;
    double inertia = 0.0;

    parts[0] = core_collision2d_shape_box_offset(0.6, 0.2, core_collision2d_vec2(0.0, -0.3));
    parts[1] = core_collision2d_shape_box_offset(0.4, 0.2, core_collision2d_vec2(-0.2, 0.1));
    parts[2] = core_collision2d_shape_convex_polygon(top_band, 4);

    require_true(!core_collision2d_compound_shape_validate(&compound), "zero compound rejects");
    require_true(core_collision2d_compound_shape_init(&compound, parts, 3), "compound init succeeds");
    require_true(core_collision2d_compound_shape_validate(&compound), "compound validates");
    require_true(compound.part_count == 3, "compound part count");
    require_true(core_collision2d_compound_shape_aabb(&compound, &aabb), "compound aabb succeeds");
    require_near(aabb.min_x, -0.6, 1.0e-9, "compound aabb min x");
    require_near(aabb.min_y, -0.5, 1.0e-9, "compound aabb min y");
    require_near(aabb.max_x, 0.6, 1.0e-9, "compound aabb max x");
    require_near(aabb.max_y, 0.5, 1.0e-9, "compound aabb max y");

    require_true(core_collision2d_compound_shape_area(&compound, &area), "compound area succeeds");
    require_near(area, 1.04, 1.0e-9, "compound area");
    require_true(core_collision2d_compound_shape_mass(&compound, 1.0, &mass), "compound mass succeeds");
    require_near(mass, 1.04, 1.0e-9, "compound mass density one");
    require_true(
        core_collision2d_compound_shape_center_of_mass(&compound, &center_of_mass),
        "compound center of mass succeeds");
    require_vec_near(
        center_of_mass,
        -0.061538461538461535,
        -0.015384615384615384,
        1.0e-9,
        "compound center of mass");
    require_true(core_collision2d_compound_shape_inertia(&compound, 1.0, &inertia), "compound inertia succeeds");
    require_near(inertia, 0.20834871794871793, 1.0e-9, "compound inertia density one");
    require_true(
        core_collision2d_compound_shape_mass_properties(&compound, 1.0, &properties),
        "compound mass properties succeeds");
    require_near(properties.density, 1.0, 1.0e-9, "compound properties density");
    require_near(properties.total_area, 1.04, 1.0e-9, "compound properties area");
    require_near(properties.total_mass, 1.04, 1.0e-9, "compound properties mass");
    require_vec_near(
        properties.center_of_mass,
        -0.061538461538461535,
        -0.015384615384615384,
        1.0e-9,
        "compound properties center");
    require_near(properties.inertia, 0.20834871794871793, 1.0e-9, "compound properties inertia");
    require_near(properties.inverse_mass, 1.0 / 1.04, 1.0e-9, "compound properties inverse mass");
    require_near(
        properties.inverse_inertia,
        1.0 / 0.20834871794871793,
        1.0e-9,
        "compound properties inverse inertia");
    require_near(properties.local_aabb.min_x, -0.6, 1.0e-9, "compound properties aabb min x");
    require_near(properties.local_aabb.max_y, 0.5, 1.0e-9, "compound properties aabb max y");
    require_true(core_collision2d_compound_shape_mass(&compound, 2.0, &mass), "compound mass density two");
    require_near(mass, 2.08, 1.0e-9, "compound mass scales with density");
    require_true(core_collision2d_compound_shape_inertia(&compound, 2.0, &inertia), "compound inertia density two");
    require_near(inertia, 0.41669743589743586, 1.0e-9, "compound inertia scales with density");

    require_true(!core_collision2d_compound_shape_init(NULL, parts, 3), "null compound output rejects");
    require_true(!core_collision2d_compound_shape_init(&compound, NULL, 3), "null compound parts reject");
    require_true(!core_collision2d_compound_shape_init(&compound, parts, 0), "zero compound parts reject");
    require_true(!core_collision2d_compound_shape_area(NULL, &area), "null compound area input rejects");
    require_true(!core_collision2d_compound_shape_area(&compound, NULL), "null compound area output rejects");
    require_true(!core_collision2d_compound_shape_mass(&compound, 0.0, &mass), "zero compound density rejects");
    require_true(
        !core_collision2d_compound_shape_mass(&compound, NAN, &mass),
        "non-finite compound density rejects");
    require_true(
        !core_collision2d_compound_shape_center_of_mass(&compound, NULL),
        "null compound center output rejects");
    require_true(!core_collision2d_compound_shape_inertia(&compound, -1.0, &inertia), "negative density rejects");
    require_true(
        !core_collision2d_compound_shape_mass_properties(&compound, 1.0, NULL),
        "null compound properties output rejects");

    parts[1] = core_collision2d_shape_circle(0.0);
    require_true(!core_collision2d_compound_shape_init(&compound, parts, 3), "invalid compound part rejects");
    require_true(!core_collision2d_compound_shape_aabb(NULL, &aabb), "null compound aabb input rejects");
    require_true(!core_collision2d_compound_shape_aabb(&compound, NULL), "null compound aabb output rejects");
}

static void test_manifold(void) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
    const CoreCollision2DVec2 points[1] = {
        {1.0, 2.0},
    };
    require_true(manifold.shape_a_kind == CORE_COLLISION2D_SHAPE_INVALID, "zero manifold shape a");
    require_true(
        core_collision2d_manifold_init(
            &manifold,
            1,
            2,
            CORE_COLLISION2D_SHAPE_CIRCLE,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, 1.0),
            0.25,
            points,
            1),
        "manifold init succeeds");
    require_true(core_collision2d_manifold_validate(&manifold), "manifold validates");
    require_true(manifold.contact_point_count == 1, "manifold point count");
    require_vec_near(manifold.contact_points[0], 1.0, 2.0, 1.0e-9, "manifold point");
}

static void test_circle_contact(void) {
    CoreCollision2DManifold manifold = {0};
    const int count = core_collision2d_contact_circle_circle(
        10,
        11,
        core_collision2d_vec2(0.0, 0.0),
        1.0,
        core_collision2d_vec2(1.5, 0.0),
        1.0,
        &manifold);
    require_true(count == 1, "circle contact count");
    require_true(core_collision2d_manifold_validate(&manifold), "circle manifold validates");
    require_vec_near(manifold.normal_from_a_to_b, 1.0, 0.0, 1.0e-9, "circle contact normal");
    require_near(manifold.penetration_depth, 0.5, 1.0e-9, "circle contact depth");
    require_vec_near(manifold.contact_points[0], 0.75, 0.0, 1.0e-9, "circle contact point");
    require_true(
        core_collision2d_contact_circle_circle(
            10,
            11,
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_collision2d_vec2(3.0, 0.0),
            1.0,
            &manifold) == 0,
        "circle miss returns zero");
}

static void test_box_contact(void) {
    CoreCollision2DManifold manifold = {0};
    const int count = core_collision2d_contact_box_box_axis_aligned(
        20,
        21,
        core_collision2d_vec2(0.0, 0.0),
        1.0,
        1.0,
        core_collision2d_vec2(1.5, 0.0),
        1.0,
        1.0,
        &manifold);
    require_true(count == 1, "box contact count");
    require_true(core_collision2d_manifold_validate(&manifold), "box manifold validates");
    require_vec_near(manifold.normal_from_a_to_b, 1.0, 0.0, 1.0e-9, "box contact normal");
    require_near(manifold.penetration_depth, 0.5, 1.0e-9, "box contact depth");
    require_vec_near(manifold.contact_points[0], 0.75, -1.0, 1.0e-9, "box contact point 0");
    require_vec_near(manifold.contact_points[1], 0.75, 1.0, 1.0e-9, "box contact point 1");
}

static void test_polygon_contact(void) {
    const CoreCollision2DVec2 square[4] = {
        {-1.0, -1.0},
        {1.0, -1.0},
        {1.0, 1.0},
        {-1.0, 1.0},
    };
    CoreCollision2DManifold manifold = {0};
    const int count = core_collision2d_contact_convex_polygon_polygon(
        30,
        31,
        square,
        4,
        core_collision2d_vec2(0.0, 0.0),
        0.0,
        square,
        4,
        core_collision2d_vec2(1.5, 0.0),
        0.0,
        &manifold);
    require_true(count == 1, "polygon contact count");
    require_true(core_collision2d_manifold_validate(&manifold), "polygon manifold validates");
    require_vec_near(manifold.normal_from_a_to_b, 1.0, -0.0, 1.0e-9, "polygon contact normal");
    require_near(manifold.penetration_depth, 0.5, 1.0e-9, "polygon contact depth");
    require_true(manifold.contact_point_count == 2, "polygon contact point count");
    require_true(
        core_collision2d_contact_convex_polygon_polygon(
            30,
            31,
            square,
            4,
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            square,
            4,
            core_collision2d_vec2(3.0, 0.0),
            0.0,
            &manifold) == 0,
        "polygon miss returns zero");
}

int main(void) {
    test_vectors_and_geometry();
    test_shapes_and_aabb();
    test_compound_shape_scaffold();
    test_manifold();
    test_circle_contact();
    test_box_contact();
    test_polygon_contact();
    puts("core_collision2d tests passed");
    return 0;
}
