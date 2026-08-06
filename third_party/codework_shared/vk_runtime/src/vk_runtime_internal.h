#ifndef CODEWORK_VK_RUNTIME_INTERNAL_H
#define CODEWORK_VK_RUNTIME_INTERNAL_H

#include "vk_runtime.h"

void vk_runtime_internal_copy_text(char *destination,
                                   size_t destination_capacity,
                                   const char *source);

VkRuntimeStatus vk_runtime_internal_set_failure(
    VkRuntimeCapabilityReport *report,
    VkRuntimeStatus status,
    VkResult result);

VkRuntimeStatus vk_runtime_internal_create_instance(
    VkRuntime *runtime,
    const VkRuntimeConfig *config);

void vk_runtime_internal_close_instance(VkRuntime *runtime);

#define copy_text vk_runtime_internal_copy_text
#define set_failure vk_runtime_internal_set_failure

#endif
