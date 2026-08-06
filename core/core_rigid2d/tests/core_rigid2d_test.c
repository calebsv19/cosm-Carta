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

static void check_inertia_and_bodies(void) {
    const CoreCollision2DVec2 fixture[] = {
        {0.0, -0.5},
        {1.0, -0.25},
        {0.75, 0.55},
        {-0.55, 0.35},
        {-0.8, -0.25},
    };
    CoreRigid2DBody body = {0};
    CoreRigid2DBody static_body = {0};
    const CoreCollision2DShape polygon = core_collision2d_shape_convex_polygon(fixture, 5);

    require_near(core_rigid2d_body_compute_circle_inertia(2.0, 0.5), 0.25, 1.0e-7, "circle inertia");
    require_near(core_rigid2d_body_compute_box_inertia(3.0, 2.0, 1.0), 5.0, 1.0e-7, "box inertia");
    require_near(
        core_rigid2d_body_compute_polygon_inertia(2.0, fixture, 5),
        0.5061511000394953,
        1.0e-7,
        "polygon inertia");
    require_near(
        core_rigid2d_body_compute_shape_inertia(2.0, &polygon),
        0.5061511000394953,
        1.0e-7,
        "shape polygon inertia");
    require_near(core_rigid2d_body_compute_circle_inertia(0.0, 0.5), 0.0, 1.0e-7, "zero mass rejects");
    require_near(core_rigid2d_body_compute_polygon_inertia(2.0, NULL, 5), 0.0, 1.0e-7, "null polygon rejects");

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
    require_true(core_rigid2d_body_validate(&body), "dynamic polygon body validates");
    require_near(body.inertia_kg_m2, 0.5061511000394953, 1.0e-7, "body inertia");
    require_near(body.inverse_inertia, 1.9756946096175023, 1.0e-7, "body inverse inertia");

    body.angular_velocity_radps = 1.5;
    require_true(core_rigid2d_body_integrate(&body, 0.5), "dynamic integrates");
    require_vec_near(body.position_m, 3.25, 3.875, 1.0e-7, "integrated position");
    require_near(body.angle_rad, 0.75, 1.0e-7, "integrated angle");
    body.lock_rotation = true;
    body.inverse_inertia = 0.0;
    require_true(core_rigid2d_body_integrate(&body, 0.5), "locked body integrates position");
    require_vec_near(body.position_m, 3.5, 3.75, 1.0e-7, "locked integrated position");
    require_near(body.angle_rad, 0.75, 1.0e-7, "locked angle unchanged");

    require_true(
        core_rigid2d_body_init_static(
            &static_body,
            11,
            core_collision2d_shape_box(2.0, 0.5),
            core_collision2d_vec2(-3.0, 6.0),
            1.5,
            core_rigid2d_material_default()),
        "static body initializes");
    require_true(core_rigid2d_body_validate(&static_body), "static body validates");
    require_true(static_body.is_static, "static flag");
    require_true(static_body.lock_rotation, "static locks rotation");
    require_true(!core_rigid2d_body_set_mass(&static_body, 3.0), "static mass update rejects");
    require_true(core_rigid2d_body_integrate(&static_body, 1.0), "static integrate no-op");
    require_vec_near(static_body.position_m, -3.0, 6.0, 1.0e-7, "static position unchanged");
    require_near(static_body.angle_rad, 1.5, 1.0e-7, "static angle unchanged");

    require_true(
        !core_rigid2d_body_init_dynamic(
            &body,
            3,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(NAN, 0.0),
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_rigid2d_material_default()),
        "nan position rejects");
    require_true(
        !core_rigid2d_body_init_dynamic(
            &body,
            4,
            core_collision2d_shape_circle(0.5),
            core_collision2d_vec2(0.0, 0.0),
            core_collision2d_vec2(0.0, 0.0),
            1.0,
            core_rigid2d_material(-0.1, 0.5)),
        "negative restitution rejects");
}

