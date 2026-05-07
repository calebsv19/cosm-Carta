#include "mapforge_graph_source.h"

#include "core/log.h"
#include "mapforge_graph_internal.h"

#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static bool source_name_has_suffix(const char *name, const char *suffix) {
    size_t name_len = 0u;
    size_t suffix_len = 0u;
    if (!name || !suffix) {
        return false;
    }
    name_len = strlen(name);
    suffix_len = strlen(suffix);
    if (name_len < suffix_len) {
        return false;
    }
    return strcasecmp(name + (name_len - suffix_len), suffix) == 0;
}

static bool source_buffer_contains(const uint8_t *buffer,
                                   size_t buffer_size,
                                   const char *needle) {
    size_t needle_len = 0u;
    if (!buffer || !needle) {
        return false;
    }
    needle_len = strlen(needle);
    if (needle_len == 0u || needle_len > buffer_size) {
        return false;
    }
    for (size_t i = 0; i + needle_len <= buffer_size; ++i) {
        if (memcmp(buffer + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

OSMSourceKind detect_osm_source_kind(const char *path) {
    uint8_t probe[4096];
    size_t read_size = 0u;
    FILE *file = NULL;

    if (!path || path[0] == '\0') {
        return OSM_SOURCE_KIND_UNKNOWN;
    }
    if (source_name_has_suffix(path, ".osm") || source_name_has_suffix(path, ".osm.xml")) {
        return OSM_SOURCE_KIND_XML;
    }
    if (source_name_has_suffix(path, ".pbf") || source_name_has_suffix(path, ".osm.pbf")) {
        return OSM_SOURCE_KIND_PBF;
    }

    file = fopen(path, "rb");
    if (!file) {
        return OSM_SOURCE_KIND_UNKNOWN;
    }
    read_size = fread(probe, 1u, sizeof(probe), file);
    fclose(file);

    if (read_size == 0u) {
        return OSM_SOURCE_KIND_UNKNOWN;
    }
    if (source_buffer_contains(probe, read_size, "OSMHeader")) {
        return OSM_SOURCE_KIND_PBF;
    }
    if (source_buffer_contains(probe, read_size, "<osm") ||
        source_buffer_contains(probe, read_size, "<?xml")) {
        return OSM_SOURCE_KIND_XML;
    }
    return OSM_SOURCE_KIND_UNKNOWN;
}

static int source_spawn_and_wait(const char *program, char *const argv[]) {
    pid_t pid = 0;
    int rc = 0;
    int status = 0;

    if (!program || !argv || !argv[0]) {
        return EINVAL;
    }

    if (strchr(program, '/') != NULL) {
        rc = posix_spawn(&pid, program, NULL, NULL, argv, environ);
    } else {
        rc = posix_spawnp(&pid, program, NULL, NULL, argv, environ);
    }
    if (rc != 0) {
        return rc;
    }

    for (;;) {
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) {
            break;
        }
        if (waited < 0 && errno == EINTR) {
            continue;
        }
        return errno != 0 ? errno : ECHILD;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return EPROTO;
    }
    return 0;
}

static bool resolve_osmium_program(char *out_path, size_t out_path_cap) {
    const char *env_path = getenv("MAPFORGE_OSMIUM_PATH");
    const char *candidates[] = {
        "/opt/homebrew/bin/osmium",
        "/usr/local/bin/osmium",
        "/usr/bin/osmium",
        "osmium"
    };
    if (!out_path || out_path_cap == 0u) {
        return false;
    }
    out_path[0] = '\0';

    if (env_path && env_path[0] != '\0') {
        if (access(env_path, X_OK) == 0) {
            snprintf(out_path, out_path_cap, "%s", env_path);
            return true;
        }
        log_error("MAPFORGE_OSMIUM_PATH is set but not executable: %s", env_path);
    }

    for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
        const char *candidate = candidates[i];
        if (candidate[0] == '/') {
            if (access(candidate, X_OK) != 0) {
                continue;
            }
        }
        snprintf(out_path, out_path_cap, "%s", candidate);
        return true;
    }
    return false;
}

bool convert_pbf_to_xml(const char *pbf_path, char *xml_path, size_t xml_path_cap) {
    char tmp_template[] = "/tmp/mapforge_osmxml_XXXXXX";
    char osmium_program[MAPFORGE_SOURCE_PATH_CAPACITY];
    int fd = -1;
    int rc = 0;
    char *osmium_argv[] = {NULL, "cat", "-F", "pbf", (char *)pbf_path, "-o", tmp_template, "-f", "osm", "--overwrite", NULL};

    if (!pbf_path || !xml_path || xml_path_cap == 0u) {
        return false;
    }
    xml_path[0] = '\0';

    if (!resolve_osmium_program(osmium_program, sizeof(osmium_program))) {
        log_error("PBF source detected (%s) but converter is unavailable. Install `osmium` or set MAPFORGE_OSMIUM_PATH to the osmium binary.", pbf_path);
        return false;
    }
    osmium_argv[0] = osmium_program;

    fd = mkstemp(tmp_template);
    if (fd < 0) {
        log_error("Failed to create temporary XML path for PBF conversion: %s", strerror(errno));
        return false;
    }
    close(fd);

    rc = source_spawn_and_wait(osmium_program, osmium_argv);
    if (rc == 0) {
        snprintf(xml_path, xml_path_cap, "%s", tmp_template);
        return true;
    }

    (void)unlink(tmp_template);
    if (rc == ENOENT) {
        log_error("PBF source detected (%s) but converter is unavailable. Install `osmium` or set MAPFORGE_OSMIUM_PATH to the osmium binary.", pbf_path);
    } else {
        log_error("PBF conversion failed for %s (converter=%s, rc=%d)", pbf_path, osmium_program, rc);
    }
    return false;
}
