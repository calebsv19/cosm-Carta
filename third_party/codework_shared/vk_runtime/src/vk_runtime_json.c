#include "vk_runtime.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct JsonWriter {
    char *output;
    size_t capacity;
    size_t length;
    bool pretty;
    bool failed;
    bool overflow;
} JsonWriter;

static void writer_append(JsonWriter *writer, const char *text) {
    if (writer->failed || !text) {
        return;
    }
    size_t text_length = strlen(text);
    if (writer->output && writer->capacity > 0u) {
        if (writer->length + text_length >= writer->capacity) {
            writer->overflow = true;
        } else {
            memcpy(writer->output + writer->length, text, text_length);
        }
    }
    writer->length += text_length;
}

static void writer_append_char(JsonWriter *writer, char value) {
    if (writer->failed) {
        return;
    }
    if (writer->output && writer->capacity > 0u) {
        if (writer->length + 1u >= writer->capacity) {
            writer->overflow = true;
        } else {
            writer->output[writer->length] = value;
        }
    }
    writer->length += 1u;
}

static void writer_append_format(JsonWriter *writer,
                                 const char *format,
                                 ...) {
    if (writer->failed) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    va_list measure_arguments;
    va_copy(measure_arguments, arguments);
    int required = vsnprintf(NULL, 0, format, measure_arguments);
    va_end(measure_arguments);
    if (required < 0) {
        va_end(arguments);
        writer->failed = true;
        return;
    }

    size_t required_size = (size_t)required;
    if (writer->output && writer->capacity > 0u) {
        if (writer->length + required_size >= writer->capacity) {
            writer->overflow = true;
        } else {
            (void)vsnprintf(writer->output + writer->length,
                            writer->capacity - writer->length,
                            format,
                            arguments);
        }
    }
    va_end(arguments);
    writer->length += required_size;
}

static void writer_indent(JsonWriter *writer, uint32_t depth) {
    if (!writer->pretty) {
        return;
    }
    writer_append_char(writer, '\n');
    for (uint32_t i = 0u; i < depth * 2u; ++i) {
        writer_append_char(writer, ' ');
    }
}

static void writer_space(JsonWriter *writer) {
    if (writer->pretty) {
        writer_append_char(writer, ' ');
    }
}

static void writer_json_string(JsonWriter *writer, const char *text) {
    writer_append_char(writer, '"');
    if (!text) {
        text = "";
    }
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor != '\0';
         ++cursor) {
        switch (*cursor) {
            case '"':
                writer_append(writer, "\\\"");
                break;
            case '\\':
                writer_append(writer, "\\\\");
                break;
            case '\b':
                writer_append(writer, "\\b");
                break;
            case '\f':
                writer_append(writer, "\\f");
                break;
            case '\n':
                writer_append(writer, "\\n");
                break;
            case '\r':
                writer_append(writer, "\\r");
                break;
            case '\t':
                writer_append(writer, "\\t");
                break;
            default:
                if (*cursor < 0x20u) {
                    writer_append_format(writer, "\\u%04x", (unsigned)*cursor);
                } else {
                    writer_append_char(writer, (char)*cursor);
                }
                break;
        }
    }
    writer_append_char(writer, '"');
}

static void writer_key(JsonWriter *writer,
                       uint32_t depth,
                       const char *key) {
    writer_indent(writer, depth);
    writer_json_string(writer, key);
    writer_append_char(writer, ':');
    writer_space(writer);
}

static const char *json_bool(bool value) {
    return value ? "true" : "false";
}

static void api_version_text(uint32_t version,
                             char output[VK_RUNTIME_VERSION_TEXT_CAPACITY]) {
    (void)snprintf(output,
                   VK_RUNTIME_VERSION_TEXT_CAPACITY,
                   "%u.%u.%u",
                   (unsigned)VK_API_VERSION_MAJOR(version),
                   (unsigned)VK_API_VERSION_MINOR(version),
                   (unsigned)VK_API_VERSION_PATCH(version));
}

static void write_nullable_queue(JsonWriter *writer, uint32_t queue_family) {
    if (queue_family == VK_RUNTIME_INVALID_QUEUE_FAMILY) {
        writer_append(writer, "null");
    } else {
        writer_append_format(writer, "%u", (unsigned)queue_family);
    }
}

