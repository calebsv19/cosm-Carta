#include "vk_renderer_draw_internal.h"

#include <stdlib.h>
#include <string.h>

VkResult vk_renderer_retire_frame_buffer(VkRendererFrameState *frame, VkAllocatedBuffer *buffer) {
    if (frame->retired_vertex_buffer_count == frame->retired_vertex_buffer_capacity) {
        uint32_t capacity = frame->retired_vertex_buffer_capacity + 4;
        VkAllocatedBuffer *buffers = realloc(frame->retired_vertex_buffers,
                                              (size_t)capacity * sizeof(*buffers));
        if (!buffers) return VK_ERROR_OUT_OF_HOST_MEMORY;
        frame->retired_vertex_buffers = buffers;
        frame->retired_vertex_buffer_capacity = capacity;
    }
    frame->retired_vertex_buffers[frame->retired_vertex_buffer_count++] = *buffer;
    memset(buffer, 0, sizeof(*buffer));
    return VK_SUCCESS;
}

void vk_renderer_release_frame_buffers(VkRenderer *renderer, VkRendererFrameState *frame,
                                        int release_storage) {
    for (uint32_t i = 0; i < frame->retired_vertex_buffer_count; ++i)
        vk_renderer_memory_destroy_buffer(&renderer->context, frame->retired_vertex_buffers + i);
    frame->retired_vertex_buffer_count = 0;
    if (release_storage) {
        free(frame->retired_vertex_buffers);
        frame->retired_vertex_buffers = NULL;
        frame->retired_vertex_buffer_capacity = 0;
    }
}