static void check_normal_solver(void) {
    CoreRigid2DBody circle = {0};
    CoreRigid2DBody floor = {0};
    CoreRigid2DSolverResult result = {0};
    CoreRigid2DSolverConfig config = core_rigid2d_solver_config_default();
    config.restitution = 0.5;

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

    const CoreCollision2DManifold contact = make_contact(
        1,
        2,
        CORE_COLLISION2D_SHAPE_CIRCLE,
        CORE_COLLISION2D_SHAPE_BOX,
        core_collision2d_vec2(0.0, -1.0),
        0.25,
        core_collision2d_vec2(0.0, -0.5));

    require_true(core_rigid2d_solver_apply_contact(&circle, &floor, &contact, config, &result), "normal solve");
    require_near(circle.velocity_mps.y, 1.0, 1.0e-7, "dynamic static bounce velocity");
    require_near(circle.position_m.y, 0.2, 1.0e-7, "dynamic static correction");
    require_near(result.normal_speed_before_mps, -2.0, 1.0e-7, "normal speed");
    require_near(result.normal_impulse_kg_mps, 6.0, 1.0e-7, "normal impulse");
    require_true(result.impulse_applied, "normal impulse applied");
    require_true(result.positional_correction_applied, "normal correction applied");
}

static void check_angular_solver(void) {
    CoreRigid2DBody box = {0};
    CoreRigid2DBody floor = {0};
    CoreRigid2DAngularSolverResult result = {0};
    CoreRigid2DSolverConfig config = core_rigid2d_solver_config_default();
    config.restitution = 0.88;

    const CoreCollision2DVec2 contact_points[2] = {
        core_collision2d_vec2(5.55, 0.0),
        core_collision2d_vec2(6.45, 0.0),
    };
    CoreCollision2DManifold contact = core_collision2d_manifold_zero();
    require_true(
        core_collision2d_manifold_init(
            &contact,
            500,
            1002,
            CORE_COLLISION2D_SHAPE_BOX,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, -1.0),
            0.05,
            contact_points,
            2),
        "angular contact initializes");

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
}

static void check_friction_solver(void) {
    CoreRigid2DBody box = {0};
    CoreRigid2DBody floor = {0};
    CoreRigid2DFrictionSolverResult result = {0};
    CoreRigid2DSolverConfig config = core_rigid2d_solver_config_default();
    config.restitution = 0.88;
    config.enable_friction = true;
    config.friction_coefficient = 0.10;

    const CoreCollision2DVec2 contact_points[2] = {
        core_collision2d_vec2(5.55, 0.0),
        core_collision2d_vec2(6.45, 0.0),
    };
    CoreCollision2DManifold contact = core_collision2d_manifold_zero();
    require_true(
        core_collision2d_manifold_init(
            &contact,
            500,
            1002,
            CORE_COLLISION2D_SHAPE_BOX,
            CORE_COLLISION2D_SHAPE_BOX,
            core_collision2d_vec2(0.0, -1.0),
            0.05,
            contact_points,
            2),
        "friction contact initializes");

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
}

static void check_invalid_solver_inputs(void) {
    CoreRigid2DBody body_a = {0};
    CoreRigid2DBody body_b = {0};
    CoreRigid2DSolverResult result = {0};
    CoreRigid2DSolverConfig config = core_rigid2d_solver_config_default();

    require_true(
        core_rigid2d_body_init_static(
            &body_a,
            11,
            core_collision2d_shape_box(1.0, 1.0),
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            core_rigid2d_material_default()),
        "static a initializes");
    require_true(
        core_rigid2d_body_init_static(
            &body_b,
            12,
            core_collision2d_shape_box(1.0, 1.0),
            core_collision2d_vec2(0.0, 0.0),
            0.0,
            core_rigid2d_material_default()),
        "static b initializes");

    const CoreCollision2DManifold contact = make_contact(
        11,
        12,
        CORE_COLLISION2D_SHAPE_BOX,
        CORE_COLLISION2D_SHAPE_BOX,
        core_collision2d_vec2(1.0, 0.0),
        0.1,
        core_collision2d_vec2(0.0, 0.0));

    require_true(!core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, config, &result), "static static rejects");
    config.positional_correction_percent = 1.5;
    require_true(!core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, config, &result), "invalid config rejects");
    require_true(!core_rigid2d_solver_apply_contact(&body_a, &body_b, &contact, core_rigid2d_solver_config_default(), NULL), "null result rejects");
}

int main(void) {
    check_inertia_and_bodies();
    check_normal_solver();
    check_angular_solver();
    check_friction_solver();
    check_invalid_solver_inputs();

    puts("core_rigid2d tests passed");
    return 0;
}