static void write_rejections(JsonWriter *writer,
                             const VkRuntimeDeviceCapability *device,
                             uint32_t depth) {
    writer_append_char(writer, '[');
    bool wrote_value = false;
    if ((device->rejection_bits &
         VK_RUNTIME_DEVICE_REJECT_GRAPHICS_QUEUE_MISSING) != 0u) {
        if (wrote_value) {
            writer_append_char(writer, ',');
            writer_space(writer);
        }
        writer_json_string(writer, "graphics_queue_missing");
        wrote_value = true;
    }
    if ((device->rejection_bits &
         VK_RUNTIME_DEVICE_REJECT_COMPUTE_QUEUE_MISSING) != 0u) {
        if (wrote_value) {
            writer_append_char(writer, ',');
            writer_space(writer);
        }
        writer_json_string(writer, "compute_queue_missing");
        wrote_value = true;
    }
    if ((device->rejection_bits &
         VK_RUNTIME_DEVICE_REJECT_TRANSFER_QUEUE_MISSING) != 0u) {
        if (wrote_value) {
            writer_append_char(writer, ',');
            writer_space(writer);
        }
        writer_json_string(writer, "transfer_queue_missing");
        wrote_value = true;
    }
    if ((device->rejection_bits &
         VK_RUNTIME_DEVICE_REJECT_PRESENT_QUEUE_MISSING) != 0u) {
        if (wrote_value) {
            writer_append_char(writer, ',');
            writer_space(writer);
        }
        writer_json_string(writer, "present_queue_missing");
        wrote_value = true;
    }
    if ((device->rejection_bits &
         VK_RUNTIME_DEVICE_REJECT_DEVICE_EXTENSION_MISSING) != 0u) {
        if (wrote_value) {
            writer_append_char(writer, ',');
            writer_space(writer);
        }
        writer_json_string(writer, "device_extension_missing");
        wrote_value = true;
    }
    if (writer->pretty && wrote_value) {
        (void)depth;
    }
    writer_append_char(writer, ']');
}

static void write_selected_queues(JsonWriter *writer,
                                  const VkRuntimeDeviceCapability *device,
                                  uint32_t depth) {
    writer_append_char(writer, '{');
    writer_key(writer, depth + 1u, "graphics");
    write_nullable_queue(writer, device->graphics_queue_family);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "compute");
    write_nullable_queue(writer, device->compute_queue_family);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "transfer");
    write_nullable_queue(writer, device->transfer_queue_family);
    writer_indent(writer, depth);
    writer_append_char(writer, '}');
}

static void write_features(JsonWriter *writer,
                           const VkRuntimeDeviceCapability *device,
                           uint32_t depth) {
    writer_append_char(writer, '{');
    writer_key(writer, depth + 1u, "sampler_anisotropy");
    writer_append(writer, json_bool(device->sampler_anisotropy));
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "shader_float64");
    writer_append(writer, json_bool(device->shader_float64));
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "shader_int64");
    writer_append(writer, json_bool(device->shader_int64));
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "portability_subset");
    writer_append(writer, json_bool(device->portability_subset));
    writer_indent(writer, depth);
    writer_append_char(writer, '}');
}

static void write_subgroup(JsonWriter *writer,
                           const VkRuntimeDeviceCapability *device,
                           uint32_t depth) {
    writer_append_char(writer, '{');
    writer_key(writer, depth + 1u, "size");
    writer_append_format(writer, "%u", (unsigned)device->subgroup_size);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "supported_stages");
    writer_append_format(
        writer, "%u", (unsigned)device->subgroup_supported_stages);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "supported_operations");
    writer_append_format(
        writer, "%u", (unsigned)device->subgroup_supported_operations);
    writer_indent(writer, depth);
    writer_append_char(writer, '}');
}

static void write_queue_families(JsonWriter *writer,
                                 const VkRuntimeDeviceCapability *device,
                                 uint32_t depth) {
    writer_append_char(writer, '[');
    for (uint32_t i = 0u; i < device->queue_family_count; ++i) {
        const VkRuntimeQueueFamilyCapability *queue =
            &device->queue_families[i];
        if (i > 0u) {
            writer_append_char(writer, ',');
        }
        writer_indent(writer, depth + 1u);
        writer_append_char(writer, '{');
        writer_key(writer, depth + 2u, "index");
        writer_append_format(writer, "%u", (unsigned)queue->index);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "queue_count");
        writer_append_format(writer, "%u", (unsigned)queue->queue_count);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "flags");
        writer_append_format(writer, "%u", (unsigned)queue->queue_flags);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "timestamp_valid_bits");
        writer_append_format(
            writer, "%u", (unsigned)queue->timestamp_valid_bits);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "min_image_transfer_granularity");
        writer_append_format(
            writer,
            "[%u,%u,%u]",
            (unsigned)queue->min_image_transfer_granularity[0],
            (unsigned)queue->min_image_transfer_granularity[1],
            (unsigned)queue->min_image_transfer_granularity[2]);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "graphics");
        writer_append(writer, json_bool(queue->graphics));
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "compute");
        writer_append(writer, json_bool(queue->compute));
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "transfer");
        writer_append(writer, json_bool(queue->transfer));
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "sparse_binding");
        writer_append(writer, json_bool(queue->sparse_binding));
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "protected");
        writer_append(writer, json_bool(queue->protected_queue));
        writer_indent(writer, depth + 1u);
        writer_append_char(writer, '}');
    }
    if (device->queue_family_count > 0u) {
        writer_indent(writer, depth);
    }
    writer_append_char(writer, ']');
}

