#include "render/renderer.h"
#include "ui/font.h"
#include "core_io.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(MAPFORGE_HAVE_VK)
#include <vulkan/vulkan.h>
#include "vk_renderer.h"
#include "vk_renderer_config.h"
#endif

#if defined(MAPFORGE_HAVE_VK)
static void renderer_log_vk_failure(const Renderer *renderer, const char *phase, VkResult result) {
    if (!renderer || !phase) {
        return;
    }
    SDL_Log("[renderer] Vulkan %s failure=%d streak=%u total_fail=%llu "
            "metrics(lines=%u geom_used=%u/%u geom_skip=%u rect=%u fill=%u)",
            phase,
            (int)result,
            renderer->vk_begin_fail_streak,
            (unsigned long long)renderer->vk_begin_failures_total,
            renderer->vk_lines_drawn,
            renderer->vk_geom_used,
            renderer->vk_geom_budget,
            renderer->vk_geom_budget_skips,
            renderer->vk_rects_drawn,
            renderer->vk_rects_filled);

    if (result == VK_ERROR_DEVICE_LOST) {
        SDL_Log("[renderer] Vulkan device lost likely from command pressure or driver reset. "
                "Tune down road/poly draw density or split rendering into fewer GPU commands per frame.");
    }
}

static void renderer_disable_vulkan(Renderer *renderer, const char *reason) {
    if (!renderer) {
        return;
    }
    bool lost_device = (reason && strstr(reason, "device-lost") != NULL);
    if (renderer->vk && !lost_device) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        ui_font_invalidate_cache(renderer);
        vk_renderer_shutdown(vk);
        SDL_free(vk);
    }
    // On device-lost, avoid blocking teardown paths; leave cleanup to process exit.
    renderer->vk = NULL;
    renderer->vk_cmd = 0;
    renderer->vulkan_available = false;
    SDL_Log("[renderer] Vulkan disabled after fatal failure (%s). No backend fallback is active.",
            reason ? reason : "unknown");
}
#endif

void renderer_set_backend(Renderer *renderer, RendererBackend backend) {
    if (!renderer) {
        return;
    }
    renderer->backend = backend;
}

RendererBackend renderer_get_backend(const Renderer *renderer) {
    if (!renderer) {
        return RENDERER_BACKEND_SDL;
    }
    return renderer->backend;
}

const char *renderer_backend_name(RendererBackend backend) {
    switch (backend) {
        case RENDERER_BACKEND_VULKAN:
            return "vulkan";
        case RENDERER_BACKEND_SDL:
        default:
            return "sdl";
    }
}

static const char *renderer_shader_root(void) {
    const char *runtime_root = getenv("VK_RENDERER_SHADER_ROOT");
    if (runtime_root && runtime_root[0] != '\0') {
        return runtime_root;
    }
#if defined(VK_RENDERER_SHADER_ROOT)
    return VK_RENDERER_SHADER_ROOT;
#else
    return NULL;
#endif
}

