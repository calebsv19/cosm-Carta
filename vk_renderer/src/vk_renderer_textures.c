#include "vk_renderer_textures.h"
#include "vk_renderer.h"
#include "vk_renderer_memory.h"

#include <string.h>

static VkCommandBuffer begin_single_time_commands(VkRenderer* renderer) {
    VkRendererDevice* device = renderer->context.device;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = renderer->command_pool.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device->device, &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &begin_info);
    return cmd;
}

static void end_single_time_commands(VkRenderer* renderer, VkCommandBuffer cmd) {
    VkRendererDevice* device = renderer->context.device;
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    vkQueueSubmit(device->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->graphics_queue);

    vkFreeCommandBuffers(device->device, renderer->command_pool.pool, 1, &cmd);
}

static void transition_image_layout(VkRenderer* renderer,
                                    VkImage image,
                                    VkFormat format,
                                    VkImageLayout old_layout,
                                    VkImageLayout new_layout) {
    (void)format;
    VkCommandBuffer cmd = begin_single_time_commands(renderer);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
               new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
               new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else {
        end_single_time_commands(renderer, cmd);
        return;
    }

    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
    end_single_time_commands(renderer, cmd);
}

static void copy_buffer_to_image(VkRenderer* renderer,
                                 VkBuffer buffer,
                                 VkImage image,
                                 uint32_t width,
                                 uint32_t height) {
    VkCommandBuffer cmd = begin_single_time_commands(renderer);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    end_single_time_commands(renderer, cmd);
}

static void copy_buffer_to_image_subrect(VkRenderer* renderer,
                                         VkBuffer buffer,
                                         VkImage image,
                                         uint32_t x,
                                         uint32_t y,
                                         uint32_t width,
                                         uint32_t height) {
    VkCommandBuffer cmd = begin_single_time_commands(renderer);

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {(int32_t)x, (int32_t)y, 0},
        .imageExtent = {width, height, 1},
    };

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);
    end_single_time_commands(renderer, cmd);
}

VkResult vk_renderer_texture_create_from_rgba(VkRenderer* renderer,
                                              const void* pixels,
                                              uint32_t width,
                                              uint32_t height,
                                              VkFilter filter,
                                              VkRendererTexture* out_texture) {
    // Guard against zero-sized textures that can crash Vulkan backends.
    if (width == 0 || height == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!renderer || !renderer->context.device || !pixels || !out_texture) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    memset(out_texture, 0, sizeof(*out_texture));
    VkDevice device = renderer->context.device->device;

    VkDeviceSize image_size = (VkDeviceSize)width * (VkDeviceSize)height * 4u;

    VkAllocatedBuffer staging = {0};
    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging);
    if (result != VK_SUCCESS) return result;

    memcpy(staging.mapped, pixels, (size_t)image_size);

    VkExtent3D extent = {
        .width = width,
        .height = height,
        .depth = 1,
    };

    result = vk_renderer_memory_create_image(&renderer->context, extent, VK_FORMAT_R8G8B8A8_UNORM,
                                             VK_IMAGE_TILING_OPTIMAL,
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                 VK_IMAGE_USAGE_SAMPLED_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             &out_texture->image);
    if (result != VK_SUCCESS) {
        vk_renderer_memory_destroy_buffer(&renderer->context, &staging);
        return result;
    }

    transition_image_layout(renderer, out_texture->image.image, out_texture->image.format,
                            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(renderer, staging.buffer, out_texture->image.image, width, height);
    transition_image_layout(renderer, out_texture->image.image, out_texture->image.format,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };

    result = vkCreateSampler(device, &sampler_info, NULL,
                             &out_texture->sampler);
    if (result != VK_SUCCESS) {
        vk_renderer_memory_destroy_image(&renderer->context, &out_texture->image);
        vk_renderer_memory_destroy_buffer(&renderer->context, &staging);
        return result;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = renderer->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &renderer->sampler_set_layout,
    };

    result = vkAllocateDescriptorSets(device, &alloc_info,
                                      &out_texture->descriptor_set);
    if (result != VK_SUCCESS) {
        vkDestroySampler(device, out_texture->sampler, NULL);
        out_texture->sampler = VK_NULL_HANDLE;
        vk_renderer_memory_destroy_image(&renderer->context, &out_texture->image);
        vk_renderer_memory_destroy_buffer(&renderer->context, &staging);
        return result;
    }

    VkDescriptorImageInfo image_info = {
        .sampler = out_texture->sampler,
        .imageView = out_texture->image.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = out_texture->descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_info,
    };

    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    out_texture->current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    out_texture->width = width;
    out_texture->height = height;

    vk_renderer_memory_destroy_buffer(&renderer->context, &staging);
    return VK_SUCCESS;
}

VkResult vk_renderer_texture_update_rgba_subrect(VkRenderer* renderer,
                                                 VkRendererTexture* texture,
                                                 const void* pixels,
                                                 size_t row_stride_bytes,
                                                 uint32_t x,
                                                 uint32_t y,
                                                 uint32_t width,
                                                 uint32_t height) {
    if (!renderer || !renderer->context.device || !texture || !pixels) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (width == 0 || height == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (!texture->image.image || !texture->image.view) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (x >= texture->width || y >= texture->height) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (width > (texture->width - x) || height > (texture->height - y)) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    size_t min_row_stride = (size_t)width * 4u;
    if (row_stride_bytes < min_row_stride) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDeviceSize image_size = (VkDeviceSize)min_row_stride * (VkDeviceSize)height;
    VkAllocatedBuffer staging = {0};
    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging);
    if (result != VK_SUCCESS) return result;

    const uint8_t* src = (const uint8_t*)pixels;
    uint8_t* dst = (uint8_t*)staging.mapped;
    for (uint32_t row = 0; row < height; ++row) {
        memcpy(dst + ((size_t)row * min_row_stride),
               src + ((size_t)row * row_stride_bytes),
               min_row_stride);
    }

    transition_image_layout(renderer,
                            texture->image.image,
                            texture->image.format,
                            texture->current_layout,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image_subrect(renderer, staging.buffer, texture->image.image, x, y, width,
                                 height);
    transition_image_layout(renderer,
                            texture->image.image,
                            texture->image.format,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    texture->current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vk_renderer_memory_destroy_buffer(&renderer->context, &staging);
    return VK_SUCCESS;
}

void vk_renderer_texture_destroy(VkRenderer* renderer,
                                 VkRendererTexture* texture) {
    if (!renderer || !renderer->context.device || !texture) return;
    VkDevice device = renderer->context.device->device;
    if (texture->descriptor_set) {
        vkFreeDescriptorSets(device, renderer->descriptor_pool, 1,
                             &texture->descriptor_set);
        texture->descriptor_set = VK_NULL_HANDLE;
    }
    if (texture->sampler) {
        vkDestroySampler(device, texture->sampler, NULL);
        texture->sampler = VK_NULL_HANDLE;
    }
    vk_renderer_memory_destroy_image(&renderer->context, &texture->image);
    memset(texture, 0, sizeof(*texture));
}