static void write_memory_heaps(JsonWriter *writer,
                               const VkRuntimeDeviceCapability *device,
                               uint32_t depth) {
    writer_append_char(writer, '[');
    for (uint32_t i = 0u; i < device->memory_heap_count; ++i) {
        const VkRuntimeMemoryHeapCapability *heap =
            &device->memory_heaps[i];
        if (i > 0u) {
            writer_append_char(writer, ',');
        }
        writer_indent(writer, depth + 1u);
        writer_append_char(writer, '{');
        writer_key(writer, depth + 2u, "index");
        writer_append_format(writer, "%u", (unsigned)i);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "size_bytes");
        writer_append_format(writer, "%" PRIu64, heap->size_bytes);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "flags");
        writer_append_format(writer, "%u", (unsigned)heap->flags);
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "device_local");
        writer_append(writer, json_bool(heap->device_local));
        writer_append_char(writer, ',');
        writer_key(writer, depth + 2u, "multi_instance");
        writer_append(writer, json_bool(heap->multi_instance));
        writer_indent(writer, depth + 1u);
        writer_append_char(writer, '}');
    }
    if (device->memory_heap_count > 0u) {
        writer_indent(writer, depth);
    }
    writer_append_char(writer, ']');
}

static void write_extensions(JsonWriter *writer,
                             const VkRuntimeDeviceCapability *device,
                             uint32_t depth) {
    writer_append_char(writer, '[');
    for (uint32_t i = 0u; i < device->extension_count; ++i) {
        if (i > 0u) {
            writer_append_char(writer, ',');
        }
        if (writer->pretty) {
            writer_indent(writer, depth + 1u);
        }
        writer_json_string(writer, device->extensions[i]);
    }
    if (writer->pretty && device->extension_count > 0u) {
        writer_indent(writer, depth);
    }
    writer_append_char(writer, ']');
}

static void write_device(JsonWriter *writer,
                         const VkRuntimeDeviceCapability *device,
                         uint32_t depth) {
    char api_version[VK_RUNTIME_VERSION_TEXT_CAPACITY];
    api_version_text(device->api_version, api_version);

    writer_append_char(writer, '{');
    writer_key(writer, depth + 1u, "name");
    writer_json_string(writer, device->device_name);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "device_uuid");
    writer_json_string(writer, device->device_uuid);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "vendor_id");
    writer_append_format(writer, "%u", (unsigned)device->vendor_id);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "device_id");
    writer_append_format(writer, "%u", (unsigned)device->device_id);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "type");
    writer_json_string(
        writer, vk_runtime_device_type_name(device->device_type));
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "api_version");
    writer_json_string(writer, api_version);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "driver_version");
    writer_append_format(writer, "%u", (unsigned)device->driver_version);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "driver_name");
    writer_json_string(writer, device->driver_name);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "driver_info");
    writer_json_string(writer, device->driver_info);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "selected");
    writer_append(writer, json_bool(device->selected));
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "suitable");
    writer_append(writer, json_bool(device->suitable));
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "rejection_bits");
    writer_append_format(writer, "%u", (unsigned)device->rejection_bits);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "rejections");
    write_rejections(writer, device, depth + 1u);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "selected_queues");
    write_selected_queues(writer, device, depth + 1u);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "features");
    write_features(writer, device, depth + 1u);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "subgroup");
    write_subgroup(writer, device, depth + 1u);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "queue_families");
    write_queue_families(writer, device, depth + 1u);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "memory_heaps");
    write_memory_heaps(writer, device, depth + 1u);
    writer_append_char(writer, ',');
    writer_key(writer, depth + 1u, "extensions");
    write_extensions(writer, device, depth + 1u);
    writer_indent(writer, depth);
    writer_append_char(writer, '}');
}

