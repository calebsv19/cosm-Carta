#ifndef CORE_SCENE_COMPILE_H
#define CORE_SCENE_COMPILE_H

#include <stddef.h>

#include "core_base.h"

#define CORE_SCENE_COMPILE_VERSION "0.8.0"
#define CORE_SCENE_COMPILE_SHA256_HEX_SIZE 65u
#define CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE 384u

typedef struct CoreSceneCompileDependency {
    const char *kind;
    const char *identity;
    const char *content_sha256;
    size_t content_bytes;
    const void *payload_data;
} CoreSceneCompileDependency;

typedef struct CoreSceneCompileOptions {
    const char *dependency_digest_sha256;
    size_t dependency_count;
    const char *dependency_manifest_json;
    const CoreSceneCompileDependency *dependency_payloads;
    size_t dependency_payload_count;
    /* Optional app-authored package entrypoint retained and receipt-bound by
     * the shared atomic publication transaction. */
    const char *package_manifest_json;
} CoreSceneCompileOptions;

typedef struct CoreSceneCompileProvenance {
    char authoring_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char runtime_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char dependency_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    size_t dependency_count;
    const char *compiler_version;
    const char *normalization_version;
} CoreSceneCompileProvenance;

typedef struct CoreSceneCompileBundlePaths {
    char scene_dir[1024];
    char authoring_path[1024];
    char runtime_path[1024];
    char dependency_manifest_path[1024];
    char package_manifest_path[1024];
    char receipt_path[1024];
    char bundle_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
} CoreSceneCompileBundlePaths;

typedef struct CoreSceneCompileVerification {
    char authoring_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char runtime_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char dependency_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char package_manifest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char bundle_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    size_t dependency_count;
    size_t dependency_payload_bytes;
    size_t package_manifest_bytes;
    size_t authoring_bytes;
    size_t runtime_bytes;
    char compiler_version[32];
    char normalization_version[96];
} CoreSceneCompileVerification;

#ifdef __cplusplus
extern "C" {
#endif

CoreResult core_scene_compile_authoring_to_runtime(const char *authoring_json,
                                                   char **out_runtime_json,
                                                   char *diagnostics,
                                                   size_t diagnostics_size);

CoreResult core_scene_compile_authoring_to_runtime_with_provenance(
    const char *authoring_json,
    const CoreSceneCompileOptions *options,
    char **out_runtime_json,
    CoreSceneCompileProvenance *out_provenance,
    char *diagnostics,
    size_t diagnostics_size);

CoreResult core_scene_compile_sha256(const void *data,
                                     size_t data_size,
                                     char out_hex[CORE_SCENE_COMPILE_SHA256_HEX_SIZE]);

CoreResult core_scene_compile_dependency_manifest_build(
    const CoreSceneCompileDependency *dependencies,
    size_t dependency_count,
    char **out_manifest_json,
    char out_digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE],
    char *diagnostics,
    size_t diagnostics_size);

CoreResult core_scene_compile_dependency_manifest_inspect(
    const char *manifest_json,
    size_t *out_dependency_count,
    char out_digest_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE],
    char *diagnostics,
    size_t diagnostics_size);

CoreResult core_scene_compile_dependency_payload_path(
    const char *kind,
    const char *content_sha256,
    char out_path[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE]);

CoreResult core_scene_compile_dependency_payloads_verify(
    const char *scene_dir,
    const char *manifest_json,
    size_t *out_payload_count,
    size_t *out_payload_bytes,
    char *diagnostics,
    size_t diagnostics_size);

CoreResult core_scene_compile_publish_bundle(const char *authoring_json,
                                             const CoreSceneCompileOptions *options,
                                             const char *final_scene_dir,
                                             CoreSceneCompileBundlePaths *out_paths,
                                             char *diagnostics,
                                             size_t diagnostics_size);

CoreResult core_scene_compile_verify_bundle(
    const char *scene_dir,
    const char *expected_bundle_sha256,
    CoreSceneCompileVerification *out_verification,
    char *diagnostics,
    size_t diagnostics_size);

CoreResult core_scene_compile_authoring_file_to_runtime_file(const char *authoring_path,
                                                             const char *runtime_path,
                                                             char *diagnostics,
                                                             size_t diagnostics_size);

#ifdef __cplusplus
}
#endif

#endif
