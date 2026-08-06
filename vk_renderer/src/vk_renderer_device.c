#include "vk_renderer_device.h"

#include <SDL2/SDL_vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VkResult runtime_result(const VkRendererDevice* device,
                               VkRuntimeStatus status) {
    VkResult result = device->runtime.report.vulkan_result;
    if (result == VK_SUCCESS && status != VK_RUNTIME_STATUS_OK) {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }
    return result;
}

static VkResult create_pipeline_cache(VkRendererDevice* device) {
    VkPipelineCacheCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };
    return vkCreatePipelineCache(device->device, &info, NULL,
                                 &device->pipeline_cache);
}

static void mirror_runtime_handles(VkRendererDevice* device) {
    const VkRuntimeCapabilityReport* report =
        vk_runtime_get_capability_report(&device->runtime);
    const VkRuntimeDeviceCapability* capability = NULL;
    if (report && report->selected_device_index < report->device_count) {
        capability = &report->devices[report->selected_device_index];
    }

    device->instance = device->runtime.instance;
    device->debug_messenger = device->runtime.debug_messenger;
    device->physical_device = device->runtime.physical_device;
    device->device = device->runtime.device;
    device->graphics_queue = device->runtime.graphics_queue;
    device->present_queue = device->runtime.present_queue;
    device->validation_enabled =
        report && report->validation_enabled ? VK_TRUE : VK_FALSE;
    if (capability) {
        device->graphics_queue_family = capability->graphics_queue_family;
        device->present_queue_family = capability->present_queue_family;
    }
    if (device->physical_device != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceProperties(device->physical_device,
                                      &device->physical_device_properties);
        vkGetPhysicalDeviceMemoryProperties(device->physical_device,
                                            &device->memory_properties);
    }
}

VkResult vk_renderer_device_init(VkRendererDevice* device,
                                 SDL_Window* window,
                                 const VkRendererConfig* config) {
    if (!device || !window || !config) return VK_ERROR_INITIALIZATION_FAILED;
    memset(device, 0, sizeof(*device));

    unsigned int sdl_extension_count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_extension_count, NULL)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    uint32_t instance_extension_count =
        (uint32_t)sdl_extension_count + config->extra_instance_extension_count;
    const char** instance_extensions =
        (const char**)calloc(instance_extension_count, sizeof(*instance_extensions));
    if (!instance_extensions) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_extension_count,
                                           instance_extensions)) {
        free(instance_extensions);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    for (uint32_t i = 0; i < config->extra_instance_extension_count; ++i) {
        instance_extensions[sdl_extension_count + i] =
            config->extra_instance_extensions[i];
    }

    const uint32_t device_extension_count =
        1u + config->extra_device_extension_count;
    const char** device_extensions =
        (const char**)calloc(device_extension_count, sizeof(*device_extensions));
    if (!device_extensions) {
        free(instance_extensions);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    device_extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    for (uint32_t i = 0; i < config->extra_device_extension_count; ++i) {
        device_extensions[i + 1u] = config->extra_device_extensions[i];
    }

    VkRuntimeConfig runtime_config;
    vk_runtime_config_defaults(&runtime_config);
    runtime_config.application_name = "CodeWork vk_renderer";
    runtime_config.application_version = VK_MAKE_API_VERSION(0, 1, 3, 0);
    runtime_config.enable_validation = config->enable_validation == VK_TRUE;
    runtime_config.require_validation = false;
    runtime_config.require_graphics_queue = true;
    runtime_config.require_compute_queue = false;
    runtime_config.require_transfer_queue = false;
    runtime_config.require_present_queue = true;
    runtime_config.prefer_discrete_gpu = config->prefer_discrete_gpu == VK_TRUE;
    runtime_config.instance_extensions = instance_extensions;
    runtime_config.instance_extension_count = instance_extension_count;
    runtime_config.device_extensions = device_extensions;
    runtime_config.device_extension_count = device_extension_count;

    VkRuntimeStatus status =
        vk_runtime_initialize_instance(&device->runtime, &runtime_config);
    if (status != VK_RUNTIME_STATUS_OK) {
        VkResult result = runtime_result(device, status);
        free(device_extensions);
        free(instance_extensions);
        vk_renderer_device_shutdown(device);
        return result;
    }
    mirror_runtime_handles(device);

    if (!SDL_Vulkan_CreateSurface(window, device->instance,
                                  &device->pending_surface)) {
        free(device_extensions);
        free(instance_extensions);
        vk_renderer_device_shutdown(device);
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    device->pending_surface_window = window;

    status = vk_runtime_initialize_device(&device->runtime, &runtime_config,
                                          device->pending_surface);
    free(device_extensions);
    free(instance_extensions);
    if (status != VK_RUNTIME_STATUS_OK) {
        VkResult result = runtime_result(device, status);
        vk_renderer_device_shutdown(device);
        return result;
    }
    mirror_runtime_handles(device);

    VkResult result = create_pipeline_cache(device);
    if (result != VK_SUCCESS) {
        vk_renderer_device_shutdown(device);
        return result;
    }
    return VK_SUCCESS;
}

VkSurfaceKHR vk_renderer_device_take_surface(VkRendererDevice* device,
                                             SDL_Window* window) {
    if (!device || device->pending_surface_window != window) {
        return VK_NULL_HANDLE;
    }
    VkSurfaceKHR surface = device->pending_surface;
    device->pending_surface = VK_NULL_HANDLE;
    device->pending_surface_window = NULL;
    return surface;
}

void vk_renderer_device_wait_idle(VkRendererDevice* device) {
    if (device && device->device != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device->device);
    }
}

void vk_renderer_device_shutdown(VkRendererDevice* device) {
    if (!device) return;
    if (device->device != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(device->device);
        if (device->pipeline_cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(device->device, device->pipeline_cache, NULL);
        }
    }
    if (device->pending_surface != VK_NULL_HANDLE &&
        device->instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(device->instance, device->pending_surface, NULL);
        device->pending_surface = VK_NULL_HANDLE;
    }
    vk_runtime_shutdown(&device->runtime);
    memset(device, 0, sizeof(*device));
}