static void write_report(JsonWriter *writer,
                         const VkRuntimeCapabilityReport *report) {
    char requested_api[VK_RUNTIME_VERSION_TEXT_CAPACITY];
    char loader_api[VK_RUNTIME_VERSION_TEXT_CAPACITY];
    char negotiated_api[VK_RUNTIME_VERSION_TEXT_CAPACITY];
    api_version_text(report->requested_api_version, requested_api);
    api_version_text(report->loader_api_version, loader_api);
    api_version_text(report->negotiated_api_version, negotiated_api);

    writer_append_char(writer, '{');
    writer_key(writer, 1u, "schema");
    writer_json_string(writer, VK_RUNTIME_CAPABILITY_SCHEMA);
    writer_append_char(writer, ',');
    writer_key(writer, 1u, "schema_version");
    writer_append_format(writer, "%u", (unsigned)report->schema_version);
    writer_append_char(writer, ',');
    writer_key(writer, 1u, "module_version");
    writer_json_string(writer, vk_runtime_version_string());
    writer_append_char(writer, ',');
    writer_key(writer, 1u, "platform");
    writer_json_string(writer, report->platform);
    writer_append_char(writer, ',');
    writer_key(writer, 1u, "architecture");
    writer_json_string(writer, report->architecture);
    writer_append_char(writer, ',');
    writer_key(writer, 1u, "compiler");
    writer_json_string(writer, report->compiler);
    writer_append_char(writer, ',');

    writer_key(writer, 1u, "vulkan");
    writer_append_char(writer, '{');
    writer_key(writer, 2u, "header_version");
    writer_append_format(
        writer, "%u", (unsigned)report->vulkan_header_version);
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "requested_api_version");
    writer_json_string(writer, requested_api);
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "loader_api_version");
    writer_json_string(writer, loader_api);
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "negotiated_api_version");
    writer_json_string(writer, negotiated_api);
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "portability_enumeration_available");
    writer_append(
        writer, json_bool(report->portability_enumeration_available));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "portability_enumeration_enabled");
    writer_append(
        writer, json_bool(report->portability_enumeration_enabled));
    writer_indent(writer, 1u);
    writer_append_char(writer, '}');
    writer_append_char(writer, ',');

    writer_key(writer, 1u, "validation");
    writer_append_char(writer, '{');
    writer_key(writer, 2u, "requested");
    writer_append(writer, json_bool(report->validation_requested));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "available");
    writer_append(writer, json_bool(report->validation_available));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "enabled");
    writer_append(writer, json_bool(report->validation_enabled));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "load_failed");
    writer_append(writer, json_bool(report->validation_load_failed));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "debug_utils_available");
    writer_append(writer, json_bool(report->debug_utils_available));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "warnings");
    writer_append_format(
        writer, "%u", (unsigned)report->validation_warning_count);
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "errors");
    writer_append_format(
        writer, "%u", (unsigned)report->validation_error_count);
    writer_indent(writer, 1u);
    writer_append_char(writer, '}');
    writer_append_char(writer, ',');

    writer_key(writer, 1u, "result");
    writer_append_char(writer, '{');
    writer_key(writer, 2u, "status");
    writer_json_string(writer, vk_runtime_status_name(report->status));
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "vulkan_result");
    writer_append_format(writer, "%d", (int)report->vulkan_result);
    writer_append_char(writer, ',');
    writer_key(writer, 2u, "selected_device_index");
    if (report->selected_device_index == UINT32_MAX) {
        writer_append(writer, "null");
    } else {
        writer_append_format(
            writer, "%u", (unsigned)report->selected_device_index);
    }
    writer_indent(writer, 1u);
    writer_append_char(writer, '}');
    writer_append_char(writer, ',');

    writer_key(writer, 1u, "devices");
    writer_append_char(writer, '[');
    for (uint32_t i = 0u; i < report->device_count; ++i) {
        if (i > 0u) {
            writer_append_char(writer, ',');
        }
        writer_indent(writer, 2u);
        write_device(writer, &report->devices[i], 2u);
    }
    if (report->device_count > 0u) {
        writer_indent(writer, 1u);
    }
    writer_append_char(writer, ']');
    writer_indent(writer, 0u);
    writer_append_char(writer, '}');
    if (writer->pretty) {
        writer_append_char(writer, '\n');
    }
}

VkRuntimeStatus vk_runtime_capability_report_to_json(
    const VkRuntimeCapabilityReport *report,
    bool pretty,
    char *output,
    size_t output_capacity,
    size_t *output_size) {
    if (!report || !output_size ||
        (output && output_capacity == 0u)) {
        return VK_RUNTIME_STATUS_INVALID_ARGUMENT;
    }

    JsonWriter writer;
    memset(&writer, 0, sizeof(writer));
    writer.output = output;
    writer.capacity = output_capacity;
    writer.pretty = pretty;
    write_report(&writer, report);

    if (writer.failed || writer.overflow) {
        if (output && output_capacity > 0u) {
            output[0] = '\0';
        }
        *output_size = writer.length + 1u;
        return VK_RUNTIME_STATUS_SERIALIZATION_FAILED;
    }

    *output_size = writer.length + 1u;
    if (output) {
        if (writer.length >= output_capacity) {
            output[0] = '\0';
            return VK_RUNTIME_STATUS_SERIALIZATION_FAILED;
        }
        output[writer.length] = '\0';
    }
    return VK_RUNTIME_STATUS_OK;
}
