#ifndef KIT_RENDER_COMMAND_VALIDATION_H
#define KIT_RENDER_COMMAND_VALIDATION_H

#include "kit_render.h"

const KitRenderTransform *kit_render_command_transform(const KitRenderCommand *command);
CoreResult kit_render_validate_command(const KitRenderCommand *command);
CoreResult kit_render_validate_frame(const KitRenderFrame *frame);

#endif
