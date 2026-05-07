#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "mapforge_graph_internal.h"

bool ensure_dir_recursive(const char *path);
bool remove_tree(const char *path);
bool build_publish_paths(const char *active_root,
                         char *out_stage_root,
                         size_t stage_size,
                         char *out_snapshot_root,
                         size_t snapshot_size,
                         char *out_staging_root,
                         size_t staging_root_size);
bool validate_staged_graph(const char *stage_root);
bool publish_staged_graph(const GraphOptions *options,
                          const char *stage_root,
                          const char *active_root,
                          const char *snapshot_root);
bool write_graph(const GraphBuild *build, const char *out_dir);
void prune_staging_dirs(const char *staging_root, uint32_t prune_days, bool dry_run);
