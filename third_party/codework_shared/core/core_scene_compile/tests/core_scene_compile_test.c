#include "core_scene_compile.h"
#include "core_scene_overlay_merge_shared.h"

#include "core_base.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_text_file(const char *path, const char *text) {
    FILE *fp;
    if (!path || !text) return 1;
    fp = fopen(path, "wb");
    if (!fp) return 1;
    if (fputs(text, fp) == EOF) {
        fclose(fp);
        return 1;
    }
    if (fclose(fp) != 0) return 1;
    return 0;
}

static int run_scene_contract_diff(const char *expected_path, const char *actual_path) {
    char command[1024];
    int rc;
    if (!expected_path || !actual_path) return -1;
    snprintf(command, sizeof(command), "./build/scene_contract_diff %s %s >/dev/null 2>&1", expected_path, actual_path);
    rc = system(command);
    if (rc < 0) return -1;
    if (!WIFEXITED(rc)) return -1;
    return WEXITSTATUS(rc);
}

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
    if (!strstr(runtime_json, "\"normalization\":\"v0.7_content_addressed_dependency_payloads\"")) return 1;
    if (!strstr(runtime_json, "\"authoring_sha256\":")) return 1;
    if (!strstr(runtime_json, "\"dependency_sha256\":")) return 1;
    if (!strstr(runtime_json, "\"hierarchy\":[]")) return 1;
    if (!strstr(runtime_json, "\"extensions\":{\"ray_tracing\":{\"samples\":32},\"custom\":{\"x\":1}}")) return 1;
    core_free(runtime_json);
    return 0;
}

