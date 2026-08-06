#include "core_rigid2d.h"

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
    require_near(actual.x, x, epsilon, message);
    require_near(actual.y, y, epsilon, message);
}

static CoreRigid2DSolverConfig make_normal_config(double restitution) {
    CoreRigid2DSolverConfig config = core_rigid2d_solver_config_default();
    config.restitution = restitution;
    config.positional_correction_percent = 0.8;
    config.positional_slop_m = 0.0;
    config.enable_friction = false;
    config.friction_coefficient = 0.0;
    return config;
}

static CoreCollision2DManifold make_contact(
    int body_a_id,
    int body_b_id,
    CoreCollision2DShapeKind shape_a,
    CoreCollision2DShapeKind shape_b,
    CoreCollision2DVec2 normal,
    double depth_m,
    CoreCollision2DVec2 point_m) {
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
    require_true(
        core_collision2d_manifold_init(
            &manifold,
            body_a_id,
            body_b_id,
            shape_a,
            shape_b,
            normal,
            depth_m,
            &point_m,
            1),
        "contact initializes");
    return manifold;
}

static CoreCollision2DManifold make_floor_contact(void) {
    const CoreCollision2DVec2 contact_points[2] = {
        core_collision2d_vec2(5.55, 0.0),
        core_collision2d_vec2(6.45, 0.0),
    };
    CoreCollision2DManifold manifold = core_collision2d_manifold_zero();
    require_true(
        core_collision2d_manifold_init(
            &manifold,
            500,
            1002,
            CORE_COLLISION2D_SHAPE_BOX,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, -1.0),
            0.05,
            contact_points,
            2),
        "floor contact initializes");
    return manifold;
}

static void check_body_contract_parity(void) {
    CoreRigid2DBody body = {0};
    CoreRigid2DBody static_body = {0};
    const CoreCollision2DShape circle = core_collision2d_shape_circle(0.5);
    const CoreCollision2DShape box = core_collision2d_shape_box(2.0, 0.5);
    const CoreRigid2DMaterial defaults = core_rigid2d_material_default();

    require_near(defaults.restitution, 0.8, 1.0e-7, "default restitution");
    require_near(defaults.friction, 0.5, 1.0e-7, "default friction");
    require_near(core_rigid2d_body_compute_circle_inertia(2.0, 0.5), 0.25, 1.0e-7, "circle inertia");
    require_near(core_rigid2d_body_compute_circle_inertia(2.0, -1.0), 0.0, 1.0e-7, "negative radius rejects");
    require_near(core_rigid2d_body_compute_box_inertia(3.0, 2.0, INFINITY), 0.0, 1.0e-7, "infinite box rejects");

    require_true(
        core_rigid2d_body_init_dynamic(
            &body,
            7,
            circle,
            core_collision2d_vec2(1.25, -2.5),
            core_collision2d_vec2(3.0, 4.5),
            2.0,
            core_rigid2d_material(0.6, 0.4)),
        "dynamic circle initializes");
    require_true(core_rigid2d_body_validate(&body), "dynamic circle validates");
    require_true(body.id == 7, "dynamic id stable");
    require_true(body.shape.kind == CORE_COLLISION2D_SHAPE_CIRCLE, "dynamic shape kind stable");
    require_true(!body.is_static, "dynamic static flag");
    require_true(!body.lock_rotation, "dynamic lock rotation flag");
    require_vec_near(body.position_m, 1.25, -2.5, 1.0e-7, "dynamic position");
    require_vec_near(body.velocity_mps, 3.0, 4.5, 1.0e-7, "dynamic velocity");
    require_near(body.mass_kg, 2.0, 1.0e-7, "dynamic mass");
    require_near(body.inverse_mass, 0.5, 1.0e-7, "dynamic inverse mass");
    require_near(body.inertia_kg_m2, 0.25, 1.0e-7, "dynamic inertia");
    require_near(body.inverse_inertia, 4.0, 1.0e-7, "dynamic inverse inertia");
    require_near(body.material.restitution, 0.6, 1.0e-7, "dynamic restitution");
    require_near(body.material.friction, 0.4, 1.0e-7, "dynamic friction");

    require_true(core_rigid2d_body_set_mass(&body, 4.0), "dynamic mass update");
    require_near(body.inverse_mass, 0.25, 1.0e-7, "updated inverse mass");
    require_near(body.inertia_kg_m2, 0.5, 1.0e-7, "updated inertia");
    require_near(body.inverse_inertia, 2.0, 1.0e-7, "updated inverse inertia");

    require_true(
        core_rigid2d_body_init_static(
            &static_body,
            11,
            box,
            core_collision2d_vec2(-3.0, 6.0),
            1.5,
            defaults),
        "static box initializes");
    require_true(core_rigid2d_body_validate(&static_body), "static box validates");
    require_true(static_body.is_static, "static flag");
    require_true(static_body.lock_rotation, "static lock rotation");
    require_near(static_body.mass_kg, 0.0, 1.0e-7, "static mass");
    require_near(static_body.inverse_mass, 0.0, 1.0e-7, "static inverse mass");
    require_near(static_body.inertia_kg_m2, 0.0, 1.0e-7, "static inertia");
    require_near(static_body.inverse_inertia, 0.0, 1.0e-7, "static inverse inertia");
    require_true(!core_rigid2d_body_set_mass(&static_body, 3.0), "static mass update rejects");

    require_true(
        !core_rigid2d_body_init_dynamic(
            &body,
            1,
            circle,
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(0.0, 0.0),
            -1.0,
            defaults),
        "negative mass rejects");
    require_true(
        !core_rigid2d_body_init_dynamic(
            &body,
            2,
            circle,
            core_collision2d_vec2(NAN, 0.0),
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            defaults),
        "nan position rejects");
}

