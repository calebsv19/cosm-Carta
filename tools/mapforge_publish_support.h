#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool mapforge_publish_path_exists(const char *path);
bool mapforge_publish_path_is_dir(const char *path);
bool mapforge_publish_ensure_dir_recursive(const char *path);
bool mapforge_publish_remove_tree(const char *path);
const char *mapforge_publish_root_reject_reason(const char *path);
bool mapforge_publish_root_is_safe(const char *path);
bool mapforge_publish_split_parent_name(const char *path,
                                        char *out_parent,
                                        size_t parent_size,
                                        char *out_name,
                                        size_t name_size);
bool mapforge_publish_build_paths(const char *active_root,
                                  const char *staging_dir_name,
                                  const char *snapshot_dir_name,
                                  char *out_stage_root,
                                  size_t stage_size,
                                  char *out_snapshot_root,
                                  size_t snapshot_size,
                                  char *out_staging_root,
                                  size_t staging_root_size);
void mapforge_publish_prune_snapshot_dir(const char *snapshot_root,
                                         uint32_t keep_old,
                                         uint32_t prune_days,
                                         bool dry_run,
                                         const char *dry_run_format,
                                         const char *failure_format,
                                         const char *success_format);
void mapforge_publish_prune_staging_dirs(const char *staging_root,
                                         uint32_t prune_days,
                                         bool dry_run,
                                         const char *dry_run_format,
                                         const char *failure_format,
                                         const char *success_format);
