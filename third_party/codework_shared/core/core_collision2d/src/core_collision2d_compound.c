#include "core_collision2d.h"

#include <math.h>

typedef struct CoreCollision2DPartMassProperties {
    double area;
    double mass;
    CoreCollision2DVec2 center_of_mass;
    double inertia_about_center_of_mass;
} CoreCollision2DPartMassProperties;

static const double CORE_COLLISION2D_PI = 3.14159265358979323846;

static bool core_collision2d_compound_is_finite_vec2(CoreCollision2DVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static double core_collision2d_compound_polygon_centroid_inertia(
    double mass,
    const CoreCollision2DVec2* vertices,
    int vertex_count) {
    if (!isfinite(mass) ||
        mass <= 0.0 ||
        vertices == 0 ||
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
        if (!core_collision2d_compound_is_finite_vec2(a) ||
            !core_collision2d_compound_is_finite_vec2(b)) {
            return 0.0;
        }

        const double cross = core_collision2d_vec2_cross(a, b);
        const double term =
            a.x * a.x + a.y * a.y + a.x * b.x + a.y * b.y + b.x * b.x + b.y * b.y;
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
    const double inertia_about_origin = mass * fabs(inertia_sum) / (12.0 * fabs(signed_area));
    const double centroid_shift = mass * (centroid_x * centroid_x + centroid_y * centroid_y);
    const double inertia_about_centroid = inertia_about_origin - centroid_shift;
    if (!isfinite(inertia_about_centroid) || inertia_about_centroid <= 0.0) {
        return 0.0;
    }
    return inertia_about_centroid;
}

static bool core_collision2d_compound_part_mass_properties(
    const CoreCollision2DShape* shape,
    double density,
    CoreCollision2DPartMassProperties* out_properties) {
    if (out_properties == 0 ||
        !core_collision2d_shape_validate(shape) ||
        !isfinite(density) ||
        density <= 0.0) {
        return false;
    }

    CoreCollision2DPartMassProperties properties = {0};
    switch (shape->kind) {
        case CORE_COLLISION2D_SHAPE_CIRCLE: {
            const double radius = shape->data.circle.radius;
            properties.area = CORE_COLLISION2D_PI * radius * radius;
            properties.mass = properties.area * density;
            properties.center_of_mass = shape->offset;
            properties.inertia_about_center_of_mass = 0.5 * properties.mass * radius * radius;
            break;
        }

        case CORE_COLLISION2D_SHAPE_BOX: {
            const double half_width = shape->data.box.half_width;
            const double half_height = shape->data.box.half_height;
            properties.area = 4.0 * half_width * half_height;
            properties.mass = properties.area * density;
            properties.center_of_mass = shape->offset;
            properties.inertia_about_center_of_mass =
                (properties.mass * (half_width * half_width + half_height * half_height)) / 3.0;
            break;
        }

        case CORE_COLLISION2D_SHAPE_CONVEX_POLYGON: {
            CoreCollision2DVec2 local_centroid = {0};
            const int count = shape->data.polygon.vertex_count;
            if (!core_collision2d_polygon_centroid(
                    shape->data.polygon.vertices,
                    count,
                    &local_centroid)) {
                return false;
            }
            properties.area = core_collision2d_polygon_area(shape->data.polygon.vertices, count);
            properties.mass = properties.area * density;
            properties.center_of_mass = core_collision2d_vec2_add(shape->offset, local_centroid);
            properties.inertia_about_center_of_mass = core_collision2d_compound_polygon_centroid_inertia(
                properties.mass,
                shape->data.polygon.vertices,
                count);
            break;
        }

        case CORE_COLLISION2D_SHAPE_INVALID:
        default:
            return false;
    }

    if (!isfinite(properties.area) ||
        properties.area <= 0.0 ||
        !isfinite(properties.mass) ||
        properties.mass <= 0.0 ||
        !core_collision2d_compound_is_finite_vec2(properties.center_of_mass) ||
        !isfinite(properties.inertia_about_center_of_mass) ||
        properties.inertia_about_center_of_mass <= 0.0) {
        return false;
    }

    *out_properties = properties;
    return true;
}

CoreCollision2DCompoundShape core_collision2d_compound_shape_zero(void) {
    CoreCollision2DCompoundShape shape = {0};
    return shape;
}

bool core_collision2d_compound_shape_init(
    CoreCollision2DCompoundShape* out_shape,
    const CoreCollision2DShape* parts,
    int part_count) {
    if (out_shape == 0 ||
        parts == 0 ||
        part_count <= 0 ||
        part_count > CORE_COLLISION2D_MAX_COMPOUND_PARTS) {
        return false;
    }

    CoreCollision2DCompoundShape shape = core_collision2d_compound_shape_zero();
    shape.part_count = part_count;
    for (int i = 0; i < part_count; ++i) {
        if (!core_collision2d_shape_validate(&parts[i])) {
            return false;
        }
        shape.parts[i].shape = parts[i];
    }

    *out_shape = shape;
    return true;
}

bool core_collision2d_compound_shape_validate(const CoreCollision2DCompoundShape* shape) {
    if (shape == 0 ||
        shape->part_count <= 0 ||
        shape->part_count > CORE_COLLISION2D_MAX_COMPOUND_PARTS) {
        return false;
    }

    for (int i = 0; i < shape->part_count; ++i) {
        if (!core_collision2d_shape_validate(&shape->parts[i].shape)) {
            return false;
        }
    }
    return true;
}

bool core_collision2d_compound_shape_aabb(
    const CoreCollision2DCompoundShape* shape,
    CoreCollision2DAabb* out_aabb) {
    if (out_aabb == 0 || !core_collision2d_compound_shape_validate(shape)) {
        return false;
    }

    CoreCollision2DAabb aggregate = {0};
    for (int i = 0; i < shape->part_count; ++i) {
        CoreCollision2DAabb part_aabb = {0};
        if (!core_collision2d_shape_aabb(&shape->parts[i].shape, &part_aabb)) {
            return false;
        }

        if (i == 0) {
            aggregate = part_aabb;
        } else {
            if (part_aabb.min_x < aggregate.min_x) {
                aggregate.min_x = part_aabb.min_x;
            }
            if (part_aabb.min_y < aggregate.min_y) {
                aggregate.min_y = part_aabb.min_y;
            }
            if (part_aabb.max_x > aggregate.max_x) {
                aggregate.max_x = part_aabb.max_x;
            }
            if (part_aabb.max_y > aggregate.max_y) {
                aggregate.max_y = part_aabb.max_y;
            }
        }
    }

    if (!core_collision2d_aabb_validate(&aggregate)) {
        return false;
    }

    *out_aabb = aggregate;
    return true;
}

bool core_collision2d_compound_shape_area(
    const CoreCollision2DCompoundShape* shape,
    double* out_area) {
    if (out_area == 0 || !core_collision2d_compound_shape_validate(shape)) {
        return false;
    }

    double total_area = 0.0;
    for (int i = 0; i < shape->part_count; ++i) {
        CoreCollision2DPartMassProperties properties = {0};
        if (!core_collision2d_compound_part_mass_properties(
                &shape->parts[i].shape,
                1.0,
                &properties)) {
            return false;
        }
        total_area += properties.area;
    }

    if (!isfinite(total_area) || total_area <= 0.0) {
        return false;
    }
    *out_area = total_area;
    return true;
}

bool core_collision2d_compound_shape_mass(
    const CoreCollision2DCompoundShape* shape,
    double density,
    double* out_mass) {
    double area = 0.0;
    if (out_mass == 0 ||
        !isfinite(density) ||
        density <= 0.0 ||
        !core_collision2d_compound_shape_area(shape, &area)) {
        return false;
    }

    const double mass = area * density;
    if (!isfinite(mass) || mass <= 0.0) {
        return false;
    }
    *out_mass = mass;
    return true;
}

bool core_collision2d_compound_shape_center_of_mass(
    const CoreCollision2DCompoundShape* shape,
    CoreCollision2DVec2* out_center_of_mass) {
    if (out_center_of_mass == 0 || !core_collision2d_compound_shape_validate(shape)) {
        return false;
    }

    double total_area = 0.0;
    CoreCollision2DVec2 weighted_center = core_collision2d_vec2(0.0, 0.0);
    for (int i = 0; i < shape->part_count; ++i) {
        CoreCollision2DPartMassProperties properties = {0};
        if (!core_collision2d_compound_part_mass_properties(
                &shape->parts[i].shape,
                1.0,
                &properties)) {
            return false;
        }
        total_area += properties.area;
        weighted_center.x += properties.center_of_mass.x * properties.area;
        weighted_center.y += properties.center_of_mass.y * properties.area;
    }

    if (!isfinite(total_area) || total_area <= 0.0) {
        return false;
    }

    const CoreCollision2DVec2 center_of_mass =
        core_collision2d_vec2(weighted_center.x / total_area, weighted_center.y / total_area);
    if (!core_collision2d_compound_is_finite_vec2(center_of_mass)) {
        return false;
    }

    *out_center_of_mass = center_of_mass;
    return true;
}

bool core_collision2d_compound_shape_inertia(
    const CoreCollision2DCompoundShape* shape,
    double density,
    double* out_inertia) {
    CoreCollision2DCompoundMassProperties properties = {0};
    if (out_inertia == 0 ||
        !core_collision2d_compound_shape_mass_properties(shape, density, &properties)) {
        return false;
    }
    *out_inertia = properties.inertia;
    return true;
}

bool core_collision2d_compound_shape_mass_properties(
    const CoreCollision2DCompoundShape* shape,
    double density,
    CoreCollision2DCompoundMassProperties* out_properties) {
    if (out_properties == 0 ||
        !core_collision2d_compound_shape_validate(shape) ||
        !isfinite(density) ||
        density <= 0.0) {
        return false;
    }

    CoreCollision2DPartMassProperties part_properties[CORE_COLLISION2D_MAX_COMPOUND_PARTS] = {0};
    double total_area = 0.0;
    double total_mass = 0.0;
    CoreCollision2DVec2 weighted_center = core_collision2d_vec2(0.0, 0.0);
    for (int i = 0; i < shape->part_count; ++i) {
        if (!core_collision2d_compound_part_mass_properties(
                &shape->parts[i].shape,
                density,
                &part_properties[i])) {
            return false;
        }

        total_area += part_properties[i].area;
        total_mass += part_properties[i].mass;
        weighted_center.x += part_properties[i].center_of_mass.x * part_properties[i].mass;
        weighted_center.y += part_properties[i].center_of_mass.y * part_properties[i].mass;
    }

    if (!isfinite(total_area) ||
        total_area <= 0.0 ||
        !isfinite(total_mass) ||
        total_mass <= 0.0) {
        return false;
    }

    const CoreCollision2DVec2 center_of_mass =
        core_collision2d_vec2(weighted_center.x / total_mass, weighted_center.y / total_mass);
    if (!core_collision2d_compound_is_finite_vec2(center_of_mass)) {
        return false;
    }

    double inertia = 0.0;
    for (int i = 0; i < shape->part_count; ++i) {
        const CoreCollision2DVec2 delta =
            core_collision2d_vec2_sub(part_properties[i].center_of_mass, center_of_mass);
        inertia +=
            part_properties[i].inertia_about_center_of_mass +
            part_properties[i].mass * core_collision2d_vec2_dot(delta, delta);
    }
    if (!isfinite(inertia) || inertia <= 0.0) {
        return false;
    }

    CoreCollision2DAabb aabb = {0};
    if (!core_collision2d_compound_shape_aabb(shape, &aabb)) {
        return false;
    }

    CoreCollision2DCompoundMassProperties properties = {0};
    properties.density = density;
    properties.total_area = total_area;
    properties.total_mass = total_mass;
    properties.center_of_mass = center_of_mass;
    properties.inertia = inertia;
    properties.inverse_mass = 1.0 / total_mass;
    properties.inverse_inertia = 1.0 / inertia;
    properties.local_aabb = aabb;

    *out_properties = properties;
    return true;
}