static void check_polygon_inertia_contract_parity(void) {
    const CoreCollision2DVec2 fixture[] = {
        {0.0, -0.5},
        {1.0, -0.25},
        {0.75, 0.55},
        {-0.55, 0.35},
        {-0.8, -0.25},
    };
    const CoreCollision2DVec2 concave[] = {
        {-1.0, -1.0},
        {1.0, -1.0},
        {0.0, 0.0},
        {1.0, 1.0},
        {-1.0, 1.0},
    };
    CoreRigid2DBody body = {0};
    const CoreCollision2DShape polygon = core_collision2d_shape_convex_polygon(fixture, 5);
    const CoreCollision2DShape concave_shape = core_collision2d_shape_convex_polygon(concave, 5);

    require_true(core_collision2d_shape_validate(&polygon), "polygon shape validates");
    require_near(
        core_rigid2d_body_compute_polygon_inertia(2.0, fixture, 5),
        0.5061511000394953,
        1.0e-7,
        "centroidal polygon inertia");
    require_near(
        core_rigid2d_body_compute_shape_inertia(2.0, &polygon),
        0.5061511000394953,
        1.0e-7,
        "shape polygon inertia");
    require_true(
        core_rigid2d_body_init_dynamic(
            &body,
            42,
            polygon,
            core_collision2d_vec2(3.0, 4.0),
            core_collision2d_vec2(0.5, -0.25),
            2.0,
            core_rigid2d_material(0.6, 0.4)),
        "dynamic polygon body initializes");
    require_near(body.inertia_kg_m2, 0.5061511000394953, 1.0e-7, "body polygon inertia");
    require_near(body.inverse_inertia, 1.9756946096175023, 1.0e-7, "body polygon inverse inertia");

    require_true(!core_collision2d_shape_validate(&concave_shape), "concave shape rejects");
    require_true(
        !core_rigid2d_body_init_dynamic(
            &body,
            43,
            concave_shape,
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_rigid2d_material_default()),
        "concave body rejects");
    require_near(core_rigid2d_body_compute_polygon_inertia(2.0, NULL, 5), 0.0, 1.0e-7, "null vertices reject");
    require_near(core_rigid2d_body_compute_polygon_inertia(2.0, fixture, 2), 0.0, 1.0e-7, "short polygon rejects");
    require_near(core_rigid2d_body_compute_polygon_inertia(0.0, fixture, 5), 0.0, 1.0e-7, "zero mass rejects");
}

