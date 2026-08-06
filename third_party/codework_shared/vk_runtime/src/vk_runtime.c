#include "vk_runtime_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

#ifndef VK_RUNTIME_BUILD_VERSION
#error "VK_RUNTIME_BUILD_VERSION must be supplied from the canonical VERSION file"
#endif

typedef struct VkRuntimeCandidate {
    VkPhysicalDevice handle;
    VkRuntimeDeviceCapability capability;
} VkRuntimeCandidate;

const char *vk_runtime_version_string(void) {
    return VK_RUNTIME_BUILD_VERSION;
}

void copy_text(char *destination,
               size_t destination_capacity,
               const char *source) {
    if (!destination || destination_capacity == 0u) {
        return;
    }
    if (!source) {
        source = "";
    }
    (void)snprintf(destination, destination_capacity, "%s", source);
}

static void set_build_identity(VkRuntimeCapabilityReport *report) {
#if defined(__APPLE__)
    copy_text(report->platform, sizeof(report->platform), "macos");
#elif defined(_WIN32)
    copy_text(report->platform, sizeof(report->platform), "windows");
#elif defined(__linux__)
    copy_text(report->platform, sizeof(report->platform), "linux");
#else
    copy_text(report->platform, sizeof(report->platform), "unknown");
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    copy_text(report->architecture, sizeof(report->architecture), "arm64");
#elif defined(__x86_64__) || defined(_M_X64)
    copy_text(report->architecture, sizeof(report->architecture), "x86_64");
#elif defined(__i386__) || defined(_M_IX86)
    copy_text(report->architecture, sizeof(report->architecture), "x86");
#else
    copy_text(report->architecture, sizeof(report->architecture), "unknown");
#endif

#if defined(__clang__)
    copy_text(report->compiler, sizeof(report->compiler), "clang " __clang_version__);
#elif defined(__GNUC__)
    (void)snprintf(report->compiler,
                   sizeof(report->compiler),
                   "gcc %u.%u.%u",
                   (unsigned)__GNUC__,
                   (unsigned)__GNUC_MINOR__,
                   (unsigned)__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    (void)snprintf(report->compiler,
                   sizeof(report->compiler),
                   "msvc %u",
                   (unsigned)_MSC_VER);
#else
    copy_text(report->compiler, sizeof(report->compiler), "unknown");
#endif
}

static void initialize_report(VkRuntimeCapabilityReport *report,
                              const VkRuntimeConfig *config) {
    memset(report, 0, sizeof(*report));
    report->schema_version = VK_RUNTIME_CAPABILITY_SCHEMA_VERSION;
    report->vulkan_header_version = VK_HEADER_VERSION_COMPLETE;
    report->requested_api_version = config->requested_api_version;
    report->selected_device_index = UINT32_MAX;
    report->validation_requested =
        config->enable_validation || config->require_validation;
    report->status = VK_RUNTIME_STATUS_OK;
    report->vulkan_result = VK_SUCCESS;
    set_build_identity(report);
}

VkRuntimeStatus set_failure(VkRuntimeCapabilityReport *report,
                            VkRuntimeStatus status,
                            VkResult result) {
    report->status = status;
    report->vulkan_result = result;
    return status;
}

static int extension_name_compare(const void *left, const void *right) {
    const char *left_name = (const char *)left;
    const char *right_name = (const char *)right;
    return strcmp(left_name, right_name);
}

static bool device_has_extension(const VkRuntimeDeviceCapability *capability,
                                 const char *extension_name) {
    for (uint32_t i = 0u; i < capability->extension_count; ++i) {
        if (strcmp(capability->extensions[i], extension_name) == 0) {
            return true;
        }
    }
    return false;
}

static VkRuntimeStatus read_device_extensions(
    VkPhysicalDevice device,
    VkRuntimeDeviceCapability *capability,
    VkResult *out_result) {
    uint32_t count = 0u;
    VkResult result =
        vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
    if (result != VK_SUCCESS) {
        *out_result = result;
        return VK_RUNTIME_STATUS_NO_SUITABLE_DEVICE;
    }

    VkExtensionProperties *properties = NULL;
    if (count > 0u) {
        properties =
            (VkExtensionProperties *)calloc(count, sizeof(*properties));
        capability->extensions =
            calloc(count, sizeof(*capability->extensions));
        if (!properties || !capability->extensions) {
            free(properties);
            free(capability->extensions);
            capability->extensions = NULL;
            *out_result = VK_ERROR_OUT_OF_HOST_MEMORY;
            return VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY;
        }
        result = vkEnumerateDeviceExtensionProperties(
            device, NULL, &count, properties);
        if (result != VK_SUCCESS) {
            free(properties);
            free(capability->extensions);
            capability->extensions = NULL;
            *out_result = result;
            return VK_RUNTIME_STATUS_NO_SUITABLE_DEVICE;
        }
        for (uint32_t i = 0u; i < count; ++i) {
            copy_text(capability->extensions[i],
                      sizeof(capability->extensions[i]),
                      properties[i].extensionName);
        }
        qsort(capability->extensions,
              count,
              sizeof(capability->extensions[0]),
              extension_name_compare);
    }

    free(properties);
    capability->extension_count = count;
    capability->portability_subset =
        device_has_extension(capability,
                             VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    *out_result = VK_SUCCESS;
    return VK_RUNTIME_STATUS_OK;
}

static uint32_t choose_graphics_queue(
    const VkRuntimeQueueFamilyCapability *families,
    uint32_t count) {
    for (uint32_t i = 0u; i < count; ++i) {
        if (families[i].graphics && families[i].queue_count > 0u) {
            return i;
        }
    }
    return VK_RUNTIME_INVALID_QUEUE_FAMILY;
}

static uint32_t choose_compute_queue(
    const VkRuntimeQueueFamilyCapability *families,
    uint32_t count) {
    for (uint32_t i = 0u; i < count; ++i) {
        if (families[i].compute && !families[i].graphics &&
            families[i].queue_count > 0u) {
            return i;
        }
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (families[i].compute && families[i].queue_count > 0u) {
            return i;
        }
    }
    return VK_RUNTIME_INVALID_QUEUE_FAMILY;
}

static uint32_t choose_transfer_queue(
    const VkRuntimeQueueFamilyCapability *families,
    uint32_t count) {
    for (uint32_t i = 0u; i < count; ++i) {
        if (families[i].transfer && !families[i].graphics &&
            !families[i].compute && families[i].queue_count > 0u) {
            return i;
        }
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (families[i].transfer && !families[i].graphics &&
            families[i].queue_count > 0u) {
            return i;
        }
    }
    for (uint32_t i = 0u; i < count; ++i) {
        if (families[i].transfer && families[i].queue_count > 0u) {
            return i;
        }
    }
    return VK_RUNTIME_INVALID_QUEUE_FAMILY;
}

static VkRuntimeStatus read_queue_families(
    VkPhysicalDevice device,
    VkSurfaceKHR presentation_surface,
    VkRuntimeDeviceCapability *capability) {
    uint32_t count = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
    if (count == 0u) {
        capability->graphics_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
        capability->compute_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
        capability->transfer_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
        capability->present_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
        return VK_RUNTIME_STATUS_OK;
    }

    VkQueueFamilyProperties *properties =
        (VkQueueFamilyProperties *)calloc(count, sizeof(*properties));
    capability->queue_families =
        (VkRuntimeQueueFamilyCapability *)calloc(
            count, sizeof(*capability->queue_families));
    if (!properties || !capability->queue_families) {
        free(properties);
        free(capability->queue_families);
        capability->queue_families = NULL;
        return VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties);
    capability->queue_family_count = count;
    for (uint32_t i = 0u; i < count; ++i) {
        VkRuntimeQueueFamilyCapability *output =
            &capability->queue_families[i];
        output->index = i;
        output->queue_count = properties[i].queueCount;
        output->queue_flags = properties[i].queueFlags;
        output->timestamp_valid_bits = properties[i].timestampValidBits;
        output->min_image_transfer_granularity[0] =
            properties[i].minImageTransferGranularity.width;
        output->min_image_transfer_granularity[1] =
            properties[i].minImageTransferGranularity.height;
        output->min_image_transfer_granularity[2] =
            properties[i].minImageTransferGranularity.depth;
        output->graphics =
            (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u;
        output->compute =
            (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u;
        output->transfer =
            (properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0u;
        output->sparse_binding =
            (properties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0u;
        output->protected_queue =
            (properties[i].queueFlags & VK_QUEUE_PROTECTED_BIT) != 0u;
        if (presentation_surface != VK_NULL_HANDLE) {
            VkBool32 present = VK_FALSE;
            VkResult present_result = vkGetPhysicalDeviceSurfaceSupportKHR(
                device, i, presentation_surface, &present);
            output->present =
                present_result == VK_SUCCESS && present == VK_TRUE;
        }
    }
    free(properties);

    capability->graphics_queue_family =
        choose_graphics_queue(capability->queue_families, count);
    capability->compute_queue_family =
        choose_compute_queue(capability->queue_families, count);
    capability->transfer_queue_family =
        choose_transfer_queue(capability->queue_families, count);
    capability->present_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
    for (uint32_t i = 0u; i < count; ++i) {
        if (capability->queue_families[i].present &&
            capability->queue_families[i].queue_count > 0u) {
            capability->present_queue_family = i;
            break;
        }
    }
    return VK_RUNTIME_STATUS_OK;
}

static void uuid_to_text(const uint8_t uuid[VK_UUID_SIZE],
                         char output[VK_RUNTIME_UUID_TEXT_CAPACITY]) {
    static const char digits[] = "0123456789abcdef";
    for (uint32_t i = 0u; i < VK_UUID_SIZE; ++i) {
        output[i * 2u] = digits[(uuid[i] >> 4u) & 0x0fu];
        output[i * 2u + 1u] = digits[uuid[i] & 0x0fu];
    }
    output[VK_UUID_SIZE * 2u] = '\0';
}

static VkRuntimeStatus read_device_capability(
    VkPhysicalDevice device,
    const VkRuntimeConfig *config,
    VkSurfaceKHR presentation_surface,
    VkRuntimeDeviceCapability *capability,
    VkResult *out_result) {
    memset(capability, 0, sizeof(*capability));
    capability->graphics_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
    capability->compute_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
    capability->transfer_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;
    capability->present_queue_family = VK_RUNTIME_INVALID_QUEUE_FAMILY;

    VkPhysicalDeviceProperties base_properties;
    vkGetPhysicalDeviceProperties(device, &base_properties);

    VkRuntimeStatus status =
        read_device_extensions(device, capability, out_result);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }

    VkPhysicalDeviceIDProperties id_properties;
    memset(&id_properties, 0, sizeof(id_properties));
    id_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceSubgroupProperties subgroup_properties;
    memset(&subgroup_properties, 0, sizeof(subgroup_properties));
    subgroup_properties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    id_properties.pNext = &subgroup_properties;

    VkPhysicalDeviceDriverProperties driver_properties;
    memset(&driver_properties, 0, sizeof(driver_properties));
    driver_properties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

    bool driver_properties_supported =
        base_properties.apiVersion >= VK_API_VERSION_1_2 ||
        device_has_extension(capability,
                             VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME);
    if (driver_properties_supported) {
        subgroup_properties.pNext = &driver_properties;
    }

    VkPhysicalDeviceProperties2 properties;
    memset(&properties, 0, sizeof(properties));
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &id_properties;
    vkGetPhysicalDeviceProperties2(device, &properties);

    copy_text(capability->device_name,
              sizeof(capability->device_name),
              properties.properties.deviceName);
    capability->vendor_id = properties.properties.vendorID;
    capability->device_id = properties.properties.deviceID;
    capability->device_type = properties.properties.deviceType;
    capability->api_version = properties.properties.apiVersion;
    capability->driver_version = properties.properties.driverVersion;
    uuid_to_text(id_properties.deviceUUID, capability->device_uuid);
    capability->subgroup_size = subgroup_properties.subgroupSize;
    capability->subgroup_supported_stages =
        subgroup_properties.supportedStages;
    capability->subgroup_supported_operations =
        subgroup_properties.supportedOperations;

    if (driver_properties_supported) {
        copy_text(capability->driver_name,
                  sizeof(capability->driver_name),
                  driver_properties.driverName);
        copy_text(capability->driver_info,
                  sizeof(capability->driver_info),
                  driver_properties.driverInfo);
    } else {
        copy_text(capability->driver_name,
                  sizeof(capability->driver_name),
                  "unavailable");
        copy_text(capability->driver_info,
                  sizeof(capability->driver_info),
                  "unavailable");
    }

    VkPhysicalDeviceFeatures features;
    memset(&features, 0, sizeof(features));
    vkGetPhysicalDeviceFeatures(device, &features);
    capability->sampler_anisotropy = features.samplerAnisotropy == VK_TRUE;
    capability->shader_float64 = features.shaderFloat64 == VK_TRUE;
    capability->shader_int64 = features.shaderInt64 == VK_TRUE;

    VkPhysicalDeviceMemoryProperties memory;
    memset(&memory, 0, sizeof(memory));
    vkGetPhysicalDeviceMemoryProperties(device, &memory);
    capability->memory_heap_count = memory.memoryHeapCount;
    for (uint32_t i = 0u; i < memory.memoryHeapCount; ++i) {
        capability->memory_heaps[i].size_bytes = memory.memoryHeaps[i].size;
        capability->memory_heaps[i].flags = memory.memoryHeaps[i].flags;
        capability->memory_heaps[i].device_local =
            (memory.memoryHeaps[i].flags &
             VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
        capability->memory_heaps[i].multi_instance =
            (memory.memoryHeaps[i].flags &
             VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) != 0u;
    }

    status = read_queue_families(device, presentation_surface, capability);
    if (status != VK_RUNTIME_STATUS_OK) {
        *out_result = VK_ERROR_OUT_OF_HOST_MEMORY;
        return status;
    }

    if (config->require_graphics_queue &&
        capability->graphics_queue_family ==
            VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        capability->rejection_bits |=
            VK_RUNTIME_DEVICE_REJECT_GRAPHICS_QUEUE_MISSING;
    }
    if (config->require_compute_queue &&
        capability->compute_queue_family ==
            VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        capability->rejection_bits |=
            VK_RUNTIME_DEVICE_REJECT_COMPUTE_QUEUE_MISSING;
    }
    if (config->require_transfer_queue &&
        capability->transfer_queue_family ==
            VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        capability->rejection_bits |=
            VK_RUNTIME_DEVICE_REJECT_TRANSFER_QUEUE_MISSING;
    }
    if (config->require_present_queue &&
        capability->present_queue_family ==
            VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        capability->rejection_bits |=
            VK_RUNTIME_DEVICE_REJECT_PRESENT_QUEUE_MISSING;
    }
    for (uint32_t i = 0u; i < config->device_extension_count; ++i) {
        if (!config->device_extensions[i] ||
            !device_has_extension(capability, config->device_extensions[i])) {
            capability->rejection_bits |=
                VK_RUNTIME_DEVICE_REJECT_DEVICE_EXTENSION_MISSING;
        }
    }
    capability->suitable =
        capability->rejection_bits == VK_RUNTIME_DEVICE_REJECT_NONE;
    *out_result = VK_SUCCESS;
    return VK_RUNTIME_STATUS_OK;
}

static int candidate_compare(const void *left, const void *right) {
    const VkRuntimeCandidate *left_candidate =
        (const VkRuntimeCandidate *)left;
    const VkRuntimeCandidate *right_candidate =
        (const VkRuntimeCandidate *)right;
    const VkRuntimeDeviceCapability *a = &left_candidate->capability;
    const VkRuntimeDeviceCapability *b = &right_candidate->capability;

    if (a->vendor_id != b->vendor_id) {
        return a->vendor_id < b->vendor_id ? -1 : 1;
    }
    if (a->device_id != b->device_id) {
        return a->device_id < b->device_id ? -1 : 1;
    }
    int name_order = strcmp(a->device_name, b->device_name);
    if (name_order != 0) {
        return name_order;
    }
    return strcmp(a->device_uuid, b->device_uuid);
}

static int device_score(const VkRuntimeDeviceCapability *capability,
                        const VkRuntimeConfig *config) {
    int score = 0;
    if (config->prefer_discrete_gpu) {
        if (capability->device_type ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (capability->device_type ==
                   VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 500;
        }
    }
    if (capability->compute_queue_family !=
            VK_RUNTIME_INVALID_QUEUE_FAMILY &&
        !capability->queue_families[capability->compute_queue_family]
             .graphics) {
        score += 50;
    }
    if (capability->transfer_queue_family !=
            VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        const VkRuntimeQueueFamilyCapability *transfer =
            &capability->queue_families[
                capability->transfer_queue_family];
        if (!transfer->graphics && !transfer->compute) {
            score += 25;
        }
    }
    return score;
}

static void destroy_device_capability(
    VkRuntimeDeviceCapability *capability) {
    if (!capability) {
        return;
    }
    free(capability->queue_families);
    free(capability->extensions);
    capability->queue_families = NULL;
    capability->extensions = NULL;
}

static VkRuntimeStatus enumerate_and_select_device(
    VkRuntime *runtime,
    const VkRuntimeConfig *config,
    VkSurfaceKHR presentation_surface) {
    uint32_t count = 0u;
    VkResult result =
        vkEnumeratePhysicalDevices(runtime->instance, &count, NULL);
    if (result != VK_SUCCESS || count == 0u) {
        return set_failure(
            &runtime->report,
            VK_RUNTIME_STATUS_NO_PHYSICAL_DEVICE,
            result == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : result);
    }

    VkPhysicalDevice *handles =
        (VkPhysicalDevice *)calloc(count, sizeof(*handles));
    VkRuntimeCandidate *candidates =
        (VkRuntimeCandidate *)calloc(count, sizeof(*candidates));
    if (!handles || !candidates) {
        free(handles);
        free(candidates);
        return set_failure(&runtime->report,
                           VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
                           VK_ERROR_OUT_OF_HOST_MEMORY);
    }

    result = vkEnumeratePhysicalDevices(runtime->instance, &count, handles);
    if (result != VK_SUCCESS) {
        free(handles);
        free(candidates);
        return set_failure(&runtime->report,
                           VK_RUNTIME_STATUS_NO_PHYSICAL_DEVICE,
                           result);
    }

    for (uint32_t i = 0u; i < count; ++i) {
        candidates[i].handle = handles[i];
        VkRuntimeStatus status = read_device_capability(
            handles[i], config, presentation_surface,
            &candidates[i].capability, &result);
        if (status != VK_RUNTIME_STATUS_OK) {
            for (uint32_t j = 0u; j <= i; ++j) {
                destroy_device_capability(&candidates[j].capability);
            }
            free(handles);
            free(candidates);
            return set_failure(&runtime->report, status, result);
        }
    }
    free(handles);

    qsort(candidates, count, sizeof(*candidates), candidate_compare);

    uint32_t selected = UINT32_MAX;
    int selected_score = 0;
    for (uint32_t i = 0u; i < count; ++i) {
        if (!candidates[i].capability.suitable) {
            continue;
        }
        int score = device_score(&candidates[i].capability, config);
        if (selected == UINT32_MAX || score > selected_score) {
            selected = i;
            selected_score = score;
        }
    }

    runtime->report.devices =
        (VkRuntimeDeviceCapability *)calloc(
            count, sizeof(*runtime->report.devices));
    if (!runtime->report.devices) {
        for (uint32_t i = 0u; i < count; ++i) {
            destroy_device_capability(&candidates[i].capability);
        }
        free(candidates);
        return set_failure(&runtime->report,
                           VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY,
                           VK_ERROR_OUT_OF_HOST_MEMORY);
    }

    runtime->report.device_count = count;
    for (uint32_t i = 0u; i < count; ++i) {
        runtime->report.devices[i] = candidates[i].capability;
    }

    if (selected == UINT32_MAX) {
        free(candidates);
        return set_failure(&runtime->report,
                           VK_RUNTIME_STATUS_NO_SUITABLE_DEVICE,
                           VK_ERROR_FEATURE_NOT_PRESENT);
    }

    runtime->physical_device = candidates[selected].handle;
    runtime->report.selected_device_index = selected;
    runtime->report.devices[selected].selected = true;
    free(candidates);
    return VK_RUNTIME_STATUS_OK;
}

static bool append_unique_queue_family(uint32_t family,
                                       uint32_t *families,
                                       uint32_t *count) {
    if (family == VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        return true;
    }
    for (uint32_t i = 0u; i < *count; ++i) {
        if (families[i] == family) {
            return true;
        }
    }
    if (*count >= 4u) {
        return false;
    }
    families[(*count)++] = family;
    return true;
}

static VkRuntimeStatus create_logical_device(
    VkRuntime *runtime,
    const VkRuntimeConfig *config) {
    if (!config->create_logical_device) {
        return VK_RUNTIME_STATUS_OK;
    }

    VkRuntimeDeviceCapability *capability =
        &runtime->report.devices[runtime->report.selected_device_index];
    uint32_t families[4];
    uint32_t family_count = 0u;
    if (!append_unique_queue_family(capability->graphics_queue_family,
                                    families,
                                    &family_count) ||
        !append_unique_queue_family(capability->compute_queue_family,
                                    families,
                                    &family_count) ||
        !append_unique_queue_family(capability->transfer_queue_family,
                                    families,
                                    &family_count) ||
        !append_unique_queue_family(capability->present_queue_family,
                                    families,
                                    &family_count) ||
        family_count == 0u) {
        return set_failure(&runtime->report,
                           VK_RUNTIME_STATUS_INTERNAL_LIMIT_EXCEEDED,
                           VK_ERROR_INITIALIZATION_FAILED);
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos[4];
    memset(queue_create_infos, 0, sizeof(queue_create_infos));
    for (uint32_t i = 0u; i < family_count; ++i) {
        queue_create_infos[i].sType =
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = families[i];
        queue_create_infos[i].queueCount = 1u;
        queue_create_infos[i].pQueuePriorities = &priority;
    }

    const char *device_extensions[32];
    uint32_t device_extension_count = 0u;
    if (capability->portability_subset) {
        device_extensions[device_extension_count++] =
            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
    }
    for (uint32_t i = 0u; i < config->device_extension_count; ++i) {
        const char *name = config->device_extensions[i];
        bool duplicate = false;
        for (uint32_t j = 0u; j < device_extension_count; ++j) {
            if (strcmp(device_extensions[j], name) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            if (device_extension_count >= 32u) {
                return set_failure(&runtime->report,
                                   VK_RUNTIME_STATUS_INTERNAL_LIMIT_EXCEEDED,
                                   VK_ERROR_TOO_MANY_OBJECTS);
            }
            device_extensions[device_extension_count++] = name;
        }
    }

    VkDeviceCreateInfo create_info;
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = family_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledExtensionCount = device_extension_count;
    create_info.ppEnabledExtensionNames =
        device_extension_count > 0u ? device_extensions : NULL;

    VkResult result = vkCreateDevice(runtime->physical_device,
                                     &create_info,
                                     NULL,
                                     &runtime->device);
    if (result != VK_SUCCESS) {
        return set_failure(
            &runtime->report,
            VK_RUNTIME_STATUS_LOGICAL_DEVICE_CREATE_FAILED,
            result);
    }

    if (capability->graphics_queue_family !=
        VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        vkGetDeviceQueue(runtime->device,
                         capability->graphics_queue_family,
                         0u,
                         &runtime->graphics_queue);
    }
    if (capability->compute_queue_family !=
        VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        vkGetDeviceQueue(runtime->device,
                         capability->compute_queue_family,
                         0u,
                         &runtime->compute_queue);
    }
    if (capability->transfer_queue_family !=
        VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        vkGetDeviceQueue(runtime->device,
                         capability->transfer_queue_family,
                         0u,
                         &runtime->transfer_queue);
    }
    if (capability->present_queue_family !=
        VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        vkGetDeviceQueue(runtime->device,
                         capability->present_queue_family,
                         0u,
                         &runtime->present_queue);
    }
    return VK_RUNTIME_STATUS_OK;
}

void vk_runtime_config_defaults(VkRuntimeConfig *config) {
    if (!config) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->application_name = "CodeWork vk_runtime probe";
    config->application_version = VK_MAKE_API_VERSION(0, 0, 1, 0);
    config->requested_api_version = VK_API_VERSION_1_2;
    config->enable_validation = false;
    config->require_validation = false;
    config->require_graphics_queue = false;
    config->require_compute_queue = true;
    config->require_transfer_queue = false;
    config->require_present_queue = false;
    config->prefer_discrete_gpu = true;
    config->create_logical_device = true;
}

VkRuntimeStatus vk_runtime_initialize(VkRuntime *runtime,
                                      const VkRuntimeConfig *config) {
    VkRuntimeStatus status = vk_runtime_initialize_instance(runtime, config);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    return vk_runtime_initialize_device(runtime, config, VK_NULL_HANDLE);
}

VkRuntimeStatus vk_runtime_initialize_instance(VkRuntime *runtime,
                                               const VkRuntimeConfig *config) {
    if (!runtime || !config || !config->application_name ||
        config->requested_api_version == 0u ||
        (config->instance_extension_count > 0u &&
         !config->instance_extensions) ||
        (config->device_extension_count > 0u &&
         !config->device_extensions)) {
        if (runtime) {
            memset(runtime, 0, sizeof(*runtime));
            runtime->report.status = VK_RUNTIME_STATUS_INVALID_ARGUMENT;
            runtime->report.vulkan_result =
                VK_ERROR_INITIALIZATION_FAILED;
            runtime->report.selected_device_index = UINT32_MAX;
        }
        return VK_RUNTIME_STATUS_INVALID_ARGUMENT;
    }

    memset(runtime, 0, sizeof(*runtime));
    initialize_report(&runtime->report, config);

    return vk_runtime_internal_create_instance(runtime, config);
}

VkRuntimeStatus vk_runtime_initialize_device(VkRuntime *runtime,
                                             const VkRuntimeConfig *config,
                                             VkSurfaceKHR presentation_surface) {
    if (!runtime || !config || runtime->instance == VK_NULL_HANDLE ||
        (config->require_present_queue &&
         presentation_surface == VK_NULL_HANDLE)) {
        return VK_RUNTIME_STATUS_INVALID_ARGUMENT;
    }
    VkRuntimeStatus status = enumerate_and_select_device(
        runtime, config, presentation_surface);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }
    status = create_logical_device(runtime, config);
    if (status != VK_RUNTIME_STATUS_OK) {
        return status;
    }

    runtime->report.status = VK_RUNTIME_STATUS_OK;
    runtime->report.vulkan_result = VK_SUCCESS;
    return VK_RUNTIME_STATUS_OK;
}

const VkRuntimeCapabilityReport *
vk_runtime_get_capability_report(const VkRuntime *runtime) {
    return runtime ? &runtime->report : NULL;
}

void vk_runtime_capability_report_destroy(
    VkRuntimeCapabilityReport *report) {
    if (!report) {
        return;
    }
    for (uint32_t i = 0u; i < report->device_count; ++i) {
        destroy_device_capability(&report->devices[i]);
    }
    free(report->devices);
    report->devices = NULL;
    report->device_count = 0u;
    report->selected_device_index = UINT32_MAX;
}

VkRuntimeStatus vk_runtime_close(VkRuntime *runtime) {
    if (!runtime) {
        return VK_RUNTIME_STATUS_INVALID_ARGUMENT;
    }
    if (runtime->device != VK_NULL_HANDLE) {
        VkResult wait_result = vkDeviceWaitIdle(runtime->device);
        if (wait_result != VK_SUCCESS &&
            runtime->report.status == VK_RUNTIME_STATUS_OK) {
            set_failure(&runtime->report,
                        VK_RUNTIME_STATUS_DEVICE_WAIT_FAILED,
                        wait_result);
        }
        vkDestroyDevice(runtime->device, NULL);
        runtime->device = VK_NULL_HANDLE;
        runtime->graphics_queue = VK_NULL_HANDLE;
        runtime->compute_queue = VK_NULL_HANDLE;
        runtime->transfer_queue = VK_NULL_HANDLE;
        runtime->present_queue = VK_NULL_HANDLE;
    }
    vk_runtime_internal_close_instance(runtime);
    runtime->physical_device = VK_NULL_HANDLE;
    return runtime->report.status;
}

void vk_runtime_shutdown(VkRuntime *runtime) {
    if (!runtime) {
        return;
    }
    (void)vk_runtime_close(runtime);
    vk_runtime_capability_report_destroy(&runtime->report);
    memset(runtime, 0, sizeof(*runtime));
}

const char *vk_runtime_status_name(VkRuntimeStatus status) {
    switch (status) {
        case VK_RUNTIME_STATUS_OK:
            return "ok";
        case VK_RUNTIME_STATUS_INVALID_ARGUMENT:
            return "invalid_argument";
        case VK_RUNTIME_STATUS_OUT_OF_HOST_MEMORY:
            return "out_of_host_memory";
        case VK_RUNTIME_STATUS_LOADER_UNAVAILABLE:
            return "loader_unavailable";
        case VK_RUNTIME_STATUS_API_VERSION_UNSUPPORTED:
            return "api_version_unsupported";
        case VK_RUNTIME_STATUS_INSTANCE_EXTENSION_MISSING:
            return "instance_extension_missing";
        case VK_RUNTIME_STATUS_VALIDATION_LAYER_MISSING:
            return "validation_layer_missing";
        case VK_RUNTIME_STATUS_INSTANCE_CREATE_FAILED:
            return "instance_create_failed";
        case VK_RUNTIME_STATUS_DEBUG_MESSENGER_CREATE_FAILED:
            return "debug_messenger_create_failed";
        case VK_RUNTIME_STATUS_NO_PHYSICAL_DEVICE:
            return "no_physical_device";
        case VK_RUNTIME_STATUS_NO_SUITABLE_DEVICE:
            return "no_suitable_device";
        case VK_RUNTIME_STATUS_DEVICE_EXTENSION_MISSING:
            return "device_extension_missing";
        case VK_RUNTIME_STATUS_LOGICAL_DEVICE_CREATE_FAILED:
            return "logical_device_create_failed";
        case VK_RUNTIME_STATUS_DEVICE_WAIT_FAILED:
            return "device_wait_failed";
        case VK_RUNTIME_STATUS_SHADER_CODE_INVALID:
            return "shader_code_invalid";
        case VK_RUNTIME_STATUS_SHADER_MODULE_CREATE_FAILED:
            return "shader_module_create_failed";
        case VK_RUNTIME_STATUS_DESCRIPTOR_BINDING_INVALID:
            return "descriptor_binding_invalid";
        case VK_RUNTIME_STATUS_BUFFER_CREATE_FAILED:
            return "buffer_create_failed";
        case VK_RUNTIME_STATUS_MEMORY_TYPE_UNAVAILABLE:
            return "memory_type_unavailable";
        case VK_RUNTIME_STATUS_MEMORY_ALLOCATE_FAILED:
            return "memory_allocate_failed";
        case VK_RUNTIME_STATUS_MEMORY_MAP_FAILED:
            return "memory_map_failed";
        case VK_RUNTIME_STATUS_DESCRIPTOR_CREATE_FAILED:
            return "descriptor_create_failed";
        case VK_RUNTIME_STATUS_COMPUTE_PIPELINE_CREATE_FAILED:
            return "compute_pipeline_create_failed";
        case VK_RUNTIME_STATUS_COMMAND_CREATE_FAILED:
            return "command_create_failed";
        case VK_RUNTIME_STATUS_COMMAND_SUBMIT_FAILED:
            return "command_submit_failed";
        case VK_RUNTIME_STATUS_FENCE_WAIT_TIMEOUT:
            return "fence_wait_timeout";
        case VK_RUNTIME_STATUS_FENCE_WAIT_FAILED:
            return "fence_wait_failed";
        case VK_RUNTIME_STATUS_RESOURCE_IN_USE:
            return "resource_in_use";
        case VK_RUNTIME_STATUS_BUFFER_RANGE_INVALID:
            return "buffer_range_invalid";
        case VK_RUNTIME_STATUS_DEVICE_LOST:
            return "device_lost";
        case VK_RUNTIME_STATUS_TIMESTAMP_QUERY_CREATE_FAILED:
            return "timestamp_query_create_failed";
        case VK_RUNTIME_STATUS_TIMESTAMP_RESULT_FAILED:
            return "timestamp_result_failed";
        case VK_RUNTIME_STATUS_SERIALIZATION_FAILED:
            return "serialization_failed";
        case VK_RUNTIME_STATUS_INTERNAL_LIMIT_EXCEEDED:
            return "internal_limit_exceeded";
        default:
            return "unknown";
    }
}

const char *vk_runtime_device_type_name(uint32_t device_type) {
    switch ((VkPhysicalDeviceType)device_type) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "other";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "integrated_gpu";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "discrete_gpu";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "virtual_gpu";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "cpu";
        default:
            return "unknown";
    }
}
