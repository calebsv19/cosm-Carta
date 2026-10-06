#include "map/terrain_catalog.h"
#include "core_io.h"
#include <json-c/json.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *const files[] = {"terrain.sqlite", "terrain.json", "terrain_vectors.sqlite",
                                  "terrain_vectors.json", "source.json"};
// Returns a field without changing or coercing its containing manifest.
static struct json_object *field(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    if (object) json_object_object_get_ex(object, key, &value);
    return value;
}
// Copies bounded typed strings without accepting truncated identifiers or paths.
static bool text(struct json_object *object, const char *key, char *out, size_t capacity) {
    struct json_object *value = field(object, key);
    if (!json_object_is_type(value, json_type_string)) return false;
    const char *s = json_object_get_string(value);
    size_t n = json_object_get_string_len(value);
    if (!n || n >= capacity || strlen(s) != n) return false;
    memcpy(out, s, n + 1);
    return true;
}
// Accepts positive integral byte counts without JSON numeric coercion.
static size_t bytes(struct json_object *object, const char *key) {
    struct json_object *value = field(object, key);
    if (!json_object_is_type(value, json_type_int)) return 0;
    int64_t n = json_object_get_int64(value);
    return n > 0 && n <= 256LL * 1024 * 1024 ? (size_t)n : 0;
}
// Restricts owned directory IDs to a single safe ASCII path component.
static bool identifier(const char *id) {
    if (!id || !*id || strlen(id) >= 64 || id[0] == '-') return false;
    for (const char *s = id; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') || *s == '-')) return false;
    return true;
}
// Reads one small regular non-symlink JSON document and fingerprints its exact bytes.
static struct json_object *read_json(const char *path, uint64_t *token) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 2 || st.st_size > 256 * 1024) {
        close(fd); return NULL;
    }
    char *data = calloc((size_t)st.st_size + 1, 1);
    if (!data) { close(fd); return NULL; }
    size_t used = 0;
    while (used < (size_t)st.st_size) {
        ssize_t n = read(fd, data + used, (size_t)st.st_size - used);
        if (n <= 0) { free(data); close(fd); return NULL; }
        used += (size_t)n;
    }
    close(fd);
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < used; i++) { hash ^= (unsigned char)data[i]; hash *= UINT64_C(1099511628211); }
    struct json_tokener *parser = json_tokener_new();
    struct json_object *object = parser ? json_tokener_parse_ex(parser, data, (int)used) : NULL;
    bool ok = parser && json_tokener_get_error(parser) == json_tokener_success &&
              json_object_is_type(object, json_type_object);
    size_t end = parser ? json_tokener_get_parse_end(parser) : 0;
    for (size_t i = end; ok && i < used; i++)
        if (data[i] != ' ' && data[i] != '\n' && data[i] != '\r' && data[i] != '\t') ok = false;
    if (parser) json_tokener_free(parser);
    free(data);
    if (!ok) { if (object) json_object_put(object); return NULL; }
    if (token) *token = hash;
    return object;
}
// Checks metric extents without allowing reversed, non-finite or out-of-world coverage.
static bool extent(struct json_object *object, double out[4]) {
    struct json_object *value = field(object, "extent");
    if (!json_object_is_type(value, json_type_array) || json_object_array_length(value) != 4) return false;
    for (int i = 0; i < 4; i++) {
        struct json_object *v = json_object_array_get_idx(value, i);
        if (!json_object_is_type(v, json_type_double) && !json_object_is_type(v, json_type_int)) return false;
        out[i] = json_object_get_double(v);
        if (!isfinite(out[i]) || fabs(out[i]) > 20037509) return false;
    }
    return out[0] < out[2] && out[1] < out[3];
}
// Validates all and only the five allowed files inside an opened owned patch directory.
static bool patch_files(int root_fd, const char *id, struct json_object *sizes, size_t *total, size_t *overhead) {
    int fd = openat(root_fd, id, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (fd < 0) return false;
    bool ok = json_object_is_type(sizes, json_type_object) && json_object_object_length(sizes) == 5;
    size_t sum = 0, extra = 0;
    for (size_t i = 0; ok && i < 5; i++) {
        struct stat st;
        size_t expected = bytes(sizes, files[i]);
        ok = expected && !fstatat(fd, files[i], &st, AT_SYMLINK_NOFOLLOW) &&
             S_ISREG(st.st_mode) && (uint64_t)st.st_size == expected;
        if (ok) sum += expected;
    }
    DIR *dir = fdopendir(dup(fd));
    if (!dir) ok = false;
    struct dirent *entry;
    while (ok && (entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (!strcmp(entry->d_name,".DS_Store")) {
            struct stat metadata;
            if (fstatat(fd,entry->d_name,&metadata,AT_SYMLINK_NOFOLLOW) || !S_ISREG(metadata.st_mode) || metadata.st_size>1024*1024) ok=false;
            else extra+=(size_t)metadata.st_size;
            continue;
        }
        bool known = false;
        for (size_t i = 0; i < 5; i++) if (!strcmp(entry->d_name, files[i])) known = true;
        if (!known) ok = false;
    }
    if (dir) closedir(dir);
    close(fd);
    *total = sum;
    if(overhead)*overhead=extra;
    return ok && sum+extra <= TERRAIN_AREA_BUDGET;
}
// Discovers the unchanged baseline from its source receipt rather than fixed Seattle coordinates.
static bool baseline(TerrainCatalog *next) {
    char path[8192];
    snprintf(path, sizeof(path), "%s/hiking_terrain_overview/terrain.json", next->region_root);
    struct json_object *manifest = read_json(path, NULL);
    struct json_object *styles = field(manifest, "styles");
    bool ok = json_object_is_type(styles, json_type_array) && json_object_array_length(styles) >= 2;
    TerrainArea *area = &next->areas[0];
    if (ok) ok = extent(json_object_array_get_idx(styles, 0), area->extent);
    snprintf(area->id, sizeof(area->id), "seattle-cascades");
    snprintf(area->name, sizeof(area->name), "Seattle–Cascades overview");
    snprintf(area->source, sizeof(area->source), "USGS 3DEP + GNIS / The National Map");
    area->resolution_m = 45;
    int a = snprintf(area->raster_path, sizeof(area->raster_path), "%s/hiking_terrain_overview/terrain.sqlite", next->region_root);
    int b = snprintf(area->vector_path, sizeof(area->vector_path), "%s/hiking_terrain_overview/terrain_vectors.sqlite", next->region_root);
    ok = ok && a > 0 && a < (int)sizeof(area->raster_path) && b > 0 && b < (int)sizeof(area->vector_path);
    struct stat st;
    if (ok && !stat(area->raster_path, &st)) area->bytes = (size_t)st.st_size;
    else ok = false;
    if (ok && !stat(area->vector_path, &st)) area->bytes += (size_t)st.st_size;
    if (manifest) json_object_put(manifest);
    next->count = ok ? 1 : 0;
    return ok;
}
bool terrain_catalog_load(TerrainCatalog *catalog, const char *region_root, const char *storage_root,
                          bool base_only, char *error, size_t cap) {
    TerrainCatalog *next = calloc(1, sizeof(*next));
    struct json_object *manifest = NULL;
    int root_fd = -1;
    const char *reason = "Terrain catalog invalid; existing areas kept";
    if (!next || !catalog || !region_root || !storage_root || strlen(region_root) >= 4000 || strlen(storage_root) >= 4000) goto fail;
    snprintf(next->region_root, sizeof(next->region_root), "%s", region_root);
    snprintf(next->storage_root, sizeof(next->storage_root), "%s", storage_root);
    baseline(next); // Missing baseline is allowed when only a distant patch is installed.
    char path[8192];
    snprintf(path, sizeof(path), "%s/catalog.json", storage_root);
    struct stat st;
    if (base_only || (lstat(path, &st) && errno == ENOENT)) goto success;
    manifest = read_json(path, &next->token);
    if (!manifest || bytes(manifest, "schema") != 1 || bytes(manifest, "additional_budget_bytes") != TERRAIN_ADDITIONAL_BUDGET) goto fail;
    struct json_object *areas = field(manifest, "areas");
    if (!json_object_is_type(areas, json_type_array) || json_object_array_length(areas) > 8) goto fail;
    root_fd = open(storage_root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (root_fd < 0) goto fail;
    next->additional_bytes = (size_t)st.st_size;
    for (size_t i = 0; i < json_object_array_length(areas); i++) {
        struct json_object *entry = json_object_array_get_idx(areas, i);
        TerrainArea *area = &next->areas[next->count];
        if (!text(entry, "id", area->id, sizeof(area->id)) || !identifier(area->id) ||
            !text(entry, "name", area->name, sizeof(area->name)) ||
            !text(entry, "source", area->source, sizeof(area->source)) || !extent(entry, area->extent)) goto fail;
        for (size_t j = 0; j < next->count; j++) if (!strcmp(area->id, next->areas[j].id)) goto fail;
        struct json_object *resolution = field(entry, "resolution_m");
        if (!json_object_is_type(resolution, json_type_double) && !json_object_is_type(resolution, json_type_int)) goto fail;
        area->resolution_m = json_object_get_double(resolution);
        if (!isfinite(area->resolution_m) || area->resolution_m < 1 || area->resolution_m > 1000) goto fail;
        char raster[128], vector[128], expected[128];
        if (!text(entry, "raster_path", raster, sizeof(raster)) || !text(entry, "vector_path", vector, sizeof(vector))) goto fail;
        snprintf(expected, sizeof(expected), "%s/terrain.sqlite", area->id);
        if (strcmp(raster, expected)) goto fail;
        snprintf(expected, sizeof(expected), "%s/terrain_vectors.sqlite", area->id);
        if (strcmp(vector, expected)) goto fail;
        size_t overhead=0;
        if (!patch_files(root_fd, area->id, field(entry, "files"), &area->bytes, &overhead) || area->bytes != bytes(entry, "bytes")) goto fail;
        next->additional_bytes += area->bytes+overhead;
        if (next->additional_bytes > TERRAIN_ADDITIONAL_BUDGET) goto fail;
        snprintf(area->raster_path, sizeof(area->raster_path), "%s/%s", storage_root, raster);
        snprintf(area->vector_path, sizeof(area->vector_path), "%s/%s", storage_root, vector);
        area->removable = true;
        next->count++;
    }
    // Unindexed payloads cannot silently evade the cache budget or ownership boundary.
    DIR *directory = fdopendir(dup(root_fd));
    if (!directory) goto fail;
    bool known = true;
    struct dirent *item;
    while ((item = readdir(directory))) {
        if (!strcmp(item->d_name,".") || !strcmp(item->d_name,"..") || !strcmp(item->d_name,"catalog.json")) continue;
        if(!strcmp(item->d_name,".DS_Store")) {
            struct stat metadata;
            if(fstatat(root_fd,item->d_name,&metadata,AT_SYMLINK_NOFOLLOW) || !S_ISREG(metadata.st_mode) || metadata.st_size>1024*1024)known=false;
            else next->additional_bytes+=(size_t)metadata.st_size;
            continue;
        }
        bool listed = false;
        for (size_t i = 0; i < next->count; i++)
            if (next->areas[i].removable && !strcmp(item->d_name,next->areas[i].id)) listed = true;
        if (!listed) known = false;
    }
    closedir(directory);
    if(next->additional_bytes>TERRAIN_ADDITIONAL_BUDGET)goto fail;
    if (!known) { reason="Terrain cache has unindexed files; inspect before adding areas"; goto fail; }
success:
    if (root_fd >= 0) close(root_fd);
    if (manifest) json_object_put(manifest);
    *catalog = *next;
    free(next);
    if (error && cap) error[0] = 0;
    return true;
fail:
    if (root_fd >= 0) close(root_fd);
    if (manifest) json_object_put(manifest);
    free(next);
    if (error && cap) snprintf(error, cap, "%s", reason);
    return false;
}
int terrain_catalog_select(const TerrainCatalog *catalog, double xmin, double ymin, double xmax, double ymax) {
    if (!catalog || !isfinite(xmin) || !isfinite(ymin) || !isfinite(xmax) || !isfinite(ymax) || xmin >= xmax || ymin >= ymax) return -1;
    int best = -1;
    double overlap = 0;
    for (size_t i = 0; i < catalog->count; i++) {
        const TerrainArea *area = &catalog->areas[i];
        const double *e = area->extent;
        double w = fmin(xmax, e[2]) - fmax(xmin, e[0]), h = fmin(ymax, e[3]) - fmax(ymin, e[1]);
        if (w <= 0 || h <= 0) continue;
        double score = w * h;
        if (best < 0 || score > overlap * 1.001 ||
            (score >= overlap * .999 && area->resolution_m < catalog->areas[best].resolution_m)) {
            best = (int)i; overlap = score;
        }
    }
    return best;
}
bool terrain_catalog_remove(TerrainCatalog *catalog, size_t index, char *error, size_t cap) {
    const char *reason = "Removal refused; reload terrain areas and retry";
    struct json_object *manifest = NULL, *kept = NULL;
    int root_fd = -1, dir_fd = -1;
    bool renamed = false, published = false, ok = false;
    char path[8192], retiring[96], id[64];
    if (!catalog || index >= catalog->count || !catalog->areas[index].removable) goto done;
    snprintf(id, sizeof(id), "%s", catalog->areas[index].id);
    snprintf(retiring, sizeof(retiring), ".removing-%s", id);
    snprintf(path, sizeof(path), "%s/catalog.json", catalog->storage_root);
    uint64_t token = 0;
    manifest = read_json(path, &token);
    if (!manifest || token != catalog->token) { reason = "Terrain areas changed; Reload before removal"; goto done; }
    root_fd = open(catalog->storage_root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (root_fd < 0) goto done;
    struct json_object *areas = field(manifest, "areas"), *target = NULL;
    kept = json_object_new_array();
    for (size_t i = 0; areas && i < json_object_array_length(areas); i++) {
        struct json_object *entry = json_object_array_get_idx(areas, i);
        const char *name = json_object_get_string(field(entry, "id"));
        if (name && !strcmp(name, id)) target = entry;
        else json_object_array_add(kept, json_object_get(entry));
    }
    size_t actual = 0;
    if (!target || !patch_files(root_fd, id, field(target, "files"), &actual, NULL) || actual != catalog->areas[index].bytes) goto done;
    dir_fd = openat(root_fd, id, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    struct stat st;
    if (dir_fd < 0 || !fstatat(root_fd, retiring, &st, AT_SYMLINK_NOFOLLOW) || errno != ENOENT) goto done;
    if (renameat(root_fd, id, root_fd, retiring)) goto done;
    renamed = true;
    json_object_object_add(manifest, "areas", kept); kept = NULL;
    const char *serialized = json_object_to_json_string_ext(manifest, JSON_C_TO_STRING_PRETTY);
    CoreResult result = core_io_write_all_atomic(path, serialized, strlen(serialized));
    if (result.code != CORE_OK) goto done;
    published = true;
    for (size_t i = 0; i < 5; i++) if (unlinkat(dir_fd, files[i], 0)) { reason = "Area detached; some cache files remain for cleanup"; goto done; }
    if(unlinkat(dir_fd,".DS_Store",0) && errno!=ENOENT){reason="Area detached; Finder metadata remains for cleanup";goto done;}
    if (unlinkat(root_fd, retiring, AT_REMOVEDIR)) { reason = "Area detached; empty cache directory remains"; goto done; }
    ok = terrain_catalog_load(catalog, catalog->region_root, catalog->storage_root, false, error, cap);
done:
    if (renamed && !published) renameat(root_fd, retiring, root_fd, id);
    if (dir_fd >= 0) close(dir_fd);
    if (root_fd >= 0) close(root_fd);
    if (kept) json_object_put(kept);
    if (manifest) json_object_put(manifest);
    if (!ok && error && cap) snprintf(error, cap, "%s", reason);
    return ok;
}
