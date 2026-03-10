#include "vk_renderer_device.h"

#include <SDL2/SDL_vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
    (void)message_type;
    (void)user_data;

    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "[vulkan] %s\n", callback_data->pMessage);
    }
    return VK_FALSE;
}

static VkBool32 validation_layers_available(const char* const* layers,
                                            uint32_t layer_count) {
    uint32_t available_count = 0;
    if (vkEnumerateInstanceLayerProperties(&available_count, NULL) != VK_SUCCESS) {
        return VK_FALSE;
    }

    if (available_count == 0) {
        return VK_FALSE;
    }

    VkLayerProperties* available =
        (VkLayerProperties*)malloc(sizeof(VkLayerProperties) * available_count);
    if (!available) {
        return VK_FALSE;
    }

    VkResult result = vkEnumerateInstanceLayerProperties(&available_count, available);
    if (result != VK_SUCCESS) {
        free(available);
        return VK_FALSE;
    }

    for (uint32_t i = 0; i < layer_count; ++i) {
        VkBool32 found = VK_FALSE;
        for (uint32_t j = 0; j < available_count; ++j) {
            if (strcmp(layers[i], available[j].layerName) == 0) {
                found = VK_TRUE;
                break;
            }
        }
        if (!found) {
            free(available);
            return VK_FALSE;
        }
    }

    free(available);
    return VK_TRUE;
}

static VkResult create_debug_messenger(VkInstance instance,
                                       VkBool32 enable,
                                       VkDebugUtilsMessengerEXT* messenger) {
    if (!enable) {
        *messenger = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = NULL,
    };

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");

    if (!func) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return func(instance, &create_info, NULL, messenger);
}

static void destroy_debug_messenger(VkInstance instance,
                                    VkDebugUtilsMessengerEXT messenger) {
    if (messenger == VK_NULL_HANDLE) return;
    PFN_vkDestroyDebugUtilsMessengerEXT func =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) {
        func(instance, messenger, NULL);
    }
}

static VkResult get_instance_extensions(SDL_Window* window,
                                        const VkRendererConfig* config,
                                        VkBool32 enable_validation,
                                        char*** out_extensions,
                                        uint32_t* out_count) {
    unsigned int sdl_count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_count, NULL)) {
        fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions count failed: %s\n", SDL_GetError());
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkBool32 add_portability_extension = VK_FALSE;
#if defined(__APPLE__)
    add_portability_extension = VK_TRUE;
#endif

    uint32_t total = sdl_count + (enable_validation ? 1u : 0u) +
                     config->extra_instance_extension_count +
                     (add_portability_extension ? 1u : 0u);
    char** list = (char**)calloc(total, sizeof(char*));
    if (!list) return VK_ERROR_OUT_OF_HOST_MEMORY;

    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl_count, (const char**)list)) {
        fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions list failed: %s\n", SDL_GetError());
        free(list);
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    uint32_t index = sdl_count;
    if (enable_validation) {
        list[index++] = (char*)VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }

    for (uint32_t i = 0; i < config->extra_instance_extension_count; ++i) {
        list[index++] = (char*)config->extra_instance_extensions[i];
    }

    if (add_portability_extension) {
        VkBool32 already_present = VK_FALSE;
        for (uint32_t i = 0; i < index; ++i) {
            if (strcmp(list[i], VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                already_present = VK_TRUE;
                break;
            }
        }
        if (!already_present) {
            list[index++] = (char*)VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        }
    }

    *out_extensions = list;
    *out_count = index;
    return VK_SUCCESS;
}

static VkResult create_instance(VkRendererDevice* device,
                                SDL_Window* window,
                                const VkRendererConfig* config,
                                VkBool32* out_validation_enabled) {
    const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    VkBool32 enable_validation = config->enable_validation;

    if (enable_validation && !validation_layers_available(validation_layers, 1)) {
        fprintf(stderr,
                "[vulkan] validation layer requested but not available; disabling validation.\n");
        enable_validation = VK_FALSE;
    }

    for (;;) {
        char** extensions = NULL;
        uint32_t extension_count = 0;
        VkResult result = get_instance_extensions(window, config, enable_validation, &extensions,
                                                  &extension_count);
        if (result != VK_SUCCESS) {
            return result;
        }

        VkApplicationInfo app_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = NULL,
            .pApplicationName = "SDL Vulkan Renderer",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "custom",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_2,
        };

        VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .pApplicationInfo = &app_info,
            .enabledExtensionCount = extension_count,
            .ppEnabledExtensionNames = (const char* const*)extensions,
            .enabledLayerCount = enable_validation ? 1u : 0u,
            .ppEnabledLayerNames = enable_validation ? validation_layers : NULL,
        };

#if defined(__APPLE__)
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = NULL,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debug_callback,
            .pUserData = NULL,
        };

        if (enable_validation) {
            create_info.pNext = &debug_create_info;
        }

        result = vkCreateInstance(&create_info, NULL, &device->instance);
        free(extensions);

        if (result == VK_SUCCESS) {
            if (out_validation_enabled) {
                *out_validation_enabled = enable_validation;
            }
            return VK_SUCCESS;
        }

        if (enable_validation &&
            (result == VK_ERROR_LAYER_NOT_PRESENT || result == VK_ERROR_EXTENSION_NOT_PRESENT)) {
            fprintf(stderr,
                    "[vulkan] validation layer failed to load (error %d); retrying without validation.\n",
                    result);
            enable_validation = VK_FALSE;
            continue;
        }

        return result;
    }
}