bool renderer_init(Renderer *renderer, SDL_Window *window, int width, int height) {
    if (!renderer || !window) {
        return false;
    }

    if (renderer->backend != RENDERER_BACKEND_VULKAN) {
        renderer->backend = RENDERER_BACKEND_SDL;
    }
    renderer->vk = NULL;
    renderer->vk_cmd = 0;
    renderer->window = window;
    renderer->vulkan_available = false;
    renderer->vk_geom_budget = 24000;
    renderer->vk_geom_used = 0;
    renderer->vk_lines_drawn = 0;
    renderer->vk_line_budget = 18000;
    renderer->vk_line_budget_skips = 0;
    renderer->vk_rects_drawn = 0;
    renderer->vk_rects_filled = 0;
    renderer->vk_geom_calls = 0;
    renderer->vk_geom_budget_skips = 0;
    renderer->vk_swapchain_recreates = 0;
    renderer->vk_begin_failures_total = 0;
    renderer->vk_begin_fail_streak = 0;
    renderer->vk_last_begin_result = 0;

#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN) {
        SDL_Log("[renderer] attempting Vulkan init");
        const char *shader_root = renderer_shader_root();
        static const char *kRequiredShaders[] = {
            "fill.vert.spv", "fill.frag.spv", "line.vert.spv", "line.frag.spv", "textured.vert.spv", "textured.frag.spv"
        };
        char shader_path[1024];
        if (!shader_root || shader_root[0] == '\0') {
            SDL_Log("[renderer] Vulkan shader root is unset; falling back to SDL renderer");
            renderer->backend = RENDERER_BACKEND_SDL;
        }
        for (size_t i = 0; i < sizeof(kRequiredShaders) / sizeof(kRequiredShaders[0]); ++i) {
            if (renderer->backend != RENDERER_BACKEND_VULKAN) {
                break;
            }
            snprintf(shader_path, sizeof(shader_path), "%s/shaders/%s", shader_root, kRequiredShaders[i]);
            if (!core_io_path_exists(shader_path)) {
                SDL_Log("[renderer] Vulkan shader missing: %s", shader_path);
                SDL_Log("[renderer] Falling back to SDL renderer");
                renderer->backend = RENDERER_BACKEND_SDL;
                break;
            }
        }
    }
    if (renderer->backend == RENDERER_BACKEND_VULKAN) {
        VkRenderer *vk = SDL_calloc(1, sizeof(*vk));
        if (vk) {
            VkRendererConfig cfg;
            vk_renderer_config_set_defaults(&cfg);
            cfg.enable_validation = SDL_FALSE;
            VkResult init_result = vk_renderer_init(vk, window, &cfg);
            if (init_result == VK_SUCCESS) {
                SDL_Log("[renderer] Vulkan init succeeded");
                renderer->vk = vk;
                renderer->vulkan_available = true;
                renderer->sdl = NULL;
                renderer->width = width;
                renderer->height = height;
                return true;
            }
            SDL_Log("[renderer] Vulkan init failed with code %d; falling back to SDL", (int)init_result);
            vk_renderer_shutdown(vk);
            SDL_free(vk);
            renderer->vk = NULL;
        } else {
            SDL_Log("[renderer] Vulkan allocation failed; falling back to SDL");
        }
    }
#endif

    // SDL fallback path.
    renderer->backend = RENDERER_BACKEND_SDL;
    renderer->sdl = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer->sdl) {
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer->sdl, SDL_BLENDMODE_BLEND);

    renderer->width = width;
    renderer->height = height;
    return true;
}

void renderer_shutdown(Renderer *renderer) {
    if (!renderer) {
        return;
    }

    if (renderer->sdl) {
        SDL_DestroyRenderer(renderer->sdl);
        renderer->sdl = NULL;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->vk) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        vk_renderer_shutdown(vk);
        SDL_free(vk);
    }
#endif
    renderer->vk = NULL;
    renderer->vk_cmd = 0;
    renderer->window = NULL;
    renderer->vulkan_available = false;
}

void renderer_begin_frame(Renderer *renderer) {
    if (!renderer) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vulkan_available && renderer->vk && renderer->window) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        vk_renderer_set_logical_size(vk, (float)renderer->width, (float)renderer->height);

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkExtent2D extent = {0};
        VkResult result = vk_renderer_begin_frame(vk, &cmd, &fb, &extent);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            if (vk_renderer_recreate_swapchain(vk, renderer->window) == VK_SUCCESS) {
                renderer->vk_swapchain_recreates += 1;
                result = vk_renderer_begin_frame(vk, &cmd, &fb, &extent);
            }
        }
        renderer->vk_last_begin_result = (int)result;
        if (result == VK_SUCCESS) {
            renderer->vk_begin_fail_streak = 0;
            renderer->vk_geom_used = 0;
            renderer->vk_lines_drawn = 0;
            renderer->vk_line_budget_skips = 0;
            renderer->vk_rects_drawn = 0;
            renderer->vk_rects_filled = 0;
            renderer->vk_geom_calls = 0;
            renderer->vk_geom_budget_skips = 0;
        } else {
            renderer->vk_begin_failures_total += 1;
            renderer->vk_begin_fail_streak += 1;
            renderer_log_vk_failure(renderer, "begin_frame", result);
            if (result == VK_ERROR_DEVICE_LOST) {
                renderer_disable_vulkan(renderer, "device-lost at begin_frame");
            } else if ((renderer->vk_begin_fail_streak % 60u) == 0u) {
                SDL_Log("[renderer] Vulkan still failing after %u consecutive begin_frame attempts",
                        renderer->vk_begin_fail_streak);
            }
        }
        renderer->vk_cmd = (result == VK_SUCCESS) ? (uintptr_t)cmd : 0;
        return;
    }
