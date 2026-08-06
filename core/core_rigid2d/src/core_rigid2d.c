#include "core_rigid2d.h"

#include <math.h>
#include <stddef.h>

static bool core_rigid2d_is_finite_vec2(CoreCollision2DVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool core_rigid2d_material_validate(CoreRigid2DMaterial material) {
    return isfinite(material.restitution) &&
           material.restitution >= 0.0 &&
           isfinite(material.friction) &&
           material.friction >= 0.0;
}

static bool core_rigid2d_solver_config_validate(CoreRigid2DSolverConfig config) {
    return isfinite(config.restitution) &&
           config.restitution >= 0.0 &&
           isfinite(config.positional_correction_percent) &&
           config.positional_correction_percent >= 0.0 &&
           config.positional_correction_percent <= 1.0 &&
           isfinite(config.positional_slop_m) &&
           config.positional_slop_m >= 0.0 &&
           isfinite(config.friction_coefficient) &&
           config.friction_coefficient >= 0.0;
}

static bool core_rigid2d_contact_matches_bodies(
    const CoreRigid2DBody* body_a,
    const CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold) {
    return body_a != NULL &&
           body_b != NULL &&
           manifold != NULL &&
           body_a->id == manifold->body_a_id &&
           body_b->id == manifold->body_b_id &&
           body_a->shape.kind == manifold->shape_a_kind &&
           body_b->shape.kind == manifold->shape_b_kind;
}

static double core_rigid2d_clamp(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static CoreCollision2DVec2 core_rigid2d_cross_scalar_vec2(double scalar, CoreCollision2DVec2 value) {
    return core_collision2d_vec2(-scalar * value.y, scalar * value.x);
}

CoreRigid2DMaterial core_rigid2d_material_default(void) {
    return core_rigid2d_material(0.8, 0.5);
}

CoreRigid2DMaterial core_rigid2d_material(double restitution, double friction) {
    CoreRigid2DMaterial material = {restitution, friction};
    return material;
}

double core_rigid2d_body_compute_circle_inertia(double mass_kg, double radius_m) {
    if (!isfinite(mass_kg) || mass_kg <= 0.0 || !isfinite(radius_m) || radius_m <= 0.0) {
        return 0.0;
    }
    return 0.5 * mass_kg * radius_m * radius_m;
}

double core_rigid2d_body_compute_box_inertia(double mass_kg, double half_width_m, double half_height_m) {
    if (!isfinite(mass_kg) || mass_kg <= 0.0 ||
        !isfinite(half_width_m) || half_width_m <= 0.0 ||
        !isfinite(half_height_m) || half_height_m <= 0.0) {
        return 0.0;
    }
    return (mass_kg * (half_width_m * half_width_m + half_height_m * half_height_m)) / 3.0;
}

double core_rigid2d_body_compute_polygon_inertia(
    double mass_kg,
    const CoreCollision2DVec2* vertices,
    int vertex_count) {
    if (!isfinite(mass_kg) || mass_kg <= 0.0 ||
        vertices == NULL ||
        vertex_count < 3 ||
        vertex_count > CORE_COLLISION2D_MAX_POLYGON_VERTICES) {
        return 0.0;
    }

    double signed_cross_sum = 0.0;
    double centroid_x_sum = 0.0;
    double centroid_y_sum = 0.0;
    double inertia_sum = 0.0;
    for (int i = 0; i < vertex_count; ++i) {
        const CoreCollision2DVec2 a = vertices[i];
        const CoreCollision2DVec2 b = vertices[(i + 1) % vertex_count];
        if (!core_rigid2d_is_finite_vec2(a) || !core_rigid2d_is_finite_vec2(b)) {
            return 0.0;
        }

        const double cross = a.x * b.y - b.x * a.y;
        const double term = a.x * a.x + a.y * a.y + a.x * b.x + a.y * b.y + b.x * b.x + b.y * b.y;
        if (!isfinite(cross) || !isfinite(term)) {
            return 0.0;
        }

        signed_cross_sum += cross;
        centroid_x_sum += (a.x + b.x) * cross;
        centroid_y_sum += (a.y + b.y) * cross;
        inertia_sum += cross * term;
    }

    const double signed_area = 0.5 * signed_cross_sum;
    if (!isfinite(signed_area) || fabs(signed_area) <= 1.0e-12 || !isfinite(inertia_sum)) {
        return 0.0;
    }

    const double centroid_x = centroid_x_sum / (3.0 * signed_cross_sum);
    const double centroid_y = centroid_y_sum / (3.0 * signed_cross_sum);
    const double inertia_about_origin = mass_kg * fabs(inertia_sum) / (12.0 * fabs(signed_area));
    const double centroid_shift = mass_kg * (centroid_x * centroid_x + centroid_y * centroid_y);
    const double inertia_about_centroid = inertia_about_origin - centroid_shift;
    if (!isfinite(inertia_about_centroid) || inertia_about_centroid <= 0.0) {
        return 0.0;
    }
    return inertia_about_centroid;
}

double core_rigid2d_body_compute_shape_inertia(double mass_kg, const CoreCollision2DShape* shape) {
    if (!core_collision2d_shape_validate(shape)) {
        return 0.0;
    }

    switch (shape->kind) {
        case CORE_COLLISION2D_SHAPE_CIRCLE:
            return core_rigid2d_body_compute_circle_inertia(mass_kg, shape->data.circle.radius);
        case CORE_COLLISION2D_SHAPE_BOX:
            return core_rigid2d_body_compute_box_inertia(
                mass_kg,
                shape->data.box.half_width,
                shape->data.box.half_height);
        case CORE_COLLISION2D_SHAPE_CONVEX_POLYGON:
            return core_rigid2d_body_compute_polygon_inertia(
                mass_kg,
                shape->data.polygon.vertices,
                shape->data.polygon.vertex_count);
        case CORE_COLLISION2D_SHAPE_INVALID:
        default:
            return 0.0;
    }
}

bool core_rigid2d_body_init_dynamic(
    CoreRigid2DBody* out_body,
    int id,
    CoreCollision2DShape shape,
    CoreCollision2DVec2 position_m,
    CoreCollision2DVec2 velocity_mps,
    double mass_kg,
    CoreRigid2DMaterial material) {
    return core_rigid2d_body_init_dynamic_shape(out_body, id, &shape, position_m, velocity_mps, mass_kg, material);
}

bool core_rigid2d_body_init_dynamic_shape(
    CoreRigid2DBody* out_body,
    int id,
    const CoreCollision2DShape* shape,
    CoreCollision2DVec2 position_m,
    CoreCollision2DVec2 velocity_mps,
    double mass_kg,
    CoreRigid2DMaterial material) {
    if (out_body == NULL ||
        !core_collision2d_shape_validate(shape) ||
        !core_rigid2d_is_finite_vec2(position_m) ||
        !core_rigid2d_is_finite_vec2(velocity_mps) ||
        !core_rigid2d_material_validate(material)) {
        return false;
    }

    const double inertia = core_rigid2d_body_compute_shape_inertia(mass_kg, shape);
    if (!isfinite(mass_kg) || mass_kg <= 0.0 || !isfinite(inertia) || inertia <= 0.0) {
        return false;
    }

    CoreRigid2DBody body = {0};
    body.id = id;
    body.shape = *shape;
    body.position_m = position_m;
    body.velocity_mps = velocity_mps;
    body.angle_rad = 0.0;
    body.angular_velocity_radps = 0.0;
    body.mass_kg = mass_kg;
    body.inverse_mass = 1.0 / mass_kg;
    body.inertia_kg_m2 = inertia;
    body.inverse_inertia = 1.0 / inertia;
    body.material = material;
    body.is_static = false;
    body.lock_rotation = false;

    *out_body = body;
    return true;
}

bool core_rigid2d_body_init_static(
    CoreRigid2DBody* out_body,
    int id,
    CoreCollision2DShape shape,
    CoreCollision2DVec2 position_m,
    double angle_rad,
    CoreRigid2DMaterial material) {
    if (out_body == NULL ||
        !core_collision2d_shape_validate(&shape) ||
        !core_rigid2d_is_finite_vec2(position_m) ||
        !isfinite(angle_rad) ||
        !core_rigid2d_material_validate(material)) {
        return false;
    }

    CoreRigid2DBody body = {0};
    body.id = id;
    body.shape = shape;
    body.position_m = position_m;
    body.velocity_mps = core_collision2d_vec2(0.0, 0.0);
    body.angle_rad = angle_rad;
    body.angular_velocity_radps = 0.0;
    body.mass_kg = 0.0;
    body.inverse_mass = 0.0;
    body.inertia_kg_m2 = 0.0;
    body.inverse_inertia = 0.0;
    body.material = material;
    body.is_static = true;
    body.lock_rotation = true;

    *out_body = body;
    return true;
}

bool core_rigid2d_body_set_mass(CoreRigid2DBody* body, double mass_kg) {
    if (body == NULL || body->is_static || !core_collision2d_shape_validate(&body->shape)) {
        return false;
    }

    const double inertia = core_rigid2d_body_compute_shape_inertia(mass_kg, &body->shape);
    if (!isfinite(mass_kg) || mass_kg <= 0.0 || !isfinite(inertia) || inertia <= 0.0) {
        return false;
    }

    body->mass_kg = mass_kg;
    body->inverse_mass = 1.0 / mass_kg;
    body->inertia_kg_m2 = inertia;
    body->inverse_inertia = body->lock_rotation ? 0.0 : 1.0 / inertia;
    return true;
}

bool core_rigid2d_body_validate(const CoreRigid2DBody* body) {
    if (body == NULL ||
        !core_collision2d_shape_validate(&body->shape) ||
        !core_rigid2d_is_finite_vec2(body->position_m) ||
        !core_rigid2d_is_finite_vec2(body->velocity_mps) ||
        !isfinite(body->angle_rad) ||
        !isfinite(body->angular_velocity_radps) ||
        !core_rigid2d_material_validate(body->material)) {
        return false;
    }

    if (body->is_static) {
        return body->mass_kg == 0.0 &&
               body->inverse_mass == 0.0 &&
               body->inertia_kg_m2 == 0.0 &&
               body->inverse_inertia == 0.0;
    }

    if (!isfinite(body->mass_kg) || body->mass_kg <= 0.0 ||
        !isfinite(body->inverse_mass) || body->inverse_mass <= 0.0 ||
        !isfinite(body->inertia_kg_m2) || body->inertia_kg_m2 <= 0.0 ||
        !isfinite(body->inverse_inertia) || body->inverse_inertia < 0.0) {
        return false;
    }

    if (body->lock_rotation && body->inverse_inertia != 0.0) {
        return false;
    }
    if (!body->lock_rotation && body->inverse_inertia <= 0.0) {
        return false;
    }

    return true;
}

bool core_rigid2d_body_integrate(CoreRigid2DBody* body, double dt_seconds) {
    if (!core_rigid2d_body_validate(body) || !isfinite(dt_seconds) || dt_seconds < 0.0) {
        return false;
    }
    if (body->is_static || dt_seconds == 0.0) {
        return true;
    }

    body->position_m = core_collision2d_vec2_add(
        body->position_m,
        core_collision2d_vec2_scale(body->velocity_mps, dt_seconds));
    if (!body->lock_rotation) {
        body->angle_rad += body->angular_velocity_radps * dt_seconds;
    }
    return true;
}

CoreRigid2DSolverConfig core_rigid2d_solver_config_default(void) {
    CoreRigid2DSolverConfig config = {0};
    config.restitution = 0.8;
    config.positional_correction_percent = 0.8;
    config.positional_slop_m = 0.0;
    config.enable_friction = false;
    config.friction_coefficient = 0.0;
    return config;
}

bool core_rigid2d_solver_apply_contact(
    CoreRigid2DBody* body_a,
    CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold,
    CoreRigid2DSolverConfig config,
    CoreRigid2DSolverResult* out_result) {
    if (out_result == NULL) {
        return false;
    }

    CoreRigid2DSolverResult result = {0};
    *out_result = result;

    if (!core_rigid2d_body_validate(body_a) ||
        !core_rigid2d_body_validate(body_b) ||
        !core_collision2d_manifold_validate(manifold) ||
        !core_rigid2d_solver_config_validate(config) ||
        !core_rigid2d_contact_matches_bodies(body_a, body_b, manifold)) {
        return false;
    }

    const double inverse_mass_a = body_a->inverse_mass;
    const double inverse_mass_b = body_b->inverse_mass;
    const double inverse_mass_sum = inverse_mass_a + inverse_mass_b;
    if (!isfinite(inverse_mass_sum) || inverse_mass_sum <= 0.0) {
        return false;
    }

    const CoreCollision2DVec2 normal = manifold->normal_from_a_to_b;
    const CoreCollision2DVec2 relative_velocity = core_collision2d_vec2_sub(
        body_b->velocity_mps,
        body_a->velocity_mps);
    const double normal_speed = core_collision2d_vec2_dot(relative_velocity, normal);

    result.normal_speed_before_mps = normal_speed;

    if (normal_speed < 0.0) {
        const double impulse_magnitude = -((1.0 + config.restitution) * normal_speed) / inverse_mass_sum;
        const CoreCollision2DVec2 impulse = core_collision2d_vec2_scale(normal, impulse_magnitude);

        body_a->velocity_mps = core_collision2d_vec2_sub(
            body_a->velocity_mps,
            core_collision2d_vec2_scale(impulse, inverse_mass_a));
        body_b->velocity_mps = core_collision2d_vec2_add(
            body_b->velocity_mps,
            core_collision2d_vec2_scale(impulse, inverse_mass_b));

        result.impulse_applied = true;
        result.normal_impulse_kg_mps = impulse_magnitude;
    }

    const double correction_depth = manifold->penetration_depth > config.positional_slop_m
        ? manifold->penetration_depth - config.positional_slop_m
        : 0.0;
    if (correction_depth > 0.0 && config.positional_correction_percent > 0.0) {
        const double correction_magnitude = (correction_depth * config.positional_correction_percent) / inverse_mass_sum;
        const CoreCollision2DVec2 correction_a = core_collision2d_vec2_scale(
            normal,
            -correction_magnitude * inverse_mass_a);
        const CoreCollision2DVec2 correction_b = core_collision2d_vec2_scale(
            normal,
            correction_magnitude * inverse_mass_b);

        body_a->position_m = core_collision2d_vec2_add(body_a->position_m, correction_a);
        body_b->position_m = core_collision2d_vec2_add(body_b->position_m, correction_b);

        result.positional_correction_applied = true;
        result.correction_depth_m = correction_depth;
        result.position_correction_a_m = correction_a;
        result.position_correction_b_m = correction_b;
    }

    result.friction_applied = false;
    *out_result = result;
    return true;
}

bool core_rigid2d_solver_apply_contact_angular(
    CoreRigid2DBody* body_a,
    CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold,
    CoreRigid2DSolverConfig config,
    double dt_seconds,
    CoreRigid2DAngularSolverResult* out_result) {
    if (out_result == NULL) {
        return false;
    }

    CoreRigid2DAngularSolverResult result = {0};
    *out_result = result;

    if (!core_rigid2d_body_validate(body_a) ||
        !core_rigid2d_body_validate(body_b) ||
        !core_collision2d_manifold_validate(manifold) ||
        !core_rigid2d_solver_config_validate(config) ||
        !core_rigid2d_contact_matches_bodies(body_a, body_b, manifold) ||
        !isfinite(dt_seconds) ||
        dt_seconds < 0.0) {
        return false;
    }

    const double inverse_mass_a = body_a->inverse_mass;
    const double inverse_mass_b = body_b->inverse_mass;
    const double inverse_inertia_a = body_a->lock_rotation ? 0.0 : body_a->inverse_inertia;
    const double inverse_inertia_b = body_b->lock_rotation ? 0.0 : body_b->inverse_inertia;
    const CoreCollision2DVec2 normal = manifold->normal_from_a_to_b;
    const CoreCollision2DVec2 contact_point = manifold->contact_points[0];
    const CoreCollision2DVec2 offset_a = core_collision2d_vec2_sub(contact_point, body_a->position_m);
    const CoreCollision2DVec2 offset_b = core_collision2d_vec2_sub(contact_point, body_b->position_m);
    const double offset_a_cross_normal = core_collision2d_vec2_cross(offset_a, normal);
    const double offset_b_cross_normal = core_collision2d_vec2_cross(offset_b, normal);
    const double angular_denominator =
        inverse_mass_a +
        inverse_mass_b +
        offset_a_cross_normal * offset_a_cross_normal * inverse_inertia_a +
        offset_b_cross_normal * offset_b_cross_normal * inverse_inertia_b;
    if (!isfinite(angular_denominator) || angular_denominator <= 0.0) {
        return false;
    }

    const CoreCollision2DVec2 velocity_at_a = core_collision2d_vec2_add(
        body_a->velocity_mps,
        core_rigid2d_cross_scalar_vec2(body_a->angular_velocity_radps, offset_a));
    const CoreCollision2DVec2 velocity_at_b = core_collision2d_vec2_add(
        body_b->velocity_mps,
        core_rigid2d_cross_scalar_vec2(body_b->angular_velocity_radps, offset_b));
    const CoreCollision2DVec2 relative_velocity = core_collision2d_vec2_sub(velocity_at_b, velocity_at_a);
    const double normal_speed = core_collision2d_vec2_dot(relative_velocity, normal);

    result.normal_speed_before_mps = normal_speed;
    result.angular_denominator = angular_denominator;
    result.contact_offset_a_cross_normal = offset_a_cross_normal;
    result.contact_offset_b_cross_normal = offset_b_cross_normal;
    result.contact_point_m = contact_point;

    if (normal_speed < 0.0) {
        const double impulse_magnitude = -((1.0 + config.restitution) * normal_speed) / angular_denominator;
        const CoreCollision2DVec2 impulse = core_collision2d_vec2_scale(normal, impulse_magnitude);
        const double angular_impulse_a = -core_collision2d_vec2_cross(offset_a, impulse) * inverse_inertia_a;
        const double angular_impulse_b = core_collision2d_vec2_cross(offset_b, impulse) * inverse_inertia_b;

        body_a->velocity_mps = core_collision2d_vec2_sub(
            body_a->velocity_mps,
            core_collision2d_vec2_scale(impulse, inverse_mass_a));
        body_b->velocity_mps = core_collision2d_vec2_add(
            body_b->velocity_mps,
            core_collision2d_vec2_scale(impulse, inverse_mass_b));
        body_a->angular_velocity_radps += angular_impulse_a;
        body_b->angular_velocity_radps += angular_impulse_b;

        result.impulse_applied = true;
        result.normal_impulse_kg_mps = impulse_magnitude;
        result.angular_impulse_a_radps = angular_impulse_a;
        result.angular_impulse_b_radps = angular_impulse_b;
    }

    const double correction_depth = manifold->penetration_depth > config.positional_slop_m
        ? manifold->penetration_depth - config.positional_slop_m
        : 0.0;
    if (correction_depth > 0.0 && config.positional_correction_percent > 0.0) {
        const double inverse_mass_sum = inverse_mass_a + inverse_mass_b;
        if (!isfinite(inverse_mass_sum) || inverse_mass_sum <= 0.0) {
            return false;
        }
        const double correction_magnitude = (correction_depth * config.positional_correction_percent) / inverse_mass_sum;
        const CoreCollision2DVec2 correction_a = core_collision2d_vec2_scale(
            normal,
            -correction_magnitude * inverse_mass_a);
        const CoreCollision2DVec2 correction_b = core_collision2d_vec2_scale(
            normal,
            correction_magnitude * inverse_mass_b);

        body_a->position_m = core_collision2d_vec2_add(body_a->position_m, correction_a);
        body_b->position_m = core_collision2d_vec2_add(body_b->position_m, correction_b);

        result.positional_correction_applied = true;
        result.correction_depth_m = correction_depth;
        result.position_correction_a_m = correction_a;
        result.position_correction_b_m = correction_b;
    }

    if (dt_seconds > 0.0) {
        const double angle_delta_a = body_a->lock_rotation ? 0.0 : body_a->angular_velocity_radps * dt_seconds;
        const double angle_delta_b = body_b->lock_rotation ? 0.0 : body_b->angular_velocity_radps * dt_seconds;
        body_a->angle_rad += angle_delta_a;
        body_b->angle_rad += angle_delta_b;
        result.angle_delta_a_rad = angle_delta_a;
        result.angle_delta_b_rad = angle_delta_b;
        result.angle_integrated = angle_delta_a != 0.0 || angle_delta_b != 0.0;
    }

    *out_result = result;
    return true;
}

bool core_rigid2d_solver_apply_contact_friction(
    CoreRigid2DBody* body_a,
    CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold,
    CoreRigid2DSolverConfig config,
    double dt_seconds,
    CoreRigid2DFrictionSolverResult* out_result) {
    if (out_result == NULL) {
        return false;
    }

    CoreRigid2DFrictionSolverResult result = {0};
    *out_result = result;

    if (!core_rigid2d_body_validate(body_a) ||
        !core_rigid2d_body_validate(body_b) ||
        !core_collision2d_manifold_validate(manifold) ||
        !core_rigid2d_solver_config_validate(config) ||
        !core_rigid2d_contact_matches_bodies(body_a, body_b, manifold) ||
        !isfinite(dt_seconds) ||
        dt_seconds < 0.0) {
        return false;
    }

    const double inverse_mass_a = body_a->inverse_mass;
    const double inverse_mass_b = body_b->inverse_mass;
    const double inverse_inertia_a = body_a->lock_rotation ? 0.0 : body_a->inverse_inertia;
    const double inverse_inertia_b = body_b->lock_rotation ? 0.0 : body_b->inverse_inertia;
    const CoreCollision2DVec2 normal = manifold->normal_from_a_to_b;
    const CoreCollision2DVec2 contact_point = manifold->contact_points[0];
    const CoreCollision2DVec2 offset_a = core_collision2d_vec2_sub(contact_point, body_a->position_m);
    const CoreCollision2DVec2 offset_b = core_collision2d_vec2_sub(contact_point, body_b->position_m);
    const double offset_a_cross_normal = core_collision2d_vec2_cross(offset_a, normal);
    const double offset_b_cross_normal = core_collision2d_vec2_cross(offset_b, normal);
    const double angular_denominator =
        inverse_mass_a +
        inverse_mass_b +
        offset_a_cross_normal * offset_a_cross_normal * inverse_inertia_a +
        offset_b_cross_normal * offset_b_cross_normal * inverse_inertia_b;
    if (!isfinite(angular_denominator) || angular_denominator <= 0.0) {
        return false;
    }

    const CoreCollision2DVec2 velocity_at_a = core_collision2d_vec2_add(
        body_a->velocity_mps,
        core_rigid2d_cross_scalar_vec2(body_a->angular_velocity_radps, offset_a));
    const CoreCollision2DVec2 velocity_at_b = core_collision2d_vec2_add(
        body_b->velocity_mps,
        core_rigid2d_cross_scalar_vec2(body_b->angular_velocity_radps, offset_b));
    const CoreCollision2DVec2 relative_velocity = core_collision2d_vec2_sub(velocity_at_b, velocity_at_a);
    const double normal_speed = core_collision2d_vec2_dot(relative_velocity, normal);

    result.normal.normal_speed_before_mps = normal_speed;
    result.normal.angular_denominator = angular_denominator;
    result.normal.contact_offset_a_cross_normal = offset_a_cross_normal;
    result.normal.contact_offset_b_cross_normal = offset_b_cross_normal;
    result.normal.contact_point_m = contact_point;

    if (normal_speed < 0.0) {
        const double impulse_magnitude = -((1.0 + config.restitution) * normal_speed) / angular_denominator;
        const CoreCollision2DVec2 impulse = core_collision2d_vec2_scale(normal, impulse_magnitude);
        const double angular_impulse_a = -core_collision2d_vec2_cross(offset_a, impulse) * inverse_inertia_a;
        const double angular_impulse_b = core_collision2d_vec2_cross(offset_b, impulse) * inverse_inertia_b;

        body_a->velocity_mps = core_collision2d_vec2_sub(
            body_a->velocity_mps,
            core_collision2d_vec2_scale(impulse, inverse_mass_a));
        body_b->velocity_mps = core_collision2d_vec2_add(
            body_b->velocity_mps,
            core_collision2d_vec2_scale(impulse, inverse_mass_b));
        body_a->angular_velocity_radps += angular_impulse_a;
        body_b->angular_velocity_radps += angular_impulse_b;

        result.normal.impulse_applied = true;
        result.normal.normal_impulse_kg_mps = impulse_magnitude;
        result.normal.angular_impulse_a_radps = angular_impulse_a;
        result.normal.angular_impulse_b_radps = angular_impulse_b;
    }

    if (config.enable_friction && result.normal.normal_impulse_kg_mps > 0.0) {
        const CoreCollision2DVec2 friction_velocity_at_a = core_collision2d_vec2_add(
            body_a->velocity_mps,
            core_rigid2d_cross_scalar_vec2(body_a->angular_velocity_radps, offset_a));
        const CoreCollision2DVec2 friction_velocity_at_b = core_collision2d_vec2_add(
            body_b->velocity_mps,
            core_rigid2d_cross_scalar_vec2(body_b->angular_velocity_radps, offset_b));
        const CoreCollision2DVec2 friction_relative_velocity = core_collision2d_vec2_sub(
            friction_velocity_at_b,
            friction_velocity_at_a);
        const double friction_normal_speed = core_collision2d_vec2_dot(friction_relative_velocity, normal);
        const CoreCollision2DVec2 tangent_candidate = core_collision2d_vec2_sub(
            friction_relative_velocity,
            core_collision2d_vec2_scale(normal, friction_normal_speed));
        const double tangent_length_sq = core_collision2d_vec2_dot(tangent_candidate, tangent_candidate);
        if (isfinite(tangent_length_sq) && tangent_length_sq > 0.000000000001) {
            const double tangent_length = sqrt(tangent_length_sq);
            const CoreCollision2DVec2 tangent = core_collision2d_vec2_scale(tangent_candidate, 1.0 / tangent_length);
            const double offset_a_cross_tangent = core_collision2d_vec2_cross(offset_a, tangent);
            const double offset_b_cross_tangent = core_collision2d_vec2_cross(offset_b, tangent);
            const double tangent_denominator =
                inverse_mass_a +
                inverse_mass_b +
                offset_a_cross_tangent * offset_a_cross_tangent * inverse_inertia_a +
                offset_b_cross_tangent * offset_b_cross_tangent * inverse_inertia_b;
            if (!isfinite(tangent_denominator) || tangent_denominator <= 0.0) {
                return false;
            }

            const double tangent_speed = core_collision2d_vec2_dot(friction_relative_velocity, tangent);
            const double requested_tangent_impulse = -tangent_speed / tangent_denominator;
            const double max_tangent_impulse = config.friction_coefficient * result.normal.normal_impulse_kg_mps;
            const double tangent_impulse = core_rigid2d_clamp(
                requested_tangent_impulse,
                -max_tangent_impulse,
                max_tangent_impulse);
            const CoreCollision2DVec2 impulse = core_collision2d_vec2_scale(tangent, tangent_impulse);
            const double angular_impulse_a = -core_collision2d_vec2_cross(offset_a, impulse) * inverse_inertia_a;
            const double angular_impulse_b = core_collision2d_vec2_cross(offset_b, impulse) * inverse_inertia_b;

            body_a->velocity_mps = core_collision2d_vec2_sub(
                body_a->velocity_mps,
                core_collision2d_vec2_scale(impulse, inverse_mass_a));
            body_b->velocity_mps = core_collision2d_vec2_add(
                body_b->velocity_mps,
                core_collision2d_vec2_scale(impulse, inverse_mass_b));
            body_a->angular_velocity_radps += angular_impulse_a;
            body_b->angular_velocity_radps += angular_impulse_b;

            result.friction_applied = true;
            result.friction_clamped = tangent_impulse != requested_tangent_impulse;
            result.tangent_speed_before_mps = tangent_speed;
            result.tangent_impulse_kg_mps = tangent_impulse;
            result.max_tangent_impulse_kg_mps = max_tangent_impulse;
            result.tangent_denominator = tangent_denominator;
            result.contact_offset_a_cross_tangent = offset_a_cross_tangent;
            result.contact_offset_b_cross_tangent = offset_b_cross_tangent;
            result.angular_friction_a_radps = angular_impulse_a;
            result.angular_friction_b_radps = angular_impulse_b;
            result.tangent = tangent;
        }
    }

    const double correction_depth = manifold->penetration_depth > config.positional_slop_m
        ? manifold->penetration_depth - config.positional_slop_m
        : 0.0;
    if (correction_depth > 0.0 && config.positional_correction_percent > 0.0) {
        const double inverse_mass_sum = inverse_mass_a + inverse_mass_b;
        if (!isfinite(inverse_mass_sum) || inverse_mass_sum <= 0.0) {
            return false;
        }
        const double correction_magnitude = (correction_depth * config.positional_correction_percent) / inverse_mass_sum;
        const CoreCollision2DVec2 correction_a = core_collision2d_vec2_scale(
            normal,
            -correction_magnitude * inverse_mass_a);
        const CoreCollision2DVec2 correction_b = core_collision2d_vec2_scale(
            normal,
            correction_magnitude * inverse_mass_b);

        body_a->position_m = core_collision2d_vec2_add(body_a->position_m, correction_a);
        body_b->position_m = core_collision2d_vec2_add(body_b->position_m, correction_b);

        result.normal.positional_correction_applied = true;
        result.normal.correction_depth_m = correction_depth;
        result.normal.position_correction_a_m = correction_a;
        result.normal.position_correction_b_m = correction_b;
    }

    if (dt_seconds > 0.0) {
        const double angle_delta_a = body_a->lock_rotation ? 0.0 : body_a->angular_velocity_radps * dt_seconds;
        const double angle_delta_b = body_b->lock_rotation ? 0.0 : body_b->angular_velocity_radps * dt_seconds;
        body_a->angle_rad += angle_delta_a;
        body_b->angle_rad += angle_delta_b;
        result.normal.angle_delta_a_rad = angle_delta_a;
        result.normal.angle_delta_b_rad = angle_delta_b;
        result.normal.angle_integrated = angle_delta_a != 0.0 || angle_delta_b != 0.0;
    }

    *out_result = result;
    return true;
}
