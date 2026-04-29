#include "core_scene_compile.h"

#include "core_base.h"

#include <stdio.h>
#include <string.h>

static int test_compile_success_and_preserve_extensions(void) {
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_test\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":2.5,"
        "\"objects\":[{\"object_id\":\"obj_a\"}],"
        "\"hierarchy\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{\"ray_tracing\":{\"samples\":32},\"custom\":{\"x\":1}}"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(authoring_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (!runtime_json) return 1;
    if (!strstr(runtime_json, "\"schema_variant\":\"scene_runtime_v1\"")) return 1;
    if (!strstr(runtime_json, "\"source_scene_id\":\"scene_test\"")) return 1;
    if (!strstr(runtime_json, "\"compile_meta\":")) return 1;
    if (!strstr(runtime_json, "\"normalization\":\"v0.3_sorted_lanes_primitive_contract\"")) return 1;
    if (!strstr(runtime_json, "\"hierarchy\":[]")) return 1;
    if (!strstr(runtime_json, "\"extensions\":{\"ray_tracing\":{\"samples\":32},\"custom\":{\"x\":1}}")) return 1;
    core_free(runtime_json);
    return 0;
}

static int test_reject_wrong_variant(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bad\","
        "\"objects\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "schema_variant")) return 1;
    return 0;
}

static int test_defaults_optional_arrays(void) {
    const char *minimal_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_min\","
        "\"objects\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(minimal_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (!runtime_json) return 1;
    if (!strstr(runtime_json, "\"materials\":[]")) return 1;
    if (!strstr(runtime_json, "\"lights\":[]")) return 1;
    if (!strstr(runtime_json, "\"cameras\":[]")) return 1;
    if (!strstr(runtime_json, "\"hierarchy\":[]")) return 1;
    if (!strstr(runtime_json, "\"constraints\":[]")) return 1;
    if (!strstr(runtime_json, "\"extensions\":{}")) return 1;
    core_free(runtime_json);
    return 0;
}

static int test_reject_duplicate_object_id(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_dup_obj\","
        "\"objects\":[{\"object_id\":\"obj_a\"},{\"object_id\":\"obj_a\"}],"
        "\"materials\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "duplicate object_id")) return 1;
    return 0;
}

static int test_reject_unresolved_material_ref(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bad_mat\","
        "\"objects\":[{\"object_id\":\"obj_a\",\"material_ref\":{\"id\":\"mat_missing\"}}],"
        "\"materials\":[{\"material_id\":\"mat_ok\"}]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "material_ref.id unresolved")) return 1;
    return 0;
}

static int test_accept_resolved_material_ref(void) {
    const char *ok_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_ok_mat\","
        "\"objects\":[{\"object_id\":\"obj_a\",\"material_ref\":{\"id\":\"mat_a\"}}],"
        "\"materials\":[{\"material_id\":\"mat_a\"}]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(ok_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (!runtime_json) return 1;
    core_free(runtime_json);
    return 0;
}

static int test_reject_invalid_space_mode_default(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_space_mode\","
        "\"space_mode_default\":\"4d\","
        "\"objects\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "space_mode_default")) return 1;
    return 0;
}

static int test_reject_non_positive_world_scale(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_scale\","
        "\"world_scale\":0,"
        "\"objects\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "world_scale")) return 1;
    return 0;
}

static int test_reject_unresolved_hierarchy_reference(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bad_hierarchy\","
        "\"objects\":[{\"object_id\":\"obj_a\"}],"
        "\"hierarchy\":[{\"parent_object_id\":\"obj_a\",\"child_object_id\":\"obj_missing\"}]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "child unresolved")) return 1;
    return 0;
}

static int test_deterministic_sorted_lanes(void) {
    const char *json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_sorted\","
        "\"objects\":[{\"object_id\":\"obj_b\"},{\"object_id\":\"obj_a\"}],"
        "\"hierarchy\":["
          "{\"parent_object_id\":\"obj_b\",\"child_object_id\":\"obj_a\"},"
          "{\"parent_object_id\":\"obj_a\",\"child_object_id\":\"obj_b\"}"
        "],"
        "\"materials\":[{\"material_id\":\"mat_b\"},{\"material_id\":\"mat_a\"}],"
        "\"lights\":[{\"light_id\":\"light_b\"},{\"light_id\":\"light_a\"}],"
        "\"cameras\":[{\"camera_id\":\"cam_b\"},{\"camera_id\":\"cam_a\"}]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    const char *obj_a = NULL;
    const char *obj_b = NULL;
    const char *mat_a = NULL;
    const char *mat_b = NULL;
    const char *light_a = NULL;
    const char *light_b = NULL;
    const char *cam_a = NULL;
    const char *cam_b = NULL;
    const char *hier_a_b = NULL;
    const char *hier_b_a = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (!runtime_json) return 1;

    obj_a = strstr(runtime_json, "\"object_id\":\"obj_a\"");
    obj_b = strstr(runtime_json, "\"object_id\":\"obj_b\"");
    mat_a = strstr(runtime_json, "\"material_id\":\"mat_a\"");
    mat_b = strstr(runtime_json, "\"material_id\":\"mat_b\"");
    light_a = strstr(runtime_json, "\"light_id\":\"light_a\"");
    light_b = strstr(runtime_json, "\"light_id\":\"light_b\"");
    cam_a = strstr(runtime_json, "\"camera_id\":\"cam_a\"");
    cam_b = strstr(runtime_json, "\"camera_id\":\"cam_b\"");
    hier_a_b = strstr(runtime_json, "\"parent_object_id\":\"obj_a\",\"child_object_id\":\"obj_b\"");
    hier_b_a = strstr(runtime_json, "\"parent_object_id\":\"obj_b\",\"child_object_id\":\"obj_a\"");

    if (!obj_a || !obj_b || obj_a > obj_b) return 1;
    if (!mat_a || !mat_b || mat_a > mat_b) return 1;
    if (!light_a || !light_b || light_a > light_b) return 1;
    if (!cam_a || !cam_b || cam_a > cam_b) return 1;
    if (!hier_a_b || !hier_b_a || hier_a_b > hier_b_a) return 1;

    core_free(runtime_json);
    return 0;
}

static int test_preserve_canonical_primitive_payloads(void) {
    const char *json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_primitives\","
        "\"space_mode_default\":\"3d\","
        "\"objects\":["
          "{"
            "\"object_id\":\"obj_prism\","
            "\"object_type\":\"rect_prism_primitive\","
            "\"dimensional_mode\":\"full_3d\","
            "\"primitive\":{"
              "\"kind\":\"rect_prism_primitive\","
              "\"width\":3.0,"
              "\"height\":2.0,"
              "\"depth\":4.0,"
              "\"lock_to_construction_plane\":false,"
              "\"lock_to_bounds\":true,"
              "\"frame\":{"
                "\"origin\":{\"x\":1.0,\"y\":2.0,\"z\":3.0},"
                "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
                "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}"
              "}"
            "}"
          "},"
          "{"
            "\"object_id\":\"obj_plane\","
            "\"object_type\":\"plane_primitive\","
            "\"dimensional_mode\":\"plane_locked\","
            "\"locked_plane\":\"xy\","
            "\"primitive\":{"
              "\"kind\":\"plane_primitive\","
              "\"width\":6.0,"
              "\"height\":5.0,"
              "\"lock_to_construction_plane\":true,"
              "\"lock_to_bounds\":false,"
              "\"frame\":{"
                "\"origin\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
                "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
                "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}"
              "}"
            "}"
          "}"
        "],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    const char *plane = NULL;
    const char *prism = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (!runtime_json) return 1;

    plane = strstr(runtime_json, "\"object_id\":\"obj_plane\"");
    prism = strstr(runtime_json, "\"object_id\":\"obj_prism\"");
    if (!plane || !prism || plane > prism) return 1;
    if (!strstr(runtime_json, "\"primitive\":{\"kind\":\"plane_primitive\",\"width\":6.0,\"height\":5.0")) return 1;
    if (!strstr(runtime_json, "\"primitive\":{\"kind\":\"rect_prism_primitive\",\"width\":3.0,\"height\":2.0,\"depth\":4.0")) return 1;

    core_free(runtime_json);
    return 0;
}

static int test_reject_missing_primitive_payload_for_primitive_object(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_missing_primitive\","
        "\"objects\":[{\"object_id\":\"obj_plane\",\"object_type\":\"plane_primitive\",\"dimensional_mode\":\"plane_locked\"}]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "requires primitive payload")) return 1;
    return 0;
}

static int test_reject_invalid_primitive_payload_shape(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bad_primitive\","
        "\"objects\":[{"
          "\"object_id\":\"obj_plane\","
          "\"object_type\":\"plane_primitive\","
          "\"dimensional_mode\":\"plane_locked\","
          "\"locked_plane\":\"xy\","
          "\"primitive\":{"
            "\"kind\":\"rect_prism_primitive\","
            "\"width\":0.0,"
            "\"height\":2.0,"
            "\"frame\":{"
              "\"origin\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_u\":{\"x\":1.0,\"y\":0.0,\"z\":0.0},"
              "\"axis_v\":{\"x\":0.0,\"y\":1.0,\"z\":0.0},"
              "\"normal\":{\"x\":0.0,\"y\":0.0,\"z\":1.0}"
            "}"
          "}"
        "}]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (!strstr(diagnostics, "invalid primitive payload")) return 1;
    return 0;
}

int main(void) {
    if (test_compile_success_and_preserve_extensions() != 0) return 1;
    if (test_reject_wrong_variant() != 0) return 1;
    if (test_defaults_optional_arrays() != 0) return 1;
    if (test_reject_duplicate_object_id() != 0) return 1;
    if (test_reject_unresolved_material_ref() != 0) return 1;
    if (test_accept_resolved_material_ref() != 0) return 1;
    if (test_reject_invalid_space_mode_default() != 0) return 1;
    if (test_reject_non_positive_world_scale() != 0) return 1;
    if (test_reject_unresolved_hierarchy_reference() != 0) return 1;
    if (test_deterministic_sorted_lanes() != 0) return 1;
    if (test_preserve_canonical_primitive_payloads() != 0) return 1;
    if (test_reject_missing_primitive_payload_for_primitive_object() != 0) return 1;
    if (test_reject_invalid_primitive_payload_shape() != 0) return 1;
    printf("core_scene_compile tests passed\n");
    return 0;
}