#endif
}

void renderer_clear(Renderer *renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer) {
        return;
    }

#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        SDL_Rect full = {0, 0, renderer->width, renderer->height};
        vk_renderer_set_draw_color(vk, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
        vk_renderer_fill_rect(vk, &full);
        return;
    }
#endif

    if (!renderer->sdl) {
        return;
    }
    SDL_SetRenderDrawColor(renderer->sdl, r, g, b, a);
    SDL_RenderClear(renderer->sdl);
}

void renderer_end_frame(Renderer *renderer) {
    if (!renderer) {
        return;
    }

#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        VkCommandBuffer cmd = (VkCommandBuffer)renderer->vk_cmd;
        VkResult result = vk_renderer_end_frame(vk, cmd);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            (void)vk_renderer_recreate_swapchain(vk, renderer->window);
            renderer->vk_swapchain_recreates += 1;
        } else if (result == VK_ERROR_DEVICE_LOST) {
            renderer->vk_begin_failures_total += 1;
            renderer->vk_begin_fail_streak += 1;
            renderer_log_vk_failure(renderer, "end_frame", result);
            renderer_disable_vulkan(renderer, "device-lost at end_frame");
        }
        renderer->vk_cmd = 0;
        return;
    }
#endif

    if (!renderer->sdl) {
        return;
    }
    SDL_RenderPresent(renderer->sdl);
}

void renderer_set_draw_color(Renderer *renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!renderer) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        vk_renderer_set_draw_color(vk, (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
        return;
    }
#endif
    if (!renderer->sdl) {
        return;
    }
    SDL_SetRenderDrawColor(renderer->sdl, r, g, b, a);
}

void renderer_draw_line(Renderer *renderer, float x0, float y0, float x1, float y1) {
    if (!renderer) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        if (renderer->vk_line_budget > 0 && renderer->vk_lines_drawn >= renderer->vk_line_budget) {
            renderer->vk_line_budget_skips += 1;
            return;
        }
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        vk_renderer_draw_line(vk, x0, y0, x1, y1);
        renderer->vk_lines_drawn += 1;
        return;
    }
#endif
    if (!renderer->sdl) {
        return;
    }
    SDL_RenderDrawLineF(renderer->sdl, x0, y0, x1, y1);
}

void renderer_draw_lines(Renderer *renderer, const SDL_FPoint *points, int count) {
    if (!renderer || !points || count < 2) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        int segments = count - 1;
        if (segments <= 0) {
            return;
        }
        if (renderer->vk_line_budget > 0 && renderer->vk_lines_drawn >= renderer->vk_line_budget) {
            renderer->vk_line_budget_skips += (uint32_t)segments;
            return;
        }
        if (renderer->vk_line_budget > 0) {
            uint32_t remaining = renderer->vk_line_budget - renderer->vk_lines_drawn;
            if ((uint32_t)segments > remaining) {
                renderer->vk_line_budget_skips += (uint32_t)segments;
                return;
            }
        }
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        vk_renderer_draw_line_strip(vk, points, (uint32_t)count);
        renderer->vk_lines_drawn += (uint32_t)segments;
        return;
    }
#endif
    if (!renderer->sdl) {
        return;
    }
    SDL_RenderDrawLinesF(renderer->sdl, points, count);
}