static void check_normal_solver_contract_parity(void) {
    CoreRigid2DBody circle = {0};
    CoreRigid2DBody floor = {0};
    CoreRigid2DBody body_a = {0};
    CoreRigid2DBody body_b = {0};
    CoreRigid2DSolverResult result = {0};

    require_true(
        core_rigid2d_body_init_dynamic(
            &circle,
            1,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(0.0, -2.0),
            2.0,
            core_rigid2d_material_default()),
        "dynamic circle initializes");
    require_true(
        core_rigid2d_body_init_static(
            &floor,
            2,
            core_collision2d_shape_box(4.0, 0.1),
            core_collision2d_vec2(0.0, -0.5),
            0.0,
            core_rigid2d_material_default()),
        "static floor initializes");
    CoreCollision2DManifold contact = make_contact(
        1,
        2,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_BOX,
        core_collision2d_vec2(0.0, -1.0),
        0.25,
        core_collision2d_vec2(0.0, -0.5));
    require_true(core_rigid2d_solver_apply_contact(&circle, &floor, &contact, make_normal_config(0.5), &result), "dynamic static solve");
    require_near(circle.velocity_mps.y, 1.0, 1.0e-7, "dynamic static bounce velocity");
    require_near(circle.position_m.y, 0.2, 1.0e-7, "dynamic static correction");
    require_near(floor.position_m.y, -0.5, 1.0e-7, "static floor unchanged");
    require_near(result.normal_speed_before_mps, -2.0, 1.0e-7, "dynamic static normal speed");
    require_near(result.normal_impulse_kg_mps, 6.0, 1.0e-7, "dynamic static impulse");
    require_near(result.correction_depth_m, 0.25, 1.0e-7, "dynamic static correction depth");
    require_vec_near(result.position_correction_a_m, 0.0, 0.2, 1.0e-7, "dynamic static correction a");
    require_vec_near(result.position_correction_b_m, 0.0, 0.0, 1.0e-7, "dynamic static correction b");
    require_true(result.impulse_applied, "dynamic static impulse applied");
    require_true(result.positional_correction_applied, "dynamic static position applied");
    require_true(!result.friction_applied, "dynamic static friction not applied");

    require_true(
        core_rigid2d_body_init_dynamic(
            &body_a,
            3,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            core_rigid2d_material_default()),
        "body a initializes");
    require_true(
        core_rigid2d_body_init_dynamic(
            &body_b,
            4,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(1.5, 0.0),
            core_collision2d_vec2(-1.0, 0.0),
            1.0,
            core_rigid2d_material_default()),
        "body b initializes");
    contact = make_contact(
        3,
        4,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        core_collision2d_vec2(1.0, 0.0),
        0.5,
        core_collision2d_vec2(0.75, 0.0));
    require_true(core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, make_normal_config(1.0), &result), "dynamic dynamic solve");
    require_near(body_a.velocity_mps.x, -1.0, 1.0e-7, "body a reflected velocity");
    require_near(body_b.velocity_mps.x, 1.0, 1.0e-7, "body b reflected velocity");
    require_near(body_a.position_m.x, -0.2, 1.0e-7, "body a correction");
    require_near(body_b.position_m.x, 1.7, 1.0e-7, "body b correction");
    require_near(result.normal_impulse_kg_mps, 2.0, 1.0e-7, "dynamic dynamic impulse");
    require_vec_near(result.position_correction_a_m, -0.2, 0.0, 1.0e-7, "dynamic dynamic correction a");
    require_vec_near(result.position_correction_b_m, 0.2, 0.0, 1.0e-7, "dynamic dynamic correction b");

    require_true(
        core_rigid2d_body_init_dynamic(
            &body_a,
            5,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(-1.0, 0.0),
            1.0,
            core_rigid2d_material_default()),
        "separating body a initializes");
    require_true(
        core_rigid2d_body_init_dynamic(
            &body_b,
            6,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.8, 0.0),
            core_collision2d_vec2(1.0, 0.0),
            1.0,
            core_rigid2d_material_default()),
        "separating body b initializes");
    contact = make_contact(
        5,
        6,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        core_collision2d_vec2(1.0, 0.0),
        0.2,
        core_collision2d_vec2(0.4, 0.0));
    require_true(core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, make_normal_config(0.8), &result), "separating overlap solve");
    require_near(body_a.velocity_mps.x, -1.0, 1.0e-7, "separating body a velocity unchanged");
    require_near(body_b.velocity_mps.x, 1.0, 1.0e-7, "separating body b velocity unchanged");
    require_near(body_a.position_m.x, -0.08, 1.0e-7, "separating body a correction");
    require_near(body_b.position_m.x, 0.88, 1.0e-7, "separating body b correction");
    require_true(!result.impulse_applied, "separating no impulse");
    require_true(result.positional_correction_applied, "separating position applied");
}

