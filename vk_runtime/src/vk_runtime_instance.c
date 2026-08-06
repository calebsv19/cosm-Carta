#include "vk_runtime_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool extension_list_contains(
    const VkExtensionProperties *extensions,
    uint32_t extension_count,
    const char *name) {
    for (uint32_t i = 0u; i < extension_count; ++i) {
        if (strcmp(extensions[i].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool validation_layer_available(void) {
    uint32_t count = 0u;
    VkResult result = vkEnumerateInstanceLayerProperties(&count, NULL);
    if (result != VK_SUCCESS || count == 0u) {
        return false;
    }

    VkLayerProperties *layers =
        (VkLayerProperties *)calloc(count, sizeof(*layers));
    if (!layers) {
        return false;
    }

    result = vkEnumerateInstanceLayerProperties(&count, layers);
    if (result != VK_SUCCESS) {
        free(layers);
        return false;
    }

    bool found = false;
    for (uint32_t i = 0u; i < count; ++i) {
        if (strcmp(layers[i].layerName,
                   "VK_LAYER_KHRONOS_validation") == 0) {
            found = true;
            break;
        }
    }
    free(layers);
    return found;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL runtime_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    VkRuntime *runtime = (VkRuntime *)user_data;
    (void)message_type;

    if (runtime) {
        if ((severity &
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
            runtime->report.validation_error_count += 1u;
        } else if ((severity &
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0u) {
            runtime->report.validation_warning_count += 1u;
        }
    }

    if ((severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) != 0u) {
        fprintf(stderr,
                "[vk_runtime validation] %s\n",
                callback_data && callback_data->pMessage
                    ? callback_data->pMessage
                    : "(message unavailable)");
    }
    return VK_FALSE;
}

static void fill_debug_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *create_info,
    VkRuntime *runtime) {
    memset(create_info, 0, sizeof(*create_info));
    create_info->sType =
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info->messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info->messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info->pfnUserCallback = runtime_debug_callback;
    create_info->pUserData = runtime;
}

VkRuntimeStatus vk_runtime_internal_create_instance(
    VkRuntime *runtime,
    const VkRuntimeConfig *config) {
    PFN_vkEnumerateInstanceVersion enumerate_instance_version =
        (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(
            VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    uint32_t loader_version = VK_API_VERSION_1_0;
    if (enumerate_instance_version) {
        VkResult result = enumerate_instance_version(&loader_version);
        if (result != VK_SUCCESS) {
            return vk_runtime_internal_set_failure(
                &runtime->report,
                VK_RUNTIME_STATUS_LOADER_UNAVAILABLE,
                result);
        }
    }
    runtime->report.loader_api_version = loader_version;

    uint32_t requested = config->requested_api_version;
    uint32_t negotiated =
        requested < loader_version ? requested : loader_version;
    runtime->report.negotiated_api_version = negotiated;
    if (VK_API_VERSION_MAJOR(negotiated) < 1u ||
        (VK_API_VERSION_MAJOR(negotiated) == 1u &&
         VK_API_VERSION_MINOR(negotiated) < 1u)) {
        return vk_runtime_internal_set_failure(
            &runtime->report,
            VK_RUNTIME_STATUS_API_VERSION_UNSUPPORTED,
            VK_ERROR_INCOMPATIBLE_DRIVER);
    }

    uint32_t extension_count = 0u;
    VkResult result =
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    if (result != VK_SUCCESS) {
        return vk_runtime_internal_set_failure(
            &runtime->report,
            VK_RUNTIME_STATUS_LOADER_UNAVAILABLE,
            result);
    }

    VkExtensionProperties *extensions = NULL;
    if (extension_count > 0u) {
        extensions = (VkExtensionProperties *)calloc(
            extension_count, sizeof(*extensions));
        if (!extensions) {
            return vk_runtime_internal_set_failure(
                &runtime->report,
                VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
                VK_ERROR_OUT_OF_HOST_MEMORY);
        }
        result = vkEnumerateInstanceExtensionProperties(
            NULL, &extension_count, extensions);
        if (result != VK_SUCCESS) {
            free(extensions);
            return vk_runtime_internal_set_failure(
                &runtime->report,
                VK_RUNTIME_STATUS_LOADER_UNAVAILABLE,
                result);
        }
    }

    runtime->report.portability_enumeration_available =
        extension_list_contains(
            extensions,
            extension_count,
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    runtime->report.debug_utils_available =
        extension_list_contains(extensions,
                                extension_count,
                                VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    runtime->report.validation_available = validation_layer_available();

    bool validation_enabled =
        runtime->report.validation_requested &&
        runtime->report.validation_available &&
        runtime->report.debug_utils_available;

    if (config->require_validation &&
        !runtime->report.validation_available) {
        free(extensions);
        return vk_runtime_internal_set_failure(
            &runtime->report,
            VK_RUNTIME_STATUS_VALIDATION_LAYER_MISSING,
            VK_ERROR_LAYER_NOT_PRESENT);
    }
    if (config->require_validation &&
        !runtime->report.debug_utils_available) {
        free(extensions);
        return vk_runtime_internal_set_failure(
            &runtime->report,
            VK_RUNTIME_STATUS_INSTANCE_EXTENSION_MISSING,
            VK_ERROR_EXTENSION_NOT_PRESENT);
    }

    VkInstanceCreateFlags instance_flags = 0u;
    if (runtime->report.portability_enumeration_available) {
        instance_flags |=
            VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        runtime->report.portability_enumeration_enabled = true;
    }

    const char *enabled_layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    VkApplicationInfo app_info;
    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = config->application_name;
    app_info.applicationVersion = config->application_version;
    app_info.pEngineName = "CodeWork vk_runtime";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 2, 0);
    app_info.apiVersion = negotiated;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info;
    fill_debug_create_info(&debug_create_info, runtime);

    bool attempt_validation = validation_enabled;
    for (;;) {
        const char *enabled_extensions[32];
        uint32_t enabled_extension_count = 0u;
        if (runtime->report.portability_enumeration_available) {
            enabled_extensions[enabled_extension_count++] =
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        }
        if (attempt_validation) {
            enabled_extensions[enabled_extension_count++] =
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        for (uint32_t i = 0u; i < config->instance_extension_count; ++i) {
            const char *name = config->instance_extensions[i];
            bool duplicate = false;
            if (!name || name[0] == '\0') {
                free(extensions);
                return vk_runtime_internal_set_failure(
                    &runtime->report,
                    VK_RUNTIME_STATUS_INVALID_ARGUMENT,
                    VK_ERROR_INITIALIZATION_FAILED);
            }
            if (!extension_list_contains(extensions, extension_count, name)) {
                free(extensions);
                return vk_runtime_internal_set_failure(
                    &runtime->report,
                    VK_RUNTIME_STATUS_INSTANCE_EXTENSION_MISSING,
                    VK_ERROR_EXTENSION_NOT_PRESENT);
            }
            for (uint32_t j = 0u; j < enabled_extension_count; ++j) {
                if (strcmp(enabled_extensions[j], name) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                if (enabled_extension_count >= 32u) {
                    free(extensions);
                    return vk_runtime_internal_set_failure(
                        &runtime->report,
                        VK_RUNTIME_STATUS_INTERNAL_LIMIT_EXCEEDED,
                        VK_ERROR_TOO_MANY_OBJECTS);
                }
                enabled_extensions[enabled_extension_count++] = name;
            }
        }

        VkInstanceCreateInfo create_info;
        memset(&create_info, 0, sizeof(create_info));
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pNext =
            attempt_validation ? &debug_create_info : NULL;
        create_info.flags = instance_flags;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = enabled_extension_count;
        create_info.ppEnabledExtensionNames =
            enabled_extension_count > 0u ? enabled_extensions : NULL;
        create_info.enabledLayerCount = attempt_validation ? 1u : 0u;
        create_info.ppEnabledLayerNames =
            attempt_validation ? enabled_layers : NULL;

        uint32_t errors_before =
            runtime->report.validation_error_count;
        result =
            vkCreateInstance(&create_info, NULL, &runtime->instance);
        bool validation_bootstrap_error =
            attempt_validation &&
            runtime->report.validation_error_count > errors_before;

        if (result != VK_SUCCESS || validation_bootstrap_error) {
            if (runtime->instance != VK_NULL_HANDLE) {
                vkDestroyInstance(runtime->instance, NULL);
                runtime->instance = VK_NULL_HANDLE;
            }
            if (attempt_validation && !config->require_validation) {
                runtime->report.validation_load_failed = true;
                runtime->report.validation_warning_count = 0u;
                runtime->report.validation_error_count = 0u;
                attempt_validation = false;
                continue;
            }
            free(extensions);
            if (attempt_validation &&
                (result == VK_ERROR_LAYER_NOT_PRESENT ||
                 validation_bootstrap_error)) {
                return vk_runtime_internal_set_failure(
                    &runtime->report,
                    VK_RUNTIME_STATUS_VALIDATION_LAYER_MISSING,
                    result == VK_SUCCESS
                        ? VK_ERROR_LAYER_NOT_PRESENT
                        : result);
            }
            return vk_runtime_internal_set_failure(
                &runtime->report,
                VK_RUNTIME_STATUS_INSTANCE_CREATE_FAILED,
                result);
        }

        if (attempt_validation) {
            PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger =
                (PFN_vkCreateDebugUtilsMessengerEXT)
                    vkGetInstanceProcAddr(
                        runtime->instance,
                        "vkCreateDebugUtilsMessengerEXT");
            if (!create_debug_messenger) {
                result = VK_ERROR_EXTENSION_NOT_PRESENT;
            } else {
                result = create_debug_messenger(
                    runtime->instance,
                    &debug_create_info,
                    NULL,
                    &runtime->debug_messenger);
            }
            if (result != VK_SUCCESS) {
                vkDestroyInstance(runtime->instance, NULL);
                runtime->instance = VK_NULL_HANDLE;
                runtime->debug_messenger = VK_NULL_HANDLE;
                if (!config->require_validation) {
                    runtime->report.validation_load_failed = true;
                    runtime->report.validation_warning_count = 0u;
                    runtime->report.validation_error_count = 0u;
                    attempt_validation = false;
                    continue;
                }
                free(extensions);
                return vk_runtime_internal_set_failure(
                    &runtime->report,
                    VK_RUNTIME_STATUS_DEBUG_MESSENGER_CREATE_FAILED,
                    result);
            }
        }

        runtime->report.validation_enabled = attempt_validation;
        free(extensions);
        return VK_RUNTIME_STATUS_OK;
    }
}

void vk_runtime_internal_close_instance(VkRuntime *runtime) {
    if (runtime->debug_messenger != VK_NULL_HANDLE &&
        runtime->instance != VK_NULL_HANDLE) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                runtime->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_debug_messenger) {
            destroy_debug_messenger(runtime->instance,
                                    runtime->debug_messenger,
                                    NULL);
        }
        runtime->debug_messenger = VK_NULL_HANDLE;
    }
    if (runtime->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(runtime->instance, NULL);
        runtime->instance = VK_NULL_HANDLE;
    }
}