static SDL_Rect renderer_rectf_to_rect(const SDL_FRect *rect) {
    SDL_Rect out = {0, 0, 0, 0};
    if (!rect) {
        return out;
    }
    out.x = (int)lroundf(rect->x);
    out.y = (int)lroundf(rect->y);
    out.w = (int)lroundf(rect->w);
    out.h = (int)lroundf(rect->h);
    if (out.w < 0) {
        out.w = 0;
    }
    if (out.h < 0) {
        out.h = 0;
    }
    return out;
}

void renderer_draw_rect(Renderer *renderer, const SDL_FRect *rect) {
    if (!renderer || !rect) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        SDL_Rect irect = renderer_rectf_to_rect(rect);
        vk_renderer_draw_rect(vk, &irect);
        renderer->vk_rects_drawn += 1;
        return;
    }
#endif
    if (!renderer->sdl) {
        return;
    }
    SDL_RenderDrawRectF(renderer->sdl, rect);
}

void renderer_fill_rect(Renderer *renderer, const SDL_FRect *rect) {
    if (!renderer || !rect) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        VkRenderer *vk = (VkRenderer *)renderer->vk;
        SDL_Rect irect = renderer_rectf_to_rect(rect);
        vk_renderer_fill_rect(vk, &irect);
        renderer->vk_rects_filled += 1;
        return;
    }
#endif
    if (!renderer->sdl) {
        return;
    }
    SDL_RenderFillRectF(renderer->sdl, rect);
}

void renderer_draw_geometry(Renderer *renderer,
                            const SDL_Vertex *vertices,
                            int num_vertices,
                            const int *indices,
                            int num_indices) {
    if (!renderer || !vertices || num_vertices < 3) {
        return;
    }

#if defined(MAPFORGE_HAVE_VK)
    if (renderer->backend == RENDERER_BACKEND_VULKAN && renderer->vk && renderer->vk_cmd != 0) {
        renderer->vk_geom_calls += 1;
        uint32_t units = (uint32_t)((indices && num_indices > 0) ? num_indices : num_vertices);
        if (renderer->vk_geom_used >= renderer->vk_geom_budget ||
            units > (renderer->vk_geom_budget - renderer->vk_geom_used)) {
            renderer->vk_geom_budget_skips += 1;
            return;
        }
        renderer->vk_geom_used += units;
        if (indices && num_indices >= 3) {
            for (int i = 0; i + 2 < num_indices; i += 3) {
                int i0 = indices[i];
                int i1 = indices[i + 1];
                int i2 = indices[i + 2];
                if (i0 < 0 || i0 >= num_vertices || i1 < 0 || i1 >= num_vertices || i2 < 0 || i2 >= num_vertices) {
                    continue;
                }
                SDL_Color c = vertices[i0].color;
                renderer_set_draw_color(renderer, c.r, c.g, c.b, c.a);
                renderer_draw_line(renderer,
                                   vertices[i0].position.x, vertices[i0].position.y,
                                   vertices[i1].position.x, vertices[i1].position.y);
                renderer_draw_line(renderer,
                                   vertices[i1].position.x, vertices[i1].position.y,
                                   vertices[i2].position.x, vertices[i2].position.y);
                renderer_draw_line(renderer,
                                   vertices[i2].position.x, vertices[i2].position.y,
                                   vertices[i0].position.x, vertices[i0].position.y);
            }
        } else {
            for (int i = 0; i + 2 < num_vertices; i += 3) {
                SDL_Color c = vertices[i].color;
                renderer_set_draw_color(renderer, c.r, c.g, c.b, c.a);
                renderer_draw_line(renderer,
                                   vertices[i].position.x, vertices[i].position.y,
                                   vertices[i + 1].position.x, vertices[i + 1].position.y);
                renderer_draw_line(renderer,
                                   vertices[i + 1].position.x, vertices[i + 1].position.y,
                                   vertices[i + 2].position.x, vertices[i + 2].position.y);
                renderer_draw_line(renderer,
                                   vertices[i + 2].position.x, vertices[i + 2].position.y,
                                   vertices[i].position.x, vertices[i].position.y);
            }
        }
        return;
    }
#endif

    if (!renderer->sdl) {
        return;
    }
    SDL_RenderGeometry(renderer->sdl, NULL, vertices, num_vertices, indices, num_indices);
}
