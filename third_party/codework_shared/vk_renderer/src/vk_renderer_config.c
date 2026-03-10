#include "vk_renderer_config.h"

#ifndef NDEBUG
#define VK_RENDERER_VALIDATION_DEFAULT VK_TRUE
#else
#define VK_RENDERER_VALIDATION_DEFAULT VK_FALSE
#endif

void vk_renderer_config_set_defaults(VkRendererConfig* config) {
    if (!config) return;
    config->enable_validation = VK_RENDERER_VALIDATION_DEFAULT;
    config->prefer_discrete_gpu = VK_TRUE;
    config->msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    config->frames_in_flight = 2;
    config->extra_instance_extensions = NULL;
    config->extra_instance_extension_count = 0;
    config->extra_device_extensions = NULL;
    config->extra_device_extension_count = 0;
    config->clear_color[0] = 0.0f;
    config->clear_color[1] = 0.0f;
    config->clear_color[2] = 0.0f;
    config->clear_color[3] = 1.0f;
}
