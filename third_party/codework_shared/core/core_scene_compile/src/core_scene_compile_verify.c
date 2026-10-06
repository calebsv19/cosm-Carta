/*
 * core_scene_compile_verify.c
 * Strict integrity verification for published scene bundles.
 */

#include "core_scene_compile.h"

#include "core_io.h"

#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void verify_diag(char *diagnostics, size_t size, const char *message) {
    if (diagnostics && size > 0u) snprintf(diagnostics, size, "%s", message ? message : "");
}

static bool build_verify_path(const char *root, const char *leaf, char *out, size_t out_size) {
    int written = snprintf(out, out_size, "%s/%s", root, leaf);
    return written > 0 && (size_t)written < out_size;
}

static char *buffer_to_string(const CoreBuffer *buffer) {
    char *text;
    if (!buffer || (!buffer->data && buffer->size > 0u)) return NULL;
    text = (char *)core_alloc(buffer->size + 1u);
    if (!text) return NULL;
    if (buffer->size > 0u) memcpy(text, buffer->data, buffer->size);
    text[buffer->size] = '\0';
    return text;
}

static bool extract_string(const char *json, const char *marker, char *out, size_t out_size) {
    const char *start = strstr(json, marker);
    const char *end;
    size_t length;
    if (!start) return false;
    start += strlen(marker);
    end = strchr(start, '"');
    if (!end) return false;
    length = (size_t)(end - start);
    if (length == 0u || length >= out_size) return false;
    memcpy(out, start, length);
    out[length] = '\0';
    return true;
}

static bool extract_size(const char *json, const char *marker, size_t *out_value) {
    const char *start = strstr(json, marker);
    char *end = NULL;
    unsigned long long value;
    if (!start || !out_value) return false;
    start += strlen(marker);
    if (!isdigit((unsigned char)*start)) return false;
    value = strtoull(start, &end, 10);
    if (!end || end == start || value > (unsigned long long)SIZE_MAX) return false;
    *out_value = (size_t)value;
    return true;
}

static bool digest_equals(const char *lhs, const char *rhs) {
    return lhs && rhs && strlen(lhs) == 64u && strlen(rhs) == 64u && strcmp(lhs, rhs) == 0;
}

