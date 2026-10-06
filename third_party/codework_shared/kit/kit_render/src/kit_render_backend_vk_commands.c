#include "kit_render_backend_vk_commands.h"
#include "kit_render_command_validation.h"

#include <math.h>

static void apply_color(VkRenderer *renderer, KitRenderColor color) {
    vk_renderer_set_draw_color(renderer, color.r / 255.0f, color.g / 255.0f,
                                 color.b / 255.0f, color.a / 255.0f);
}

CoreResult kit_render_vk_prepare_command(VkRenderer *renderer, const KitRenderCommand *command) {
    vk_renderer_reset_draw_transform(renderer);
    const KitRenderTransform *transform = kit_render_command_transform(command);
    if (transform && vk_renderer_set_draw_transform(renderer, transform->tx, transform->ty,
                                                     transform->sx, transform->sy) != VK_SUCCESS)
        return (CoreResult){CORE_ERR_INVALID_ARG, "invalid Vulkan draw transform"};
    return core_result_ok();
}

CoreResult kit_render_vk_draw_command(VkRenderer *renderer, const KitRenderFrame *frame,
                                      const KitRenderCommand *command) {
    switch (command->kind) {
        case KIT_RENDER_CMD_CLEAR: {
            SDL_Rect full = {0, 0, (int)frame->width_px, (int)frame->height_px};
            apply_color(renderer, command->data.clear.color);
            vk_renderer_fill_rect(renderer, &full);
            break;
        }
        case KIT_RENDER_CMD_SET_CLIP: {
            /* Clip is in frame coordinates, independent of per-command transforms. */
            KitRenderRect rect = command->data.clip.rect;
            double left = fmin(fmax(floor((double)rect.x), 0), frame->width_px);
            double top = fmin(fmax(floor((double)rect.y), 0), frame->height_px);
            double right = fmin(fmax(ceil((double)rect.x + rect.width), left), frame->width_px);
            double bottom = fmin(fmax(ceil((double)rect.y + rect.height), top), frame->height_px);
            if (rect.width == 0) right = left;
            if (rect.height == 0) bottom = top;
            SDL_Rect clip = {(int)left, (int)top, (int)(right - left), (int)(bottom - top)};
            vk_renderer_set_clip_rect(renderer, &clip);
            break;
        }
        case KIT_RENDER_CMD_CLEAR_CLIP:
            vk_renderer_set_clip_rect(renderer, 0);
            break;
        case KIT_RENDER_CMD_RECT: {
            KitRenderRect rect = command->data.rect.rect;
            SDL_FRect bounds = {rect.x, rect.y, rect.width, rect.height};
            apply_color(renderer, command->data.rect.color);
            vk_renderer_fill_rounded_rect(renderer, &bounds, command->data.rect.corner_radius);
            break;
        }
        case KIT_RENDER_CMD_LINE:
            apply_color(renderer, command->data.line.color);
            vk_renderer_draw_line_thick(renderer, command->data.line.p0.x, command->data.line.p0.y,
                                         command->data.line.p1.x, command->data.line.p1.y,
                                         command->data.line.thickness);
            break;
        case KIT_RENDER_CMD_POLYLINE:
            apply_color(renderer, command->data.polyline.color);
            for (uint32_t i = 1; i < command->data.polyline.point_count; ++i) {
                const KitRenderVec2 *points = command->data.polyline.points;
                vk_renderer_draw_line_thick(renderer, points[i - 1].x, points[i - 1].y,
                                             points[i].x, points[i].y,
                                             command->data.polyline.thickness);
            }
            break;
        case KIT_RENDER_CMD_TEXTURED_QUAD: {
            const KitRenderTexturedQuadCommand *quad = &command->data.textured_quad;
            SDL_FRect bounds = {quad->rect.x, quad->rect.y, quad->rect.width, quad->rect.height};
            float uv_min[2] = {quad->uv_min.x, quad->uv_min.y};
            float uv_max[2] = {quad->uv_max.x, quad->uv_max.y};
            float tint[4] = {quad->tint.r / 255.0f, quad->tint.g / 255.0f,
                             quad->tint.b / 255.0f, quad->tint.a / 255.0f};
            VkResult result = vk_renderer_draw_textured_quad(renderer,
                (const VkRendererTexture *)(uintptr_t)quad->texture_id, &bounds, uv_min, uv_max, tint);
            if (result != VK_SUCCESS)
                return (CoreResult){result == VK_ERROR_OUT_OF_HOST_MEMORY ||
                                   result == VK_ERROR_OUT_OF_DEVICE_MEMORY ? CORE_ERR_OUT_OF_MEMORY : CORE_ERR_IO,
                                   "Vulkan textured quad submission failed"};
            break;
        }
        default:
            return (CoreResult){CORE_ERR_INVALID_ARG, "unknown Vulkan render command"};
    }
    return core_result_ok();
}
