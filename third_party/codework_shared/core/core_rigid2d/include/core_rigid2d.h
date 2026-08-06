#ifndef CORE_RIGID2D_H
#define CORE_RIGID2D_H

#include <stdbool.h>

#include "core_collision2d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreRigid2DMaterial {
    double restitution;
    double friction;
} CoreRigid2DMaterial;

typedef struct CoreRigid2DBody {
    int id;
    CoreCollision2DShape shape;
    CoreCollision2DVec2 position_m;
    CoreCollision2DVec2 velocity_mps;
    double angle_rad;
    double angular_velocity_radps;
    double mass_kg;
    double inverse_mass;
    double inertia_kg_m2;
    double inverse_inertia;
    CoreRigid2DMaterial material;
    bool is_static;
    bool lock_rotation;
} CoreRigid2DBody;

typedef struct CoreRigid2DSolverConfig {
    double restitution;
    double positional_correction_percent;
    double positional_slop_m;
    bool enable_friction;
    double friction_coefficient;
} CoreRigid2DSolverConfig;

typedef struct CoreRigid2DSolverResult {
    bool impulse_applied;
    bool positional_correction_applied;
    bool friction_applied;
    double normal_speed_before_mps;
    double normal_impulse_kg_mps;
    double correction_depth_m;
    CoreCollision2DVec2 position_correction_a_m;
    CoreCollision2DVec2 position_correction_b_m;
} CoreRigid2DSolverResult;

typedef struct CoreRigid2DAngularSolverResult {
    bool impulse_applied;
    bool positional_correction_applied;
    bool angle_integrated;
    double normal_speed_before_mps;
    double normal_impulse_kg_mps;
    double angular_denominator;
    double contact_offset_a_cross_normal;
    double contact_offset_b_cross_normal;
    double angular_impulse_a_radps;
    double angular_impulse_b_radps;
    double angle_delta_a_rad;
    double angle_delta_b_rad;
    double correction_depth_m;
    CoreCollision2DVec2 contact_point_m;
    CoreCollision2DVec2 position_correction_a_m;
    CoreCollision2DVec2 position_correction_b_m;
} CoreRigid2DAngularSolverResult;

typedef struct CoreRigid2DFrictionSolverResult {
    CoreRigid2DAngularSolverResult normal;
    bool friction_applied;
    bool friction_clamped;
    double tangent_speed_before_mps;
    double tangent_impulse_kg_mps;
    double max_tangent_impulse_kg_mps;
    double tangent_denominator;
    double contact_offset_a_cross_tangent;
    double contact_offset_b_cross_tangent;
    double angular_friction_a_radps;
    double angular_friction_b_radps;
    CoreCollision2DVec2 tangent;
} CoreRigid2DFrictionSolverResult;

CoreRigid2DMaterial core_rigid2d_material_default(void);
CoreRigid2DMaterial core_rigid2d_material(double restitution, double friction);

double core_rigid2d_body_compute_circle_inertia(double mass_kg, double radius_m);
double core_rigid2d_body_compute_box_inertia(double mass_kg, double half_width_m, double half_height_m);
double core_rigid2d_body_compute_polygon_inertia(
    double mass_kg,
    const CoreCollision2DVec2* vertices,
    int vertex_count);
double core_rigid2d_body_compute_shape_inertia(double mass_kg, const CoreCollision2DShape* shape);

bool core_rigid2d_body_init_dynamic(
    CoreRigid2DBody* out_body,
    int id,
    CoreCollision2DShape shape,
    CoreCollision2DVec2 position_m,
    CoreCollision2DVec2 velocity_mps,
    double mass_kg,
    CoreRigid2DMaterial material);
bool core_rigid2d_body_init_dynamic_shape(
    CoreRigid2DBody* out_body,
    int id,
    const CoreCollision2DShape* shape,
    CoreCollision2DVec2 position_m,
    CoreCollision2DVec2 velocity_mps,
    double mass_kg,
    CoreRigid2DMaterial material);
bool core_rigid2d_body_init_static(
    CoreRigid2DBody* out_body,
    int id,
    CoreCollision2DShape shape,
    CoreCollision2DVec2 position_m,
    double angle_rad,
    CoreRigid2DMaterial material);
bool core_rigid2d_body_set_mass(CoreRigid2DBody* body, double mass_kg);
bool core_rigid2d_body_validate(const CoreRigid2DBody* body);
bool core_rigid2d_body_integrate(CoreRigid2DBody* body, double dt_seconds);

CoreRigid2DSolverConfig core_rigid2d_solver_config_default(void);
bool core_rigid2d_solver_apply_contact(
    CoreRigid2DBody* body_a,
    CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold,
    CoreRigid2DSolverConfig config,
    CoreRigid2DSolverResult* out_result);
bool core_rigid2d_solver_apply_contact_angular(
    CoreRigid2DBody* body_a,
    CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold,
    CoreRigid2DSolverConfig config,
    double dt_seconds,
    CoreRigid2DAngularSolverResult* out_result);
bool core_rigid2d_solver_apply_contact_friction(
    CoreRigid2DBody* body_a,
    CoreRigid2DBody* body_b,
    const CoreCollision2DManifold* manifold,
    CoreRigid2DSolverConfig config,
    double dt_seconds,
    CoreRigid2DFrictionSolverResult* out_result);

#ifdef __cplusplus
}
#endif

#endif
