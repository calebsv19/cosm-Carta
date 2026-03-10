/*
 * shape_library.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "geo/shape_library.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *join_path(const char *dir, const char *file) {
    size_t len_dir = strlen(dir);
    size_t len_file = strlen(file);
    bool needs_slash = (len_dir > 0 && dir[len_dir - 1] != '/');
    size_t total = len_dir + needs_slash + len_file + 1;
    char *buf = (char *)malloc(total);
    if (!buf) return NULL;
    memcpy(buf, dir, len_dir);
    if (needs_slash) buf[len_dir] = '/';
    memcpy(buf + len_dir + (needs_slash ? 1 : 0), file, len_file + 1);
    return buf;
}

void shape_library_free(ShapeAssetLibrary *lib) {
    if (!lib) return;
    if (lib->assets) {
        for (size_t i = 0; i < lib->count; ++i) {
            shape_asset_free(&lib->assets[i]);
        }
        free(lib->assets);
        lib->assets = NULL;
        lib->count = 0;
    }
}

static bool has_asset_ext(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcmp(dot, ".json") == 0) || (strcmp(dot, ".asset.json") == 0);
}

bool shape_library_load_dir(const char *dir_path, ShapeAssetLibrary *out_lib) {
    if (!dir_path || !out_lib) return false;
    memset(out_lib, 0, sizeof(*out_lib));

    DIR *dir = opendir(dir_path);
    if (!dir) return false;

    ShapeAsset *assets = NULL;
    size_t count = 0;
    size_t capacity = 0;

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (!has_asset_ext(ent->d_name)) continue;
        char *path = join_path(dir_path, ent->d_name);
        if (!path) continue;

        ShapeAsset asset = {0};
        if (shape_asset_load_file(path, &asset)) {
            if (count >= capacity) {
                size_t new_cap = capacity ? capacity * 2 : 8;
                ShapeAsset *tmp = (ShapeAsset *)realloc(assets, new_cap * sizeof(ShapeAsset));
                if (!tmp) {
                    shape_asset_free(&asset);
                    free(path);
                    continue;
                }
                assets = tmp;
                capacity = new_cap;
            }
            assets[count++] = asset;
            fprintf(stdout, "[shape] Loaded %s (paths=%zu)\n", path, asset.path_count);
        } else {
            fprintf(stderr, "[shape] Failed to load %s\n", path);
        }
        free(path);
    }
    closedir(dir);

    if (count == 0) {
        free(assets);
        return false;
    }
    out_lib->assets = assets;
    out_lib->count = count;
    return true;
}

const ShapeAsset *shape_library_get(const ShapeAssetLibrary *lib, size_t index) {
    if (!lib || index >= lib->count) return NULL;
    return &lib->assets[index];
}

const ShapeAsset *shape_library_find(const ShapeAssetLibrary *lib, const char *name) {
    if (!lib || !name) return NULL;
    for (size_t i = 0; i < lib->count; ++i) {
        const ShapeAsset *a = &lib->assets[i];
        if (a->name && strcmp(a->name, name) == 0) {
            return a;
        }
    }
    return NULL;
}