CoreResult core_scene_compile_verify_bundle(
    const char *scene_dir,
    const char *expected_bundle_sha256,
    CoreSceneCompileVerification *out_verification,
    char *diagnostics,
    size_t diagnostics_size) {
    char authoring_path[1024];
    char runtime_path[1024];
    char dependency_path[1024];
    char package_manifest_path[1024];
    char receipt_path[1024];
    CoreBuffer authoring = {0};
    CoreBuffer runtime = {0};
    CoreBuffer dependencies = {0};
    CoreBuffer package_manifest = {0};
    CoreBuffer receipt = {0};
    char *runtime_text = NULL;
    char *dependency_text = NULL;
    char *package_manifest_text = NULL;
    char *receipt_text = NULL;
    CoreSceneCompileVerification verified = {0};
    char receipt_authoring_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char receipt_runtime_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char receipt_dependency_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char receipt_package_manifest_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE] = {0};
    char receipt_bundle_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char runtime_authoring_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char runtime_dependency_sha[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
    char runtime_compiler_version[32];
    char runtime_normalization_version[96];
    char bundle_input[768];
    size_t receipt_authoring_bytes = 0u;
    size_t receipt_runtime_bytes = 0u;
    size_t receipt_dependency_bytes = 0u;
    size_t receipt_dependency_count = 0u;
    size_t receipt_package_manifest_bytes = 0u;
    size_t runtime_dependency_count = 0u;
    size_t verified_payload_count = 0u;
    const char *authoring_section = NULL;
    const char *runtime_section = NULL;
    const char *dependency_section = NULL;
    const char *package_manifest_section = NULL;
    bool has_package_manifest = false;
    int written;
    CoreResult result = { CORE_ERR_FORMAT, "scene bundle verification failed" };

    if (!scene_dir || !scene_dir[0]) return (CoreResult){ CORE_ERR_INVALID_ARG, "scene directory missing" };
    if (out_verification) memset(out_verification, 0, sizeof(*out_verification));
    verify_diag(diagnostics, diagnostics_size, NULL);
    if (!build_verify_path(scene_dir, "scene_authoring.json", authoring_path, sizeof(authoring_path)) ||
        !build_verify_path(scene_dir, "scene_runtime.json", runtime_path, sizeof(runtime_path)) ||
        !build_verify_path(scene_dir, "scene_dependencies.json", dependency_path, sizeof(dependency_path)) ||
        !build_verify_path(scene_dir, "scene_package.json", package_manifest_path, sizeof(package_manifest_path)) ||
        !build_verify_path(scene_dir, "scene_export_receipt.json", receipt_path, sizeof(receipt_path))) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "scene bundle path too long" };
    }
    if (core_io_read_all(receipt_path, &receipt).code != CORE_OK) {
        verify_diag(diagnostics, diagnostics_size, "scene bundle is missing its export receipt");
        result = (CoreResult){ CORE_ERR_IO, "missing scene bundle receipt" };
        goto cleanup;
    }
    receipt_text = buffer_to_string(&receipt);
    if (!receipt_text) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    has_package_manifest = strstr(receipt_text, "\"package_manifest\":") != NULL;
    if (core_io_read_all(authoring_path, &authoring).code != CORE_OK ||
        core_io_read_all(runtime_path, &runtime).code != CORE_OK ||
        core_io_read_all(dependency_path, &dependencies).code != CORE_OK ||
        (has_package_manifest &&
         core_io_read_all(package_manifest_path, &package_manifest).code != CORE_OK)) {
        verify_diag(diagnostics, diagnostics_size, "scene bundle is missing a required artifact");
        result = (CoreResult){ CORE_ERR_IO, "missing scene bundle artifact" };
        goto cleanup;
    }
    runtime_text = buffer_to_string(&runtime);
    dependency_text = buffer_to_string(&dependencies);
    if (has_package_manifest) package_manifest_text = buffer_to_string(&package_manifest);
    if (!runtime_text || !dependency_text || (has_package_manifest && !package_manifest_text)) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    if (!strstr(receipt_text, "\"schema_variant\":\"scene_export_receipt_v1\"") ||
        !strstr(receipt_text, "\"mode\":\"create_only_atomic_directory\"") ||
        !strstr(receipt_text, "\"path\":\"scene_authoring.json\"") ||
        !strstr(receipt_text, "\"path\":\"scene_runtime.json\"") ||
        !strstr(receipt_text, "\"path\":\"scene_dependencies.json\"")) {
        verify_diag(diagnostics, diagnostics_size, "unsupported or incomplete scene export receipt");
        goto cleanup;
    }
    if (!extract_string(receipt_text, "\"compiler\":{\"name\":\"core_scene_compile\",\"version\":\"",
                        verified.compiler_version, sizeof(verified.compiler_version)) ||
        !extract_string(receipt_text, "\"normalization\":\"", verified.normalization_version,
                        sizeof(verified.normalization_version)) ||
        !extract_string(receipt_text, "\"authoring\":{\"path\":\"scene_authoring.json\",\"sha256\":\"",
                        receipt_authoring_sha, sizeof(receipt_authoring_sha)) ||
        !extract_string(receipt_text, "\"runtime\":{\"path\":\"scene_runtime.json\",\"sha256\":\"",
                        receipt_runtime_sha, sizeof(receipt_runtime_sha)) ||
        !extract_string(receipt_text, "\"dependencies\":{\"path\":\"scene_dependencies.json\",\"sha256\":\"",
                        receipt_dependency_sha, sizeof(receipt_dependency_sha)) ||
        !extract_string(receipt_text, "\"bundle_sha256\":\"", receipt_bundle_sha,
                        sizeof(receipt_bundle_sha))) {
        verify_diag(diagnostics, diagnostics_size, "scene export receipt fields are malformed");
        goto cleanup;
    }
    authoring_section = strstr(receipt_text, "\"authoring\":");
    runtime_section = strstr(receipt_text, "\"runtime\":");
    dependency_section = strstr(receipt_text, "\"dependencies\":");
    package_manifest_section = strstr(receipt_text, "\"package_manifest\":");
    if (!authoring_section || !runtime_section || !dependency_section ||
        !extract_size(authoring_section, "\"bytes\":", &receipt_authoring_bytes) ||
        !extract_size(runtime_section, "\"bytes\":", &receipt_runtime_bytes) ||
        !extract_size(dependency_section, "\"bytes\":", &receipt_dependency_bytes) ||
        !extract_size(dependency_section, "\"count\":", &receipt_dependency_count)) {
        verify_diag(diagnostics, diagnostics_size, "scene export receipt fields are malformed");
        goto cleanup;
    }
    if (has_package_manifest &&
        (!package_manifest_section ||
         !extract_string(package_manifest_section,
                         "\"path\":\"scene_package.json\",\"sha256\":\"",
                         receipt_package_manifest_sha,
                         sizeof(receipt_package_manifest_sha)) ||
         !extract_size(package_manifest_section, "\"bytes\":", &receipt_package_manifest_bytes))) {
        verify_diag(diagnostics, diagnostics_size, "scene package manifest receipt fields are malformed");
        goto cleanup;
    }
    if (core_scene_compile_sha256(authoring.data, authoring.size, verified.authoring_sha256).code != CORE_OK ||
        core_scene_compile_sha256(runtime.data, runtime.size, verified.runtime_sha256).code != CORE_OK ||
        core_scene_compile_dependency_manifest_inspect(dependency_text,
                                                       &verified.dependency_count,
                                                       verified.dependency_sha256,
                                                       diagnostics,
                                                       diagnostics_size).code != CORE_OK) {
        goto cleanup;
    }
    if (has_package_manifest &&
        (core_scene_compile_sha256(package_manifest.data,
                                   package_manifest.size,
                                   verified.package_manifest_sha256).code != CORE_OK ||
         package_manifest.size != receipt_package_manifest_bytes ||
         !digest_equals(verified.package_manifest_sha256, receipt_package_manifest_sha))) {
        verify_diag(diagnostics, diagnostics_size, "scene package manifest digest or size mismatch");
        goto cleanup;
    }
    verified.authoring_bytes = authoring.size;
    verified.runtime_bytes = runtime.size;
    if (authoring.size != receipt_authoring_bytes || runtime.size != receipt_runtime_bytes ||
        dependencies.size != receipt_dependency_bytes || verified.dependency_count != receipt_dependency_count ||
        !digest_equals(verified.authoring_sha256, receipt_authoring_sha) ||
        !digest_equals(verified.runtime_sha256, receipt_runtime_sha) ||
        !digest_equals(verified.dependency_sha256, receipt_dependency_sha)) {
        verify_diag(diagnostics, diagnostics_size, "scene bundle artifact digest, size, or dependency count mismatch");
        goto cleanup;
    }
    result = core_scene_compile_dependency_payloads_verify(scene_dir,
                                                           dependency_text,
                                                           &verified_payload_count,
                                                           &verified.dependency_payload_bytes,
                                                           diagnostics,
                                                           diagnostics_size);
    if (result.code != CORE_OK || verified_payload_count != verified.dependency_count) {
        if (result.code == CORE_OK) {
            verify_diag(diagnostics, diagnostics_size, "dependency payload count does not match manifest");
            result = (CoreResult){ CORE_ERR_FORMAT, "dependency payload count mismatch" };
        }
        goto cleanup;
    }
    if (!extract_string(runtime_text, "\"authoring_sha256\":\"", runtime_authoring_sha,
                        sizeof(runtime_authoring_sha)) ||
        !extract_string(runtime_text, "\"dependency_sha256\":\"", runtime_dependency_sha,
                        sizeof(runtime_dependency_sha)) ||
        !extract_string(runtime_text, "\"compiler_version\":\"", runtime_compiler_version,
                        sizeof(runtime_compiler_version)) ||
        !extract_string(runtime_text, "\"normalization\":\"", runtime_normalization_version,
                        sizeof(runtime_normalization_version)) ||
        !extract_size(runtime_text, "\"dependency_count\":", &runtime_dependency_count) ||
        !digest_equals(runtime_authoring_sha, verified.authoring_sha256) ||
        !digest_equals(runtime_dependency_sha, verified.dependency_sha256) ||
        runtime_dependency_count != verified.dependency_count ||
        strcmp(runtime_compiler_version, verified.compiler_version) != 0 ||
        strcmp(runtime_normalization_version, verified.normalization_version) != 0) {
        verify_diag(diagnostics, diagnostics_size, "runtime compile provenance does not match bundle artifacts");
        goto cleanup;
    }
    verified.package_manifest_bytes = package_manifest.size;
    written = snprintf(bundle_input, sizeof(bundle_input),
                       "authoring:%s\nruntime:%s\ndependencies:%s\npackage:%s\ncompiler:%s\nnormalization:%s\n",
                       verified.authoring_sha256, verified.runtime_sha256, verified.dependency_sha256,
                       has_package_manifest ? verified.package_manifest_sha256 : "none",
                       verified.compiler_version, verified.normalization_version);
    if (written <= 0 || (size_t)written >= sizeof(bundle_input) ||
        core_scene_compile_sha256(bundle_input, (size_t)written, verified.bundle_sha256).code != CORE_OK ||
        !digest_equals(verified.bundle_sha256, receipt_bundle_sha) ||
        (expected_bundle_sha256 && !digest_equals(verified.bundle_sha256, expected_bundle_sha256))) {
        verify_diag(diagnostics, diagnostics_size, "scene bundle receipt or expected bundle digest mismatch");
        goto cleanup;
    }
    if (strcmp(verified.compiler_version, CORE_SCENE_COMPILE_VERSION) != 0) {
        verify_diag(diagnostics, diagnostics_size, "scene bundle compiler version is unsupported");
        result = (CoreResult){ CORE_ERR_FORMAT, "unsupported compiler version" };
        goto cleanup;
    }
    if (out_verification) *out_verification = verified;
    verify_diag(diagnostics, diagnostics_size, "ok");
    result = core_result_ok();

cleanup:
    core_free(runtime_text);
    core_free(dependency_text);
    core_free(package_manifest_text);
    core_free(receipt_text);
    core_io_buffer_free(&authoring);
    core_io_buffer_free(&runtime);
    core_io_buffer_free(&dependencies);
    core_io_buffer_free(&package_manifest);
    core_io_buffer_free(&receipt);
    return result;
}
