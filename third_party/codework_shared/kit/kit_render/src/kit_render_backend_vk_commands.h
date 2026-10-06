#ifndef KIT_RENDER_BACKEND_VK_COMMANDS_H
#define KIT_RENDER_BACKEND_VK_COMMANDS_H

#include "kit_render.h"
#include "vk_renderer.h"

CoreResult kit_render_vk_prepare_command(VkRenderer *renderer, const KitRenderCommand *command);
CoreResult kit_render_vk_draw_command(VkRenderer *renderer, const KitRenderFrame *frame,
                                      const KitRenderCommand *command);

#endif