typedef struct QueueFamilySelection {
    uint32_t graphics_index;
    uint32_t present_index;
    VkBool32 has_graphics;
    VkBool32 has_present;
} QueueFamilySelection;

static VkBool32 queue_family_supports_surface(VkPhysicalDevice device,
                                              uint32_t index,
                                              VkSurfaceKHR surface) {
    VkBool32 present = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &present);
    return present;
}

static QueueFamilySelection select_queue_families(VkPhysicalDevice device,
                                                  VkSurfaceKHR surface) {
    QueueFamilySelection selection = {0, 0, VK_FALSE, VK_FALSE};

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    if (count == 0) return selection;

    VkQueueFamilyProperties* properties =
        (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties);

    for (uint32_t i = 0; i < count; ++i) {
        if (!selection.has_graphics &&
            (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            selection.graphics_index = i;
            selection.has_graphics = VK_TRUE;
        }

        if (!selection.has_present && queue_family_supports_surface(device, i, surface)) {
            selection.present_index = i;
            selection.has_present = VK_TRUE;
        }

        if (selection.has_graphics && selection.has_present) break;
    }

    free(properties);
    return selection;
}

static VkBool32 device_supports_extensions(VkPhysicalDevice device,
                                           const char* const* extensions,
                                           uint32_t extension_count) {
    uint32_t available_count = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &available_count, NULL);
    VkExtensionProperties* available =
        (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * available_count);
    vkEnumerateDeviceExtensionProperties(device, NULL, &available_count, available);

    VkBool32 supported = VK_TRUE;
    for (uint32_t i = 0; i < extension_count; ++i) {
        VkBool32 found = VK_FALSE;
        for (uint32_t j = 0; j < available_count; ++j) {
            if (strcmp(extensions[i], available[j].extensionName) == 0) {
                found = VK_TRUE;
                break;
            }
        }
        if (!found) {
            supported = VK_FALSE;
            break;
        }
    }

    free(available);
    return supported;
}

static VkBool32 device_supports_extension(VkPhysicalDevice device,
                                          const char *extension_name) {
    return device_supports_extensions(device, &extension_name, 1u);
}

static VkBool32 device_supports_swapchain(VkPhysicalDevice device,
                                          VkSurfaceKHR surface) {
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, NULL);
    if (format_count == 0) return VK_FALSE;
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, NULL);
    if (present_mode_count == 0) return VK_FALSE;
    return VK_TRUE;
}

static VkBool32 device_is_suitable(VkPhysicalDevice device,
                                   VkSurfaceKHR surface,
                                   const VkRendererConfig* config) {
    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    uint32_t extension_count = 1;

    VkBool32 supports_required = device_supports_extensions(device, extensions, extension_count);
    if (!supports_required) return VK_FALSE;

    if (config->extra_device_extension_count > 0) {
        supports_required = device_supports_extensions(
            device, config->extra_device_extensions, config->extra_device_extension_count);
        if (!supports_required) return VK_FALSE;
    }

    if (!device_supports_swapchain(device, surface)) return VK_FALSE;

    QueueFamilySelection families = select_queue_families(device, surface);
    return families.has_graphics && families.has_present;
}

