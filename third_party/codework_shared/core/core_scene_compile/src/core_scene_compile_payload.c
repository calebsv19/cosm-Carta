#if defined(__linux__)
#define _GNU_SOURCE
#endif

/*
 * core_scene_compile_payload.c
 * Atomic staging support for content-addressed dependency payloads.
 */

#include "core_scene_compile_payload_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static void payload_diag(char *diagnostics, size_t size, const char *message) {
    if (diagnostics && size > 0u) snprintf(diagnostics, size, "%s", message ? message : "");
}
static bool join_path(const char *root, const char *relative, char *out, size_t out_size) {
    int written = snprintf(out, out_size, "%s/%s", root, relative);
    return written > 0 && (size_t)written < out_size;
}

static bool same_payload_path(const CoreSceneCompileDependency *lhs,
                              const CoreSceneCompileDependency *rhs) {
    return strcmp(lhs->kind, rhs->kind) == 0 &&
           strcmp(lhs->content_sha256, rhs->content_sha256) == 0;
}

static bool write_payload_create_only(const char *path, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t offset = 0u;
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
    if (fd < 0) return false;
    while (offset < size) {
        ssize_t amount = write(fd, bytes + offset, size - offset);
        if (amount <= 0) {
            (void)close(fd);
            return false;
        }
        offset += (size_t)amount;
    }
    if (fsync(fd) != 0) {
        (void)close(fd);
        return false;
    }
    return close(fd) == 0;
}

static bool sync_directory_path(const char *path) {
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    int result;
    if (fd < 0) return false;
    result = fsync(fd);
    (void)close(fd);
    return result == 0;
}

CoreResult core_scene_compile_dependency_payloads_publish(
    const char *staging_dir,
    const CoreSceneCompileDependency *dependencies,
    size_t dependency_count,
    char *diagnostics,
    size_t diagnostics_size) {
    char dependencies_dir[1024];
    if (!staging_dir || (!dependencies && dependency_count > 0u)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid dependency payload publication arguments" };
    }
    if (dependency_count == 0u) return core_result_ok();
    if (!join_path(staging_dir, "dependencies", dependencies_dir, sizeof(dependencies_dir)) ||
        mkdir(dependencies_dir, 0755) != 0) {
        payload_diag(diagnostics, diagnostics_size, "failed to create dependency payload directory");
        return (CoreResult){ CORE_ERR_IO, "failed to create dependency payload directory" };
    }
    for (size_t i = 0u; i < dependency_count; ++i) {
        char relative_path[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE];
        char kind_relative[96];
        char kind_dir[1024];
        char payload_path[1024];
        char actual_sha256[CORE_SCENE_COMPILE_SHA256_HEX_SIZE];
        bool already_written = false;
        int written;
        if ((!dependencies[i].payload_data && dependencies[i].content_bytes > 0u) ||
            core_scene_compile_sha256(dependencies[i].payload_data,
                                      dependencies[i].content_bytes,
                                      actual_sha256).code != CORE_OK ||
            strcmp(actual_sha256, dependencies[i].content_sha256) != 0 ||
            core_scene_compile_dependency_payload_path(dependencies[i].kind,
                                                       dependencies[i].content_sha256,
                                                       relative_path).code != CORE_OK) {
            payload_diag(diagnostics, diagnostics_size, "dependency payload bytes do not match manifest metadata");
            return (CoreResult){ CORE_ERR_FORMAT, "dependency payload mismatch" };
        }
        for (size_t j = 0u; j < i; ++j) {
            if (same_payload_path(&dependencies[i], &dependencies[j])) {
                already_written = true;
                break;
            }
        }
        if (already_written) continue;
        written = snprintf(kind_relative, sizeof(kind_relative), "dependencies/%s", dependencies[i].kind);
        if (written <= 0 || (size_t)written >= sizeof(kind_relative) ||
            !join_path(staging_dir, kind_relative, kind_dir, sizeof(kind_dir)) ||
            (!join_path(staging_dir, relative_path, payload_path, sizeof(payload_path)))) {
            return (CoreResult){ CORE_ERR_INVALID_ARG, "dependency payload path too long" };
        }
        if (mkdir(kind_dir, 0755) != 0 && errno != EEXIST) {
            payload_diag(diagnostics, diagnostics_size, "failed to create dependency kind directory");
            return (CoreResult){ CORE_ERR_IO, "failed to create dependency kind directory" };
        }
        if (!write_payload_create_only(payload_path,
                                       dependencies[i].payload_data,
                                       dependencies[i].content_bytes) ||
            !sync_directory_path(kind_dir)) {
            payload_diag(diagnostics, diagnostics_size, "failed to durably write dependency payload");
            return (CoreResult){ CORE_ERR_IO, "failed to write dependency payload" };
        }
    }
    if (!sync_directory_path(dependencies_dir)) {
        payload_diag(diagnostics, diagnostics_size, "failed to sync dependency payload directory");
        return (CoreResult){ CORE_ERR_IO, "failed to sync dependency payload directory" };
    }
    return core_result_ok();
}

void core_scene_compile_dependency_payloads_cleanup(
    const char *staging_dir,
    const CoreSceneCompileDependency *dependencies,
    size_t dependency_count) {
    char dependencies_dir[1024];
    if (!staging_dir || !dependencies || dependency_count == 0u) return;
    for (size_t i = 0u; i < dependency_count; ++i) {
        char relative_path[CORE_SCENE_COMPILE_DEPENDENCY_PATH_SIZE];
        char payload_path[1024];
        if (core_scene_compile_dependency_payload_path(dependencies[i].kind,
                                                       dependencies[i].content_sha256,
                                                       relative_path).code == CORE_OK &&
            join_path(staging_dir, relative_path, payload_path, sizeof(payload_path))) {
            (void)unlink(payload_path);
        }
    }
    for (size_t i = 0u; i < dependency_count; ++i) {
        char kind_relative[96];
        char kind_dir[1024];
        int written = snprintf(kind_relative, sizeof(kind_relative), "dependencies/%s", dependencies[i].kind);
        if (written > 0 && (size_t)written < sizeof(kind_relative) &&
            join_path(staging_dir, kind_relative, kind_dir, sizeof(kind_dir))) {
            (void)rmdir(kind_dir);
        }
    }
    if (join_path(staging_dir, "dependencies", dependencies_dir, sizeof(dependencies_dir))) {
        (void)rmdir(dependencies_dir);
    }
}
