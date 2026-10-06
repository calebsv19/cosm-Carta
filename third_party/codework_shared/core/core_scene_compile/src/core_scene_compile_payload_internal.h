#ifndef CORE_SCENE_COMPILE_PAYLOAD_INTERNAL_H
#define CORE_SCENE_COMPILE_PAYLOAD_INTERNAL_H

#include "core_scene_compile.h"

CoreResult core_scene_compile_dependency_payloads_publish(
    const char *staging_dir,
    const CoreSceneCompileDependency *dependencies,
    size_t dependency_count,
    char *diagnostics,
    size_t diagnostics_size);

void core_scene_compile_dependency_payloads_cleanup(
    const char *staging_dir,
    const CoreSceneCompileDependency *dependencies,
    size_t dependency_count);

#endif
