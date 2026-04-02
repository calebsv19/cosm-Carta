#ifndef CORE_SCENE_COMPILE_H
#define CORE_SCENE_COMPILE_H

#include <stddef.h>

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

CoreResult core_scene_compile_authoring_to_runtime(const char *authoring_json,
                                                   char **out_runtime_json,
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