static void check_restitution_contract_parity(void) {
    CoreRigid2DBody inelastic_circle = {0};
    CoreRigid2DBody elastic_circle = {0};
    CoreRigid2DBody floor_a = {0};
    CoreRigid2DBody floor_b = {0};
    CoreRigid2DSolverResult result = {0};
    CoreCollision2DManifold inelastic_contact = {0};
    CoreCollision2DManifold elastic_contact = {0};

    require_true(
        core_rigid2d_body_init_dynamic(
            &inelastic_circle,
            7,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(0.0, -2.0),
            2.0,
            core_rigid2d_material_default()),
        "inelastic circle initializes");
    require_true(
        core_rigid2d_body_init_dynamic(
            &elastic_circle,
            8,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(0.0, -2.0),
            2.0,
            core_rigid2d_material_default()),
        "elastic circle initializes");
    require_true(
        core_rigid2d_body_init_static(
            &floor_a,
            9,
            core_collision2d_shape_box(4.0, 0.1),
            core_collision2d_vec2(0.0, -0.5),
            0.0,
            core_rigid2d_material_default()),
        "floor a initializes");
    require_true(
        core_rigid2d_body_init_static(
            &floor_b,
            10,
            core_collision2d_shape_box(4.0, 0.1),
            core_collision2d_vec2(0.0, -0.5),
            0.0,
            core_rigid2d_material_default()),
        "floor b initializes");

    inelastic_contact = make_contact(
        7,
        9,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_BOX,
        core_collision2d_vec2(0.0, -1.0),
        0.0,
        core_collision2d_vec2(0.0, -0.5));
    elastic_contact = make_contact(
        8,
        10,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_BOX,
        core_collision2d_vec2(0.0, -1.0),
        0.0,
        core_collision2d_vec2(0.0, -0.5));

    require_true(core_rigid2d_solver_apply_contact(&inelastic_circle, &floor_a, &inelastic_contact, make_normal_config(0.0), &result), "inelastic solve");
    require_near(inelastic_circle.velocity_mps.y, 0.0, 1.0e-7, "inelastic velocity");
    require_true(core_rigid2d_solver_apply_contact(&elastic_circle, &floor_b, &elastic_contact, make_normal_config(1.0), &result), "elastic solve");
    require_near(elastic_circle.velocity_mps.y, 2.0, 1.0e-7, "elastic velocity");
}

static void check_invalid_solver_contract_parity(void) {
    CoreRigid2DBody body_a = {0};
    CoreRigid2DBody body_b = {0};
    CoreRigid2DSolverResult result = {0};
    CoreRigid2DSolverConfig invalid_config = make_normal_config(0.8);

    require_true(
        core_rigid2d_body_init_static(
            &body_a,
            11,
            core_collision2d_shape_box(1.0, 1.0),
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            core_rigid2d_material_default()),
        "static body a initializes");
    require_true(
        core_rigid2d_body_init_static(
            &body_b,
            12,
            core_collision2d_shape_box(1.0, 1.0),
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            core_rigid2d_material_default()),
        "static body b initializes");

    const CoreCollision2DManifold contact = make_contact(
        11,
        12,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        core_collision2d_vec2(1.0, 0.0),
        0.1,
        core_collision2d_vec2(0.0, 0.0));

    require_true(!core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, make_normal_config(0.8), &result), "static static rejects");
    invalid_config.positional_correction_percent = 1.5;
    require_true(!core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, invalid_config, &result), "invalid config rejects");
    require_true(!core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, make_normal_config(0.8), NULL), "null result rejects");
}

static void check_angular_contract_parity(void) {
    CoreRigid2DBody box = {0};
    CoreRigid2DBody floor = {0};
    CoreRigid2DAngularSolverResult result = {0};
    CoreRigid2DSolverConfig config = make_normal_config(0.88);
    const CoreCollision2DManifold contact = make_floor_contact();

    require_true(
        core_rigid2d_body_init_dynamic(
            &box,
            500,
            core_collision2d_shape_box(0.45, 0.25),
            core_collision2d_vec2(6.0, 0.20),
            core_collision2d_vec2(0.75, -1.25),
            0.80,
            core_rigid2d_material(0.88, 0.10)),
        "box initializes");
    require_true(
        core_rigid2d_body_init_static(
            &floor,
            1002,
            core_collision2d_shape_box(6.0, 0.01),
            core_collision2d_vec2(6.0, 0.0),
            0.0,
            core_rigid2d_material(0.88, 0.04)),
        "floor initializes");

    require_true(
        core_rigid2d_solver_apply_contact_angular(&box, &floor, &contact, config, 1.0 / 60.0, &result),
        "angular solve applies");
    require_near(box.velocity_mps.y, -0.53624642, 1.0e-7, "box y velocity");
    require_near(box.position_m.y, 0.24, 1.0e-7, "box y correction");
    require_near(box.angular_velocity_radps, -3.63610315, 1.0e-7, "box angular velocity");
    require_near(box.angle_rad, -0.06060172, 1.0e-7, "box angle integrated");
    require_true(result.impulse_applied, "angular impulse applied");
    require_true(result.positional_correction_applied, "angular correction applied");
    require_true(result.angle_integrated, "angle integrated");
    require_vec_near(result.contact_point_m, 5.55, 0.0, 1.0e-7, "selected contact point");
    require_near(result.normal_speed_before_mps, -1.25, 1.0e-7, "angular normal speed");
    require_near(result.normal_impulse_kg_mps, 0.571002865330, 1.0e-7, "angular impulse");
    require_near(result.angular_denominator, 4.115566037736, 1.0e-7, "angular denominator");
    require_near(result.contact_offset_a_cross_normal, 0.45, 1.0e-7, "r a cross normal");
    require_near(result.contact_offset_b_cross_normal, 0.45, 1.0e-7, "r b cross normal");
    require_near(result.angular_impulse_a_radps, -3.63610315, 1.0e-7, "angular impulse a");
    require_near(result.angular_impulse_b_radps, 0.0, 1.0e-7, "angular impulse b");
    require_near(result.angle_delta_a_rad, -0.06060172, 1.0e-7, "angle delta a");
    require_near(result.angle_delta_b_rad, 0.0, 1.0e-7, "angle delta b");
    require_near(result.correction_depth_m, 0.05, 1.0e-7, "angular correction depth");
    require_vec_near(result.position_correction_a_m, 0.0, 0.04, 1.0e-7, "angular correction a");
    require_vec_near(result.position_correction_b_m, 0.0, 0.0, 1.0e-7, "angular correction b");
}

