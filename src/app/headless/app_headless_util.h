#ifndef MAPFORGE_APP_HEADLESS_UTIL_H
#define MAPFORGE_APP_HEADLESS_UTIL_H

#include <json-c/json.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

bool map_forge_headless_fail(char *out_error, size_t out_error_size, const char *message);
void map_forge_headless_diag_set(char *out, size_t out_size, const char *message);
bool map_forge_headless_copy_string(char *dst, size_t dst_size, const char *src);
bool map_forge_headless_ensure_dir_recursive_mode(const char *path, mode_t mode);
bool map_forge_headless_ensure_parent_dir_mode(const char *path, mode_t mode);
bool map_forge_headless_parent_dir(const char *path, char *out_dir, size_t out_size);
bool map_forge_headless_realpath_or_copy(const char *path, char *out_path, size_t out_size);
bool map_forge_headless_resolve_relative(const char *base_dir, const char *path, char *out_path, size_t out_size);

void map_forge_headless_json_write_string(FILE *file, const char *value);
bool map_forge_headless_json_get_object(json_object *owner, const char *key, json_object **out_obj);
bool map_forge_headless_json_get_string_ref(json_object *owner, const char *key, const char **out_value);
bool map_forge_headless_json_get_required_string(json_object *obj,
                                                 const char *key,
                                                 char *out_value,
                                                 size_t out_size,
                                                 char *out_error,
                                                 size_t out_error_size);
bool map_forge_headless_json_get_required_u32(json_object *obj,
                                              const char *key,
                                              uint32_t *out_value,
                                              char *out_error,
                                              size_t out_error_size);
bool map_forge_headless_json_get_optional_string(json_object *obj,
                                                 const char *key,
                                                 char *out_value,
                                                 size_t out_size);
bool map_forge_headless_json_get_optional_bool(json_object *obj, const char *key, bool *out_value);
bool map_forge_headless_json_get_optional_int(json_object *obj, const char *key, int *out_value);
bool map_forge_headless_json_get_optional_float(json_object *obj, const char *key, float *out_value);

#endif