static int test_sha256_and_provenance_are_deterministic(void) {
    const char *authoring_json =
        "{\"schema_family\":\"codework_scene\",\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,\"scene_id\":\"scene_digest\",\"objects\":[]}";
    const CoreSceneCompileOptions options = {
        .dependency_digest_sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        .dependency_count = 2u,
        .dependency_manifest_json = NULL
    };
    CoreSceneCompileProvenance first = {0};
    CoreSceneCompileProvenance second = {0};
    char digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char diagnostics[256];
    char *first_runtime = NULL;
    char *second_runtime = NULL;
    CoreResult r;
    r = core_scene_compile_sha256("abc", 3u, digest);
    if (r.code != CORE_OK || strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) return 1;
    r = core_scene_compile_authoring_to_runtime_with_provenance(authoring_json,
                                                                &options,
                                                                &first_runtime,
                                                                &first,
                                                                diagnostics,
                                                                sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    r = core_scene_compile_authoring_to_runtime_with_provenance(authoring_json,
                                                                &options,
                                                                &second_runtime,
                                                                &second,
                                                                diagnostics,
                                                                sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (strcmp(first_runtime, second_runtime) != 0) return 1;
    if (strcmp(first.authoring_sha256, second.authoring_sha256) != 0 ||
        strcmp(first.runtime_sha256, second.runtime_sha256) != 0 ||
        strcmp(first.dependency_sha256, options.dependency_digest_sha256) != 0 ||
        first.dependency_count != 2u || strcmp(first.compiler_version, CORE_SCENE_COMPILE_VERSION) != 0) return 1;
    if (strstr(first_runtime, "compiled_at_ns") != NULL) return 1;
    core_free(first_runtime);
    core_free(second_runtime);
    return 0;
}

static int test_dependency_manifest_is_canonical_and_digest_bound(void) {
    const CoreSceneCompileDependency dependencies[] = {
        { .kind = "texture_runtime", .identity = "texture_z",
          .content_sha256 = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
          .content_bytes = 20u, .payload_data = NULL },
        { .kind = "mesh_asset_runtime", .identity = "mesh_a",
          .content_sha256 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          .content_bytes = 10u, .payload_data = NULL }
    };
    char *manifest = NULL;
    char digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char inspected_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char diagnostics[256];
    size_t count = 0u;
    const char *mesh_position;
    const char *texture_position;
    CoreResult result = core_scene_compile_dependency_manifest_build(dependencies,
                                                                     2u,
                                                                     &manifest,
                                                                     digest,
                                                                     diagnostics,
                                                                     sizeof(diagnostics));
    if (result.code != CORE_OK || !manifest) return 1;
    mesh_position = strstr(manifest, "\"identity\":\"mesh_a\"");
    texture_position = strstr(manifest, "\"identity\":\"texture_z\"");
    if (!mesh_position || !texture_position || mesh_position >= texture_position ||
        !strstr(manifest, "\"scene_dependency_manifest_v2\"") ||
        !strstr(manifest, "\"path\":\"dependencies/mesh_asset_runtime/aaaaaaaa")) return 1;
    result = core_scene_compile_dependency_manifest_inspect(manifest,
                                                             &count,
                                                             inspected_digest,
                                                             diagnostics,
                                                             sizeof(diagnostics));
    if (result.code != CORE_OK || count != 2u || strcmp(digest, inspected_digest) != 0) return 1;
    core_free(manifest);
    return 0;
}

static int test_bundle_publication_is_atomic_create_only(void) {
    const char *authoring_json =
        "{\"schema_family\":\"codework_scene\",\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,\"scene_id\":\"scene_bundle\",\"objects\":[]}";
    char root_template[] = "/tmp/core_scene_bundle_XXXXXX";
    char final_dir[512];
    char diagnostics[256];
    char receipt_text[4096];
    char payload_path[1024];
    char payload_relative[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE];
    char staging_dir[1024];
    CoreSceneCompileBundlePaths paths = {0};
    CoreSceneCompileVerification verification = {0};
    const char payload_bytes[] = "mesh-payload";
    const char tampered_payload[] = "mesh-tampered";
    const char package_manifest[] =
        "{\"schema_variant\":\"sculpt_scene_package_v1\","
        "\"authoring\":{\"path\":\"scene_authoring.json\"}}\n";
    CoreSceneCompileDependency dependency = {0};
    CoreSceneCompileOptions options = {0};
    char *manifest = NULL;
    char payload_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char manifest_digest[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    CoreResult result;
    FILE *receipt;
    json_object *receipt_json;
    size_t read_size;
    if (!mkdtemp(root_template)) return 1;
    if (snprintf(final_dir, sizeof(final_dir), "%s/published", root_template) >= (int)sizeof(final_dir)) return 1;
    dependency.kind = "mesh_asset_runtime";
    dependency.identity = "mesh_bundle";
    dependency.content_bytes = strlen(payload_bytes);
    dependency.payload_data = payload_bytes;
    if (core_scene_compile_sha256(payload_bytes,
                                  dependency.content_bytes,
                                  payload_digest).code != CORE_OK) return 1;
    dependency.content_sha256 = payload_digest;
    result = core_scene_compile_dependency_manifest_build(&dependency,
                                                           1u,
                                                           &manifest,
                                                           manifest_digest,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (result.code != CORE_OK) return 1;
    options.dependency_digest_sha256 = manifest_digest;
    options.dependency_count = 1u;
    options.dependency_manifest_json = manifest;
    if (snprintf(staging_dir, sizeof(staging_dir), "%s.staging-%ld", final_dir, (long)getpid()) >=
        (int)sizeof(staging_dir)) return 1;
    result = core_scene_compile_publish_bundle(authoring_json,
                                               &options,
                                               final_dir,
                                               NULL,
                                               diagnostics,
                                               sizeof(diagnostics));
    if (result.code == CORE_OK || access(final_dir, F_OK) == 0 || access(staging_dir, F_OK) == 0) return 1;
    options.dependency_payloads = &dependency;
    options.dependency_payload_count = 1u;
    options.package_manifest_json = package_manifest;
    result = core_scene_compile_publish_bundle(authoring_json,
                                               &options,
                                               final_dir,
                                               &paths,
                                               diagnostics,
                                               sizeof(diagnostics));
    if (result.code != CORE_OK || access(paths.authoring_path, F_OK) != 0 ||
        access(paths.runtime_path, F_OK) != 0 || access(paths.dependency_manifest_path, F_OK) != 0 ||
        access(paths.package_manifest_path, F_OK) != 0 ||
        access(paths.receipt_path, F_OK) != 0) return 1;
    receipt = fopen(paths.receipt_path, "rb");
    if (!receipt) return 1;
    read_size = fread(receipt_text, 1u, sizeof(receipt_text) - 1u, receipt);
    fclose(receipt);
    receipt_text[read_size] = '\0';
    receipt_json = json_tokener_parse(receipt_text);
    if (!receipt_json) return 1;
    json_object_put(receipt_json);
    if (!strstr(receipt_text, "\"scene_export_receipt_v1\"") ||
        !strstr(receipt_text, paths.bundle_sha256) || !strstr(receipt_text, CORE_SCENE_COMPILE_VERSION) ||
        !strstr(receipt_text, "scene_dependencies.json") ||
        !strstr(receipt_text, "scene_package.json")) return 1;
    result = core_scene_compile_verify_bundle(final_dir,
                                              paths.bundle_sha256,
                                              &verification,
                                              diagnostics,
                                              sizeof(diagnostics));
    if (result.code != CORE_OK || verification.dependency_count != 1u ||
        verification.dependency_payload_bytes != strlen(payload_bytes) ||
        verification.package_manifest_bytes != strlen(package_manifest) ||
        strlen(verification.package_manifest_sha256) != 64u ||
        strcmp(verification.dependency_sha256, manifest_digest) != 0) return 1;
    if (core_scene_compile_dependency_payload_path(dependency.kind,
                                                   dependency.content_sha256,
                                                   payload_relative).code != CORE_OK ||
        snprintf(payload_path, sizeof(payload_path), "%s/%s", final_dir, payload_relative) >=
            (int)sizeof(payload_path) || access(payload_path, F_OK) != 0) return 1;
    if (write_text_file(payload_path, tampered_payload) != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code == CORE_OK) return 1;
    if (write_text_file(payload_path, payload_bytes) != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code != CORE_OK) return 1;
    if (write_text_file(paths.package_manifest_path, "tampered") != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code == CORE_OK) return 1;
    if (write_text_file(paths.package_manifest_path, package_manifest) != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code != CORE_OK) return 1;
    if (unlink(payload_path) != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code == CORE_OK) return 1;
    if (symlink("/etc/hosts", payload_path) != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code == CORE_OK || unlink(payload_path) != 0) return 1;
    if (write_text_file(payload_path, payload_bytes) != 0) return 1;
    if (write_text_file(paths.runtime_path, "tampered") != 0) return 1;
    result = core_scene_compile_verify_bundle(final_dir, paths.bundle_sha256, NULL,
                                              diagnostics, sizeof(diagnostics));
    if (result.code == CORE_OK) return 1;
    result = core_scene_compile_publish_bundle(authoring_json,
                                               &options,
                                               final_dir,
                                               NULL,
                                               diagnostics,
                                               sizeof(diagnostics));
    if (result.code == CORE_OK || !strstr(diagnostics, "already exists")) return 1;
    core_free(manifest);
    unlink(payload_path);
    {
        char kind_dir[1024];
        char dependencies_dir[1024];
        if (snprintf(kind_dir, sizeof(kind_dir), "%s/dependencies/mesh_asset_runtime", final_dir) <
                (int)sizeof(kind_dir) &&
            snprintf(dependencies_dir, sizeof(dependencies_dir), "%s/dependencies", final_dir) <
                (int)sizeof(dependencies_dir)) {
            rmdir(kind_dir);
            rmdir(dependencies_dir);
        }
    }
    unlink(paths.receipt_path);
    unlink(paths.package_manifest_path);
    unlink(paths.dependency_manifest_path);
    unlink(paths.runtime_path);
    unlink(paths.authoring_path);
    rmdir(paths.scene_dir);
    rmdir(root_template);
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

static int test_reject_malformed_json_input(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_bad\","
        "\"objects\":[{\"object_id\":\"obj_a\"}]";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(bad_json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (runtime_json != NULL) return 1;
    if (diagnostics[0] == '\0') return 1;
    return 0;
}

static int test_reject_escaped_top_level_key_lookup(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema\\u005fvariant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_escape\","
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

static int test_duplicate_top_level_key_uses_first_match(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_dup_key\","
        "\"space_mode_default\":\"4d\","
        "\"space_mode_default\":\"2d\","
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

static int test_accept_mesh_asset_instance_geometry_ref(void) {
    const char *json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_mesh_asset\","
        "\"space_mode_default\":\"3d\","
        "\"objects\":[{"
          "\"object_id\":\"obj_mesh\","
          "\"object_type\":\"mesh_asset_instance\","
          "\"dimensional_mode\":\"full_3d\","
          "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_bookshelf_01\",\"variant\":\"runtime_default\"}"
        "}],"
        "\"materials\":[]"
        "}";
    char diagnostics[256];
    char *runtime_json = NULL;
    CoreResult r = core_scene_compile_authoring_to_runtime(json,
                                                           &runtime_json,
                                                           diagnostics,
                                                           sizeof(diagnostics));
    if (r.code != CORE_OK) return 1;
    if (!runtime_json) return 1;
    if (!strstr(runtime_json, "\"object_type\":\"mesh_asset_instance\"")) return 1;
    if (!strstr(runtime_json, "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_bookshelf_01\",\"variant\":\"runtime_default\"}")) return 1;
    core_free(runtime_json);
    return 0;
}

static int test_reject_mesh_asset_instance_wrong_geometry_ref_kind(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_mesh_asset_bad_kind\","
        "\"objects\":[{"
          "\"object_id\":\"obj_mesh\","
          "\"object_type\":\"mesh_asset_instance\","
          "\"dimensional_mode\":\"full_3d\","
          "\"geometry_ref\":{\"kind\":\"shape_asset\",\"id\":\"asset_bookshelf_01\"}"
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
    if (!strstr(diagnostics, "requires geometry_ref.kind mesh_asset")) return 1;
    return 0;
}

static int test_reject_mesh_asset_instance_wrong_dimensional_mode(void) {
    const char *bad_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_mesh_asset_bad_mode\","
        "\"objects\":[{"
          "\"object_id\":\"obj_mesh\","
          "\"object_type\":\"mesh_asset_instance\","
          "\"dimensional_mode\":\"plane_locked\","
          "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_bookshelf_01\"}"
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
    if (!strstr(diagnostics, "dimensional_mode must be full_3d")) return 1;
    return 0;
}

static int test_authoring_file_read_failure_diagnostics(void) {
    char diagnostics[256];
    CoreResult r = core_scene_compile_authoring_file_to_runtime_file("build/no_such_scene_authoring.json",
                                                                     "build/out_runtime.json",
                                                                     diagnostics,
                                                                     sizeof(diagnostics));
    if (r.code == CORE_OK) return 1;
    if (!strstr(diagnostics, "failed to read authoring file")) return 1;
    return 0;
}

static int test_runtime_file_write_failure_diagnostics(void) {
    const char *authoring_path = "build/core_scene_compile_authoring_input.json";
    const char *authoring_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_write_fail\","
        "\"objects\":[]"
        "}";
    char diagnostics[256];
    CoreResult r;

    if (write_text_file(authoring_path, authoring_json) != 0) return 1;
    r = core_scene_compile_authoring_file_to_runtime_file(authoring_path,
                                                          "build/missing_dir/runtime.json",
                                                          diagnostics,
                                                          sizeof(diagnostics));
    remove(authoring_path);
    if (r.code == CORE_OK) return 1;
    if (!strstr(diagnostics, "failed to write runtime file")) return 1;
    return 0;
}

static int test_overlay_merge_rejects_wrong_producer(void) {
    json_object *runtime_root = json_tokener_parse("{\"space_mode_default\":\"2d\",\"extensions\":{}}");
    json_object *overlay_root =
        json_tokener_parse("{\"overlay_meta\":{\"producer\":\"ray_bridge\",\"logical_clock\":1}}");
    char diagnostics[256];
    int failed = 0;
    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        core_scene_overlay_merge_apply(runtime_root,
                                       overlay_root,
                                       "ray_tracing",
                                       "physics_bridge",
                                       diagnostics,
                                       sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && !strstr(diagnostics, "producer not allowed")) failed = 1;
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_overlay_merge_rejects_stale_clock(void) {
    json_object *runtime_root = json_tokener_parse(
        "{"
        "\"space_mode_default\":\"2d\","
        "\"extensions\":{\"overlay_merge\":{\"producer_clocks\":{\"physics_bridge\":5}}}"
        "}");
    json_object *overlay_root = json_tokener_parse(
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_bridge\",\"logical_clock\":5},"
        "\"extensions\":{\"physics_sim\":{\"density\":1}}"
        "}");
    char diagnostics[256];
    int failed = 0;
    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        core_scene_overlay_merge_apply(runtime_root,
                                       overlay_root,
                                       "physics_sim",
                                       "physics_bridge",
                                       diagnostics,
                                       sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && !strstr(diagnostics, "logical_clock is stale")) failed = 1;
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_overlay_merge_rejects_equal_clock_tie_break_loss(void) {
    json_object *runtime_root = json_tokener_parse(
        "{"
        "\"space_mode_default\":\"2d\","
        "\"extensions\":{\"overlay_merge\":{\"space_mode_default\":{\"producer\":\"a_bridge\",\"logical_clock\":7}}}"
        "}");
    json_object *overlay_root = json_tokener_parse(
        "{"
        "\"overlay_meta\":{\"producer\":\"z_bridge\",\"logical_clock\":7},"
        "\"space_mode_default\":\"3d\""
        "}");
    char diagnostics[256];
    int failed = 0;
    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        core_scene_overlay_merge_apply(runtime_root,
                                       overlay_root,
                                       "physics_sim",
                                       "z_bridge",
                                       diagnostics,
                                       sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && !strstr(diagnostics, "tie-break lost")) failed = 1;
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_overlay_merge_rejects_forbidden_top_level_key(void) {
    json_object *runtime_root = json_tokener_parse("{\"space_mode_default\":\"2d\",\"extensions\":{}}");
    json_object *overlay_root = json_tokener_parse(
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_bridge\",\"logical_clock\":1},"
        "\"scene_id\":\"not_allowed\""
        "}");
    char diagnostics[256];
    int failed = 0;
    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        core_scene_overlay_merge_apply(runtime_root,
                                       overlay_root,
                                       "physics_sim",
                                       "physics_bridge",
                                       diagnostics,
                                       sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && !strstr(diagnostics, "overlay key not allowed")) failed = 1;
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_overlay_merge_rejects_invalid_extension_namespace(void) {
    json_object *runtime_root = json_tokener_parse("{\"space_mode_default\":\"2d\",\"extensions\":{}}");
    json_object *overlay_root = json_tokener_parse(
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_bridge\",\"logical_clock\":2},"
        "\"extensions\":{\"ray_tracing\":{\"samples\":4}}"
        "}");
    char diagnostics[256];
    int failed = 0;
    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        core_scene_overlay_merge_apply(runtime_root,
                                       overlay_root,
                                       "physics_sim",
                                       "physics_bridge",
                                       diagnostics,
                                       sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && !strstr(diagnostics, "namespace not allowed")) failed = 1;
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_overlay_merge_rejects_invalid_space_mode_value(void) {
    json_object *runtime_root = json_tokener_parse("{\"space_mode_default\":\"2d\",\"extensions\":{}}");
    json_object *overlay_root = json_tokener_parse(
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_bridge\",\"logical_clock\":3},"
        "\"space_mode_default\":\"4d\""
        "}");
    char diagnostics[256];
    int failed = 0;
    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        core_scene_overlay_merge_apply(runtime_root,
                                       overlay_root,
                                       "physics_sim",
                                       "physics_bridge",
                                       diagnostics,
                                       sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && !strstr(diagnostics, "space_mode_default")) failed = 1;
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_overlay_merge_persists_clock_and_namespace(void) {
    json_object *runtime_root = json_tokener_parse("{\"space_mode_default\":\"2d\",\"extensions\":{}}");
    json_object *overlay_root = json_tokener_parse(
        "{"
        "\"overlay_meta\":{\"producer\":\"physics_bridge\",\"logical_clock\":9},"
        "\"space_mode_default\":\"3d\","
        "\"extensions\":{\"physics_sim\":{\"density\":1,\"mode\":\"draft\"}}"
        "}");
    json_object *extensions = NULL;
    json_object *overlay_merge = NULL;
    json_object *producer_clocks = NULL;
    json_object *producer_clock = NULL;
    json_object *space_mode = NULL;
    json_object *physics_ext = NULL;
    char diagnostics[256];
    int failed = 0;

    if (!runtime_root || !overlay_root) failed = 1;
    if (!failed &&
        !core_scene_overlay_merge_apply(runtime_root,
                                        overlay_root,
                                        "physics_sim",
                                        "physics_bridge",
                                        diagnostics,
                                        sizeof(diagnostics))) {
        failed = 1;
    }
    if (!failed && strcmp(diagnostics, "ok") != 0) failed = 1;
    if (!failed &&
        (!json_object_object_get_ex(runtime_root, "space_mode_default", &space_mode) ||
         strcmp(json_object_get_string(space_mode), "3d") != 0)) {
        failed = 1;
    }
    if (!failed &&
        (!json_object_object_get_ex(runtime_root, "extensions", &extensions) ||
         !json_object_object_get_ex(extensions, "physics_sim", &physics_ext) ||
         !json_object_is_type(physics_ext, json_type_object))) {
        failed = 1;
    }
    if (!failed &&
        (!json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) ||
         !json_object_object_get_ex(overlay_merge, "producer_clocks", &producer_clocks) ||
         !json_object_object_get_ex(producer_clocks, "physics_bridge", &producer_clock) ||
         json_object_get_int64(producer_clock) != 9)) {
        failed = 1;
    }

    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return failed;
}

static int test_scene_contract_diff_ignores_volatile_lanes_and_id_order(void) {
    const char *expected_path = "build/scene_contract_diff_expected.json";
    const char *actual_path = "build/scene_contract_diff_actual.json";
    const char *expected_json =
        "{"
        "\"compile_meta\":{\"compiled_at_ns\":1},"
        "\"extensions\":{\"overlay_merge\":{\"producer_clocks\":{\"bridge\":1}},\"custom\":{\"x\":1}},"
        "\"objects\":[{\"object_id\":\"a\",\"value\":1},{\"object_id\":\"b\",\"value\":2}]"
        "}";
    const char *actual_json =
        "{"
        "\"compile_meta\":{\"compiled_at_ns\":999},"
        "\"extensions\":{\"overlay_merge\":{\"producer_clocks\":{\"bridge\":99}},\"custom\":{\"x\":1}},"
        "\"objects\":[{\"object_id\":\"b\",\"value\":2},{\"object_id\":\"a\",\"value\":1}]"
        "}";
    int rc;
    if (write_text_file(expected_path, expected_json) != 0) return 1;
    if (write_text_file(actual_path, actual_json) != 0) return 1;
    rc = run_scene_contract_diff(expected_path, actual_path);
    remove(expected_path);
    remove(actual_path);
    return rc != 0;
}

static int test_scene_contract_diff_rejects_duplicate_ids(void) {
    const char *expected_path = "build/scene_contract_diff_dup_expected.json";
    const char *actual_path = "build/scene_contract_diff_dup_actual.json";
    const char *expected_json = "{\"objects\":[{\"object_id\":\"a\"},{\"object_id\":\"b\"}]}";
    const char *actual_json = "{\"objects\":[{\"object_id\":\"a\"},{\"object_id\":\"a\"}]}";
    int rc;
    if (write_text_file(expected_path, expected_json) != 0) return 1;
    if (write_text_file(actual_path, actual_json) != 0) return 1;
    rc = run_scene_contract_diff(expected_path, actual_path);
    remove(expected_path);
    remove(actual_path);
    return rc == 0;
}

static int test_scene_contract_diff_numeric_tolerance(void) {
    const char *expected_path = "build/scene_contract_diff_num_expected.json";
    const char *close_path = "build/scene_contract_diff_num_close.json";
    const char *far_path = "build/scene_contract_diff_num_far.json";
    int rc_close;
    int rc_far;
    if (write_text_file(expected_path, "{\"world_scale\":1.0}") != 0) return 1;
    if (write_text_file(close_path, "{\"world_scale\":1.0000000001}") != 0) return 1;
    if (write_text_file(far_path, "{\"world_scale\":1.00001}") != 0) return 1;
    rc_close = run_scene_contract_diff(expected_path, close_path);
    rc_far = run_scene_contract_diff(expected_path, far_path);
    remove(expected_path);
    remove(close_path);
    remove(far_path);
    if (rc_close != 0) return 1;
    if (rc_far == 0) return 1;
    return 0;
}

int main(void) {
#define RUN_TEST(fn) do { if (fn() != 0) { fprintf(stderr, "%s failed\n", #fn); return 1; } } while (0)
    RUN_TEST(test_compile_success_and_preserve_extensions);
    RUN_TEST(test_sha256_and_provenance_are_deterministic);
    RUN_TEST(test_dependency_manifest_is_canonical_and_digest_bound);
    RUN_TEST(test_bundle_publication_is_atomic_create_only);
    RUN_TEST(test_reject_wrong_variant);
    RUN_TEST(test_reject_malformed_json_input);
    RUN_TEST(test_reject_escaped_top_level_key_lookup);
    RUN_TEST(test_duplicate_top_level_key_uses_first_match);
    RUN_TEST(test_defaults_optional_arrays);
    RUN_TEST(test_reject_duplicate_object_id);
    RUN_TEST(test_reject_unresolved_material_ref);
    RUN_TEST(test_accept_resolved_material_ref);
    RUN_TEST(test_reject_invalid_space_mode_default);
    RUN_TEST(test_reject_non_positive_world_scale);
    RUN_TEST(test_reject_unresolved_hierarchy_reference);
    RUN_TEST(test_deterministic_sorted_lanes);
    RUN_TEST(test_preserve_canonical_primitive_payloads);
    RUN_TEST(test_reject_missing_primitive_payload_for_primitive_object);
    RUN_TEST(test_reject_invalid_primitive_payload_shape);
    RUN_TEST(test_accept_mesh_asset_instance_geometry_ref);
    RUN_TEST(test_reject_mesh_asset_instance_wrong_geometry_ref_kind);
    RUN_TEST(test_reject_mesh_asset_instance_wrong_dimensional_mode);
    RUN_TEST(test_authoring_file_read_failure_diagnostics);
    RUN_TEST(test_runtime_file_write_failure_diagnostics);
    RUN_TEST(test_overlay_merge_rejects_wrong_producer);
    RUN_TEST(test_overlay_merge_rejects_stale_clock);
    RUN_TEST(test_overlay_merge_rejects_equal_clock_tie_break_loss);
    RUN_TEST(test_overlay_merge_rejects_forbidden_top_level_key);
    RUN_TEST(test_overlay_merge_rejects_invalid_extension_namespace);
    RUN_TEST(test_overlay_merge_rejects_invalid_space_mode_value);
    RUN_TEST(test_overlay_merge_persists_clock_and_namespace);
    RUN_TEST(test_scene_contract_diff_ignores_volatile_lanes_and_id_order);
    RUN_TEST(test_scene_contract_diff_rejects_duplicate_ids);
    RUN_TEST(test_scene_contract_diff_numeric_tolerance);
#undef RUN_TEST
    printf("core_scene_compile tests passed\n");
    return 0;
}