static VkResult pick_physical_device(VkRendererDevice* device,
                                     VkSurfaceKHR surface,
                                     const VkRendererConfig* config) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(device->instance, &device_count, NULL);
    if (device_count == 0) {
        fprintf(stderr, "No Vulkan physical devices found.\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPhysicalDevice* devices =
        (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(device->instance, &device_count, devices);

    VkPhysicalDevice selected = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < device_count; ++i) {
        VkPhysicalDevice candidate = devices[i];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(candidate, &props);
        if (!device_is_suitable(candidate, surface, config)) continue;

        if (config->prefer_discrete_gpu &&
            props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            selected = candidate;
            break;
        }

        if (selected == VK_NULL_HANDLE) {
            selected = candidate;
        }
    }

    if (selected == VK_NULL_HANDLE) {
        fprintf(stderr, "No suitable Vulkan device found.\n");
        free(devices);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    device->physical_device = selected;
    vkGetPhysicalDeviceProperties(device->physical_device, &device->physical_device_properties);
    vkGetPhysicalDeviceMemoryProperties(device->physical_device, &device->memory_properties);

    QueueFamilySelection families = select_queue_families(device->physical_device, surface);
    device->graphics_queue_family = families.graphics_index;
    device->present_queue_family = families.present_index;

    free(devices);
    return VK_SUCCESS;
}

static VkResult create_logical_device(VkRendererDevice* device,
                                      VkSurfaceKHR surface,
                                      const VkRendererConfig* config) {
    QueueFamilySelection families = select_queue_families(device->physical_device, surface);

    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo queue_create_infos[2];
    uint32_t queue_info_count = 0;

    queue_create_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = families.graphics_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    if (families.graphics_index != families.present_index) {
        queue_create_infos[queue_info_count++] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = families.present_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    VkPhysicalDeviceFeatures device_features = {
        .samplerAnisotropy = VK_TRUE,
    };

    const char* device_extensions[9];
    uint32_t device_extension_count = 0;
    device_extensions[device_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
#if defined(__APPLE__)
    if (device_supports_extension(device->physical_device, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
        device_extensions[device_extension_count++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
    }
#endif
    for (uint32_t i = 0; i < config->extra_device_extension_count; ++i) {
        device_extensions[device_extension_count++] = config->extra_device_extensions[i];
    }

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueCreateInfoCount = queue_info_count,
        .pQueueCreateInfos = queue_create_infos,
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = &device_features,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = NULL,
    };

    VkResult result = vkCreateDevice(device->physical_device, &create_info, NULL, &device->device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkCreateDevice failed: %d\n", (int)result);
        return result;
    }

    vkGetDeviceQueue(device->device, families.graphics_index, 0, &device->graphics_queue);
    vkGetDeviceQueue(device->device, families.present_index, 0, &device->present_queue);

    device->graphics_queue_family = families.graphics_index;
    device->present_queue_family = families.present_index;

    return VK_SUCCESS;
}

static VkResult create_pipeline_cache(VkRendererDevice* device) {
    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .initialDataSize = 0,
        .pInitialData = NULL,
    };
    return vkCreatePipelineCache(device->device, &cache_info, NULL, &device->pipeline_cache);
}

VkResult vk_renderer_device_init(VkRendererDevice* device,
                                 SDL_Window* window,
                                 const VkRendererConfig* config) {
    if (!device || !window || !config) return VK_ERROR_INITIALIZATION_FAILED;
    memset(device, 0, sizeof(*device));

    VkBool32 validation_enabled = config->enable_validation;
    VkResult result = create_instance(device, window, config, &validation_enabled);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "create_instance failed: %d\n", (int)result);
        return result;
    }

    device->validation_enabled = validation_enabled;
    result = create_debug_messenger(device->instance, validation_enabled, &device->debug_messenger);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create debug messenger.\n");
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, device->instance, &surface)) {
        fprintf(stderr, "SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        vk_renderer_device_shutdown(device);
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    result = pick_physical_device(device, surface, config);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "pick_physical_device failed: %d\n", (int)result);
        vkDestroySurfaceKHR(device->instance, surface, NULL);
        vk_renderer_device_shutdown(device);
        return result;
    }

    result = create_logical_device(device, surface, config);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "create_logical_device failed: %d\n", (int)result);
        vkDestroySurfaceKHR(device->instance, surface, NULL);
        vk_renderer_device_shutdown(device);
        return result;
    }

    result = create_pipeline_cache(device);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "create_pipeline_cache failed: %d\n", (int)result);
        vkDestroySurfaceKHR(device->instance, surface, NULL);
        vk_renderer_device_shutdown(device);
        return result;
    }

    vkDestroySurfaceKHR(device->instance, surface, NULL);
    return VK_SUCCESS;
}

void vk_renderer_device_wait_idle(VkRendererDevice* device) {
    if (!device || device->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device->device);
}

void vk_renderer_device_shutdown(VkRendererDevice* device) {
    if (!device) return;

    if (device->device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device->device);
        if (device->pipeline_cache != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(device->device, device->pipeline_cache, NULL);
            device->pipeline_cache = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device->device, NULL);
    }

    destroy_debug_messenger(device->instance, device->debug_messenger);

    if (device->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(device->instance, NULL);
    }

    memset(device, 0, sizeof(*device));
}
