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

int main(void) {
    if (test_compile_success_and_preserve_extensions() != 0) return 1;
    if (test_reject_wrong_variant() != 0) return 1;
    if (test_defaults_optional_arrays() != 0) return 1;
    if (test_reject_duplicate_object_id() != 0) return 1;
    if (test_reject_unresolved_material_ref() != 0) return 1;
    if (test_accept_resolved_material_ref() != 0) return 1;
    printf("core_scene_compile tests passed\n");
    return 0;
}
