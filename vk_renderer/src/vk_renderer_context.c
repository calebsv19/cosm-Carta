#include "vk_renderer_context.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VkSurfaceFormatKHR choose_surface_format(VkPhysicalDevice device,
                                                VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, NULL);
    VkSurfaceFormatKHR* formats =
        (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats);

    VkSurfaceFormatKHR chosen = formats[0];
    for (uint32_t i = 0; i < count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = formats[i];
            break;
        }
    }

    free(formats);
    return chosen;
}

static VkPresentModeKHR choose_present_mode(VkPhysicalDevice device,
                                            VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, NULL);
    VkPresentModeKHR* modes =
        (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes);

#if defined(__APPLE__)
    (void)device;
    (void)surface;
    free(modes);
    return VK_PRESENT_MODE_FIFO_KHR;
#endif

    VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < count; ++i) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosen = modes[i];
            break;
        }
    }

    free(modes);
    return chosen;
}

static VkExtent2D choose_extent(SDL_Window* window,
                                VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);

    VkExtent2D extent = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
    };

    if (extent.width < capabilities.minImageExtent.width)
        extent.width = capabilities.minImageExtent.width;
    if (extent.width > capabilities.maxImageExtent.width)
        extent.width = capabilities.maxImageExtent.width;

    if (extent.height < capabilities.minImageExtent.height)
        extent.height = capabilities.minImageExtent.height;
    if (extent.height > capabilities.maxImageExtent.height)
        extent.height = capabilities.maxImageExtent.height;

    return extent;
}

static void destroy_swapchain(VkRendererContext* ctx) {
    if (!ctx || !ctx->device) return;
    VkDevice device = ctx->device->device;

    if (ctx->swapchain.image_views) {
        for (uint32_t i = 0; i < ctx->swapchain.image_count; ++i) {
            if (ctx->swapchain.image_views[i]) {
                vkDestroyImageView(device, ctx->swapchain.image_views[i], NULL);
            }
        }
        free(ctx->swapchain.image_views);
    }

    if (ctx->swapchain.images) {
        free(ctx->swapchain.images);
    }

    if (ctx->swapchain.handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, ctx->swapchain.handle, NULL);
    }

    ctx->swapchain = (VkRendererSwapchain){0};
}

static VkResult create_swapchain(VkRendererContext* ctx,
                                 SDL_Window* window,
                                 const VkRendererConfig* config) {
    (void)config;
    if (!ctx || !ctx->device) return VK_ERROR_INITIALIZATION_FAILED;

    VkRendererDevice* device = ctx->device;
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device->physical_device, ctx->surface, &capabilities);

    VkSurfaceFormatKHR surface_format =
        choose_surface_format(device->physical_device, ctx->surface);
    VkPresentModeKHR present_mode =
        choose_present_mode(device->physical_device, ctx->surface);
    VkExtent2D extent = choose_extent(window, capabilities);

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = ctx->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    uint32_t queue_family_indices[] = {device->graphics_queue_family,
                                       device->present_queue_family};
    if (device->graphics_queue_family != device->present_queue_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = NULL;
    }

    VkResult result =
        vkCreateSwapchainKHR(device->device, &create_info, NULL, &ctx->swapchain.handle);
    if (result != VK_SUCCESS) return result;

    ctx->swapchain.image_format = surface_format.format;
    ctx->swapchain.extent = extent;

    vkGetSwapchainImagesKHR(device->device, ctx->swapchain.handle, &image_count, NULL);
    ctx->swapchain.image_count = image_count;
    ctx->swapchain.images =
        (VkImage*)malloc(sizeof(VkImage) * ctx->swapchain.image_count);
    vkGetSwapchainImagesKHR(device->device, ctx->swapchain.handle, &image_count,
                            ctx->swapchain.images);

    ctx->swapchain.image_views =
        (VkImageView*)calloc(ctx->swapchain.image_count, sizeof(VkImageView));
    for (uint32_t i = 0; i < ctx->swapchain.image_count; ++i) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = ctx->swapchain.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = ctx->swapchain.image_format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        result = vkCreateImageView(device->device, &view_info, NULL,
                                   &ctx->swapchain.image_views[i]);
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    return VK_SUCCESS;
}

VkResult vk_renderer_context_create(VkRendererContext* ctx,
                                    SDL_Window* window,
                                    const VkRendererConfig* config) {
    if (!ctx || !window || !config) return VK_ERROR_INITIALIZATION_FAILED;
    memset(ctx, 0, sizeof(*ctx));
    ctx->owns_device = VK_TRUE;

    VkResult result = vk_renderer_device_init(&ctx->owned_device, window, config);
    if (result != VK_SUCCESS) {
        memset(ctx, 0, sizeof(*ctx));
        return result;
    }

    ctx->device = &ctx->owned_device;
    result = vk_renderer_context_create_with_device(ctx, ctx->device, window, config);
    if (result != VK_SUCCESS) {
        vk_renderer_device_shutdown(&ctx->owned_device);
        memset(ctx, 0, sizeof(*ctx));
    }

    return result;
}

VkResult vk_renderer_context_create_with_device(VkRendererContext* ctx,
                                                VkRendererDevice* device,
                                                SDL_Window* window,
                                                const VkRendererConfig* config) {
    if (!ctx || !device || !window || !config) return VK_ERROR_INITIALIZATION_FAILED;

    ctx->device = device;
    ctx->surface = VK_NULL_HANDLE;
    ctx->swapchain = (VkRendererSwapchain){0};

    if (!SDL_Vulkan_CreateSurface(window, device->instance, &ctx->surface)) {
        fprintf(stderr, "SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    VkBool32 present_supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device->physical_device,
                                         device->present_queue_family,
                                         ctx->surface,
                                         &present_supported);
    if (!present_supported) {
        vkDestroySurfaceKHR(device->instance, ctx->surface, NULL);
        ctx->surface = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result = create_swapchain(ctx, window, config);
    if (result != VK_SUCCESS) {
        vkDestroySurfaceKHR(device->instance, ctx->surface, NULL);
        ctx->surface = VK_NULL_HANDLE;
        return result;
    }

    return VK_SUCCESS;
}

void vk_renderer_context_destroy(VkRendererContext* ctx) {
    if (!ctx) return;

    if (ctx->device) {
        vk_renderer_device_wait_idle(ctx->device);
    }

    destroy_swapchain(ctx);

    if (ctx->surface != VK_NULL_HANDLE && ctx->device) {
        vkDestroySurfaceKHR(ctx->device->instance, ctx->surface, NULL);
    }

    if (ctx->owns_device) {
        vk_renderer_device_shutdown(&ctx->owned_device);
    }

    memset(ctx, 0, sizeof(*ctx));
}

VkResult vk_renderer_context_recreate_swapchain(VkRendererContext* ctx,
                                                SDL_Window* window,
                                                const VkRendererConfig* config) {
    if (!ctx || !ctx->device) return VK_ERROR_INITIALIZATION_FAILED;
    vk_renderer_device_wait_idle(ctx->device);
    destroy_swapchain(ctx);
    return create_swapchain(ctx, window, config);
}
