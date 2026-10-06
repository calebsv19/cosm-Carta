#include "kit_render_command_validation.h"

#include <float.h>
#include <limits.h>
#include <math.h>

static CoreResult invalid(void) {
    return (CoreResult){CORE_ERR_INVALID_ARG, "invalid render command geometry or transform"};
}

const KitRenderTransform *kit_render_command_transform(const KitRenderCommand *command) {
    if (!command) return 0;
    switch (command->kind) {
        case KIT_RENDER_CMD_RECT: return &command->data.rect.transform;
        case KIT_RENDER_CMD_LINE: return &command->data.line.transform;
        case KIT_RENDER_CMD_POLYLINE: return &command->data.polyline.transform;
        case KIT_RENDER_CMD_TEXTURED_QUAD: return &command->data.textured_quad.transform;
        case KIT_RENDER_CMD_TEXT: return &command->data.text.transform;
        default: return 0;
    }
}

static int finite_float(double value) {
    return isfinite(value) && value >= -FLT_MAX && value <= FLT_MAX;
}

static int valid_point(KitRenderVec2 point, const KitRenderTransform *transform) {
    if (!isfinite(point.x) || !isfinite(point.y)) return 0;
    if (!transform) return 1;
    double x = (double)point.x * transform->sx;
    double y = (double)point.y * transform->sy;
    return finite_float(x) && finite_float(y) &&
           finite_float(x + transform->tx) && finite_float(y + transform->ty);
}

static int valid_rect(KitRenderRect rect, const KitRenderTransform *transform) {
    double right = (double)rect.x + rect.width;
    double bottom = (double)rect.y + rect.height;
    return isfinite(rect.width) && isfinite(rect.height) &&
           rect.width >= 0 && rect.height >= 0 && finite_float(right) && finite_float(bottom) &&
           valid_point((KitRenderVec2){rect.x, rect.y}, transform) &&
           valid_point((KitRenderVec2){(float)right, (float)bottom}, transform);
}

CoreResult kit_render_validate_command(const KitRenderCommand *command) {
    if (!command) return invalid();
    const KitRenderTransform *transform = kit_render_command_transform(command);
    if (transform && (!isfinite(transform->tx) || !isfinite(transform->ty) ||
                      !isfinite(transform->sx) || !isfinite(transform->sy))) return invalid();
    switch (command->kind) {
        case KIT_RENDER_CMD_CLEAR:
        case KIT_RENDER_CMD_CLEAR_CLIP:
            break;
        case KIT_RENDER_CMD_SET_CLIP: {
            KitRenderRect rect = command->data.clip.rect;
            if (!valid_rect(rect, 0) || fabs((double)rect.x) > INT_MAX ||
                fabs((double)rect.y) > INT_MAX || rect.width > (double)INT_MAX ||
                rect.height > (double)INT_MAX ||
                fabs((double)rect.x + rect.width) > INT_MAX ||
                fabs((double)rect.y + rect.height) > INT_MAX) return invalid();
            break;
        }
        case KIT_RENDER_CMD_RECT:
            if (!valid_rect(command->data.rect.rect, transform) ||
                !isfinite(command->data.rect.corner_radius) ||
                command->data.rect.corner_radius < 0) return invalid();
            break;
        case KIT_RENDER_CMD_LINE:
            if (!valid_point(command->data.line.p0, transform) ||
                !valid_point(command->data.line.p1, transform) ||
                !isfinite(command->data.line.thickness) || command->data.line.thickness <= 0)
                return invalid();
            break;
        case KIT_RENDER_CMD_POLYLINE:
            if (!command->data.polyline.points || command->data.polyline.point_count < 2 ||
                !isfinite(command->data.polyline.thickness) || command->data.polyline.thickness <= 0)
                return invalid();
            for (uint32_t i = 0; i < command->data.polyline.point_count; ++i)
                if (!valid_point(command->data.polyline.points[i], transform)) return invalid();
            break;
        case KIT_RENDER_CMD_TEXTURED_QUAD:
            if (!command->data.textured_quad.texture_id ||
                !valid_rect(command->data.textured_quad.rect, transform) ||
                !valid_point(command->data.textured_quad.uv_min, 0) ||
                !valid_point(command->data.textured_quad.uv_max, 0)) return invalid();
            break;
        case KIT_RENDER_CMD_TEXT:
            if (!command->data.text.text || !command->data.text.text[0] ||
                !valid_point(command->data.text.origin, transform)) return invalid();
            break;
        default:
            return invalid();
    }
    return core_result_ok();
}

CoreResult kit_render_validate_frame(const KitRenderFrame *frame) {
    if (!frame || !frame->width_px || !frame->height_px ||
        frame->width_px > INT_MAX || frame->height_px > INT_MAX ||
        !frame->command_buffer || !frame->command_buffer->commands ||
        frame->command_buffer->count > frame->command_buffer->capacity) return invalid();
    for (size_t i = 0; i < frame->command_buffer->count; ++i) {
        CoreResult result = kit_render_validate_command(&frame->command_buffer->commands[i]);
        if (result.code != CORE_OK) return result;
    }
    return core_result_ok();
}