static void check_friction_contract_parity(void) {
    CoreRigid2DBody box = {0};
    CoreRigid2DBody floor = {0};
    CoreRigid2DFrictionSolverResult result = {0};
    CoreRigid2DSolverConfig config = make_normal_config(0.88);
    config.enable_friction = true;
    config.friction_coefficient = 0.10;
    const CoreCollision2DManifold contact = make_floor_contact();

    require_true(
        core_rigid2d_body_init_dynamic(
            &box,
            500,
            core_collision2d_shape_box(0.45, 0.25),
            core_collision2d_vec2(6.0, 0.20),
            core_collision2d_vec2(0.75, -1.25),
            0.80,
            core_rigid2d_material(0.88, 0.10)),
        "friction box initializes");
    require_true(
        core_rigid2d_body_init_static(
            &floor,
            1002,
            core_collision2d_shape_box(6.0, 0.01),
            core_collision2d_vec2(6.0, 0.0),
            0.0,
            core_rigid2d_material(0.88, 0.04)),
        "friction floor initializes");

    require_true(
        core_rigid2d_solver_apply_contact_friction(&box, &floor, &contact, config, 1.0 / 60.0, &result),
        "friction solve applies");
    require_true(result.normal.impulse_applied, "friction normal impulse applied");
    require_true(result.friction_applied, "friction impulse applied");
    require_true(!result.friction_clamped, "friction impulse not clamped");
    require_true(result.normal.angle_integrated, "friction angle integrated");
    require_vec_near(result.tangent, -1.0, 0.0, 1.0e-7, "friction tangent");
    require_near(result.tangent_speed_before_mps, 0.022779369628, 1.0e-7, "friction tangent speed");
    require_near(result.tangent_impulse_kg_mps, -0.012543445094, 1.0e-7, "friction tangent impulse");
    require_near(result.max_tangent_impulse_kg_mps, 0.057100286533, 1.0e-7, "friction max tangent impulse");
    require_near(result.tangent_denominator, 1.816037735849, 1.0e-7, "friction tangent denominator");
    require_near(result.contact_offset_a_cross_tangent, -0.2, 1.0e-7, "friction rta");
    require_near(result.contact_offset_b_cross_tangent, 0.0, 1.0e-7, "friction rtb");
    require_near(result.angular_friction_a_radps, -0.035500316303, 1.0e-7, "friction angular a");
    require_near(result.angular_friction_b_radps, 0.0, 1.0e-7, "friction angular b");
    require_near(box.velocity_mps.x, 0.73432069, 1.0e-7, "friction post x velocity");
    require_near(box.velocity_mps.y, -0.53624642, 1.0e-7, "friction post y velocity");
    require_near(box.position_m.y, 0.24, 1.0e-7, "friction post y correction");
    require_near(box.angle_rad, -0.061193391136, 1.0e-7, "friction post angle");
    require_near(box.angular_velocity_radps, -3.671603468165, 1.0e-7, "friction post angular velocity");
}

int main(void) {
    check_body_contract_parity();
    check_polygon_inertia_contract_parity();
    check_normal_solver_contract_parity();
    check_restitution_contract_parity();
    check_invalid_solver_contract_parity();
    check_angular_contract_parity();
    check_friction_contract_parity();

    puts("core_rigid2d parity tests passed");
    return 0;
}
