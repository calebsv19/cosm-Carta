#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "kit_graph_struct.h"
#include "kit_ui.h"
#include "vk_renderer.h"

#define VALIDATION_MAX_NODES 12u

typedef struct ValidationFixture {
    const char *name;
    const char *summary;
    const KitGraphStructNode *nodes;
    uint32_t node_count;
    const KitGraphStructEdge *edges;
    uint32_t edge_count;
} ValidationFixture;

static const KitGraphStructNode k_nodes_tree[7] = {
    {1u, "Root"},
    {2u, "Alpha"},
    {3u, "Beta"},
    {4u, "Gamma"},
    {5u, "Delta"},
    {6u, "Epsilon"},
    {7u, "Zeta"}
};

static const KitGraphStructEdge k_edges_tree[6] = {
    {1u, 2u}, {1u, 3u}, {2u, 4u}, {2u, 5u}, {3u, 6u}, {3u, 7u}
};

static const KitGraphStructNode k_nodes_dag[7] = {
    {1u, "Root"},
    {2u, "Alpha"},
    {3u, "Beta"},
    {4u, "Gamma"},
    {5u, "Delta"},
    {6u, "Epsilon"},
    {7u, "Zeta"}
};

static const KitGraphStructEdge k_edges_dag[8] = {
    {1u, 2u}, {1u, 3u}, {2u, 4u}, {2u, 5u},
    {3u, 5u}, {3u, 6u}, {4u, 7u}, {5u, 7u}
};

static const KitGraphStructNode k_nodes_chain[8] = {
    {1u, "A1"},
    {2u, "A2"},
    {3u, "A3"},
    {4u, "A4"},
    {5u, "A5"},
    {6u, "A6"},
    {7u, "A7"},
    {8u, "A8"}
};

static const KitGraphStructEdge k_edges_chain[7] = {
    {1u, 2u}, {2u, 3u}, {3u, 4u}, {4u, 5u}, {5u, 6u}, {6u, 7u}, {7u, 8u}
};

static const KitGraphStructNode k_nodes_star[8] = {
    {1u, "Hub"},
    {2u, "S1"},
    {3u, "S2"},
    {4u, "S3"},
    {5u, "S4"},
    {6u, "S5"},
    {7u, "S6"},
    {8u, "S7"}
};

static const KitGraphStructEdge k_edges_star[10] = {
    {1u, 2u}, {1u, 3u}, {1u, 4u}, {1u, 5u}, {1u, 6u}, {1u, 7u}, {1u, 8u},
    {2u, 3u}, {4u, 5u}, {6u, 7u}
};

static const KitGraphStructNode k_nodes_cycle[6] = {
    {1u, "C1"},
    {2u, "C2"},
    {3u, "C3"},
    {4u, "C4"},
    {5u, "C5"},
    {6u, "C6"}
};

static const KitGraphStructEdge k_edges_cycle[10] = {
    {1u, 2u}, {2u, 3u}, {3u, 4u}, {4u, 5u}, {5u, 6u}, {6u, 1u},
    {1u, 4u}, {2u, 5u}, {3u, 6u}, {6u, 3u}
};

static const KitGraphStructNode k_nodes_mixed[9] = {
    {1u, "Spec"},
    {2u, "Design"},
    {3u, "Impl"},
    {4u, "Test"},
    {5u, "Fix"},
    {6u, "Review"},
    {7u, "Ship"},
    {8u, "Docs"},
    {9u, "Post"}
};

static const KitGraphStructEdge k_edges_mixed[13] = {
    {1u, 2u}, {2u, 3u}, {3u, 4u}, {4u, 5u}, {5u, 3u},
    {4u, 6u}, {6u, 7u}, {3u, 8u}, {8u, 7u}, {2u, 8u},
    {7u, 9u}, {1u, 6u}, {6u, 5u}
};

static const ValidationFixture k_fixtures[] = {
    {"TREE", "Baseline branching fixture.", k_nodes_tree, 7u, k_edges_tree, 6u},
    {"DAG", "Cross-links for depth and ordering stress.", k_nodes_dag, 7u, k_edges_dag, 8u},
    {"CHAIN", "Linear dependency ladder.", k_nodes_chain, 8u, k_edges_chain, 7u},
    {"STAR", "Hub-and-spoke with light cross edges.", k_nodes_star, 8u, k_edges_star, 10u},
    {"CYCLE", "Dense cyclic graph stress fixture.", k_nodes_cycle, 6u, k_edges_cycle, 10u},
    {"MIXED", "Workflow-style mixed directional neighborhood.", k_nodes_mixed, 9u, k_edges_mixed, 13u}
};

static const ValidationFixture *active_fixture(int fixture_index, int *out_fixture_index) {
    int index = fixture_index;
    int count = (int)(sizeof(k_fixtures) / sizeof(k_fixtures[0]));

    if (count <= 0) {
        return 0;
    }

    if (index < 0) {
        index = 0;
    }
    if (index >= count) {
        index = count - 1;
    }

    if (out_fixture_index) {
        *out_fixture_index = index;
    }
    return &k_fixtures[index];
}

static const char *selected_label(const ValidationFixture *fixture, uint32_t node_id) {
    uint32_t i;

    if (!fixture || node_id == 0u) {
        return "None";
    }

    for (i = 0u; i < fixture->node_count; ++i) {
        if (fixture->nodes[i].id == node_id) {
            return fixture->nodes[i].label;
        }
    }

    return "None";
}

static int run_frame(KitRenderContext *render_ctx,
                     KitUiContext *ui_ctx,
                     const KitUiInputState *input,
                     int wheel_y,
                     KitGraphStructViewport *viewport,
                     uint32_t *selected_node_id,
                     int *has_selected,
                     int drag_dx,
                     int drag_dy,
                     int *layout_mode,
                     int *fixture_index) {
    const ValidationFixture *fixture;
    KitGraphStructNodeLayout layouts[VALIDATION_MAX_NODES];
    KitGraphStructHit hit;
    KitRenderCommand commands[1024];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitRenderRect info = {32.0f, 32.0f, 280.0f, 536.0f};
    KitRenderRect canvas = {336.0f, 32.0f, 592.0f, 536.0f};
    CoreResult result;
    int hovered_mode;
    KitUiSegmentedResult mode_result;
    KitUiButtonResult fixture_result;
    KitUiButtonResult focus_result;
    const char *mode_labels[2] = {"Tree", "DAG"};
    char fixture_button_text[64];

    fixture = active_fixture(fixture_index ? *fixture_index : 0, fixture_index);
    if (!fixture) {
        return 1;
    }

    if (wheel_y != 0 && kit_ui_point_in_rect(canvas, input->mouse_x, input->mouse_y)) {
        int steps = wheel_y > 0 ? wheel_y : -wheel_y;
        float factor = wheel_y > 0 ? 1.08f : 0.92f;
        while (steps > 0) {
            (void)kit_graph_struct_viewport_zoom_by(viewport, factor, 0.45f, 2.2f);
            steps -= 1;
        }
    }
    if (drag_dx != 0 || drag_dy != 0) {
        (void)kit_graph_struct_viewport_pan_by(viewport, (float)drag_dx, (float)drag_dy);
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 1024;
    command_buffer.count = 0;

    result = kit_render_begin_frame(render_ctx, 960u, 600u, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_begin_frame failed: %d\n", (int)result.code);
        return 1;
    }

    result = kit_render_push_clear(&frame, (KitRenderColor){16, 20, 28, 255});
    if (result.code != CORE_OK) return 1;

    result = kit_render_push_rect(&frame,
                                  &(KitRenderRectCommand){
                                      info, 8.0f, {38, 44, 58, 255}, {0.0f, 0.0f, 1.0f, 1.0f}
                                  });
    if (result.code != CORE_OK) return 1;

    result = kit_ui_draw_label(ui_ctx,
                               &frame,
                               (KitRenderRect){info.x + 10.0f, info.y + 10.0f, info.width - 20.0f, 24.0f},
                               "GRAPH STRUCT VALIDATION",
                               CORE_THEME_COLOR_TEXT_PRIMARY);
    if (result.code != CORE_OK) return 1;

    hovered_mode = -1;
    if (kit_ui_point_in_rect((KitRenderRect){info.x + 10.0f, info.y + 50.0f, info.width - 20.0f, 34.0f}, input->mouse_x, input->mouse_y)) {
        float seg_width = (info.width - 20.0f) * 0.5f;
        hovered_mode = (int)((input->mouse_x - (info.x + 10.0f)) / seg_width);
        if (hovered_mode < 0) hovered_mode = 0;
        if (hovered_mode > 1) hovered_mode = 1;
    }
    mode_result = kit_ui_eval_segmented((KitRenderRect){info.x + 10.0f, info.y + 50.0f, info.width - 20.0f, 34.0f},
                                        input,
                                        1,
                                        2,
                                        *layout_mode);
    if (mode_result.clicked_index >= 0) {
        *layout_mode = mode_result.selected_index;
    }
    result = kit_ui_draw_segmented(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 50.0f, info.width - 20.0f, 34.0f},
                                   mode_labels,
                                   2,
                                   *layout_mode,
                                   hovered_mode,
                                   1);
    if (result.code != CORE_OK) return 1;

    if (*layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG) {
        result = kit_graph_struct_compute_layered_dag_layout(fixture->nodes,
                                                             fixture->node_count,
                                                             fixture->edges,
                                                             fixture->edge_count,
                                                             canvas,
                                                             viewport,
                                                             0,
                                                             layouts);
    } else {
        result = kit_graph_struct_compute_layered_tree_layout(fixture->nodes,
                                                              fixture->node_count,
                                                              fixture->edges,
                                                              fixture->edge_count,
                                                              canvas,
                                                              viewport,
                                                              0,
                                                              layouts);
    }
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_hit_test(layouts, fixture->node_count, input->mouse_x, input->mouse_y, &hit);
    if (result.code != CORE_OK) return 1;
    if (input->mouse_released && hit.active) {
        *selected_node_id = hit.node_id;
        *has_selected = 1;
    }

    snprintf(fixture_button_text, sizeof(fixture_button_text), "Fixture: %s", fixture->name);
    fixture_result = kit_ui_eval_button((KitRenderRect){info.x + 10.0f, info.y + 98.0f, info.width - 20.0f, 32.0f}, input, 1);
    if (fixture_result.clicked && fixture_index) {
        int count = (int)(sizeof(k_fixtures) / sizeof(k_fixtures[0]));
        *fixture_index = (*fixture_index + 1) % count;
        *has_selected = 0;
        *selected_node_id = 0u;
        fixture = active_fixture(*fixture_index, fixture_index);
    }
    result = kit_ui_draw_button(ui_ctx,
                                &frame,
                                (KitRenderRect){info.x + 10.0f, info.y + 98.0f, info.width - 20.0f, 32.0f},
                                fixture_button_text,
                                fixture_result.state);
    if (result.code != CORE_OK) return 1;

    focus_result = kit_ui_eval_button((KitRenderRect){info.x + 10.0f, info.y + 136.0f, info.width - 20.0f, 32.0f}, input, *has_selected);
    if (focus_result.clicked && *has_selected) {
        result = kit_graph_struct_focus_on_node(layouts,
                                                fixture->node_count,
                                                *selected_node_id,
                                                canvas,
                                                26.0f,
                                                viewport);
        if (result.code != CORE_OK) return 1;
        if (*layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG) {
            result = kit_graph_struct_compute_layered_dag_layout(fixture->nodes,
                                                                 fixture->node_count,
                                                                 fixture->edges,
                                                                 fixture->edge_count,
                                                                 canvas,
                                                                 viewport,
                                                                 0,
                                                                 layouts);
        } else {
            result = kit_graph_struct_compute_layered_tree_layout(fixture->nodes,
                                                                  fixture->node_count,
                                                                  fixture->edges,
                                                                  fixture->edge_count,
                                                                  canvas,
                                                                  viewport,
                                                                  0,
                                                                  layouts);
        }
        if (result.code != CORE_OK) return 1;
    }
    result = kit_ui_draw_button(ui_ctx,
                                &frame,
                                (KitRenderRect){info.x + 10.0f, info.y + 136.0f, info.width - 20.0f, 32.0f},
                                "Focus Selected",
                                focus_result.state);
    if (result.code != CORE_OK) return 1;

    result = kit_graph_struct_draw(&frame,
                                   canvas,
                                   fixture->nodes,
                                   fixture->node_count,
                                   fixture->edges,
                                   fixture->edge_count,
                                   layouts,
                                   *selected_node_id,
                                   *has_selected,
                                   hit.node_id,
                                   hit.active);
    if (result.code != CORE_OK) return 1;

    {
        char zoom_text[96];
        char selection_text[96];
        char mode_text[96];
        snprintf(zoom_text, sizeof(zoom_text), "Zoom: %.2fx", viewport->zoom);
        snprintf(selection_text,
                 sizeof(selection_text),
                 "Selected: %s",
                 *has_selected ? selected_label(fixture, *selected_node_id) : "None");
        snprintf(mode_text,
                 sizeof(mode_text),
                 "Mode: %s",
                 *layout_mode == KIT_GRAPH_STRUCT_LAYOUT_LAYERED_DAG ? "Layered DAG" : "Layered Tree");
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 184.0f, info.width - 20.0f, 22.0f},
                                   mode_text,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 210.0f, info.width - 20.0f, 22.0f},
                                   fixture->name,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 236.0f, info.width - 20.0f, 22.0f},
                                   zoom_text,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 262.0f, info.width - 20.0f, 22.0f},
                                   selection_text,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 288.0f, info.width - 20.0f, 22.0f},
                                   fixture->summary,
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 334.0f, info.width - 20.0f, 22.0f},
                                   "Wheel: zoom canvas",
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 360.0f, info.width - 20.0f, 22.0f},
                                   "Drag left mouse: pan",
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
        result = kit_ui_draw_label(ui_ctx,
                                   &frame,
                                   (KitRenderRect){info.x + 10.0f, info.y + 386.0f, info.width - 20.0f, 22.0f},
                                   "Click node: select",
                                   CORE_THEME_COLOR_TEXT_MUTED);
        if (result.code != CORE_OK) return 1;
    }

    result = kit_render_end_frame(render_ctx, &frame);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_end_frame failed: %d\n", (int)result.code);
        return 1;
    }

    return 0;
}

int main(void) {
    SDL_Window *window = 0;
    SDL_Event event;
    bool running = true;
    VkRenderer renderer;
    VkRendererConfig config;
    KitRenderContext render_ctx;
    KitUiContext ui_ctx;
    KitUiInputState input = {0};
    KitGraphStructViewport viewport;
    uint32_t selected_node_id = 0u;
    int has_selected = 0;
    int wheel_y = 0;
    int drag_dx = 0;
    int drag_dy = 0;
    int last_x = 0;
    int last_y = 0;
    int layout_mode = KIT_GRAPH_STRUCT_LAYOUT_LAYERED_TREE;
    int fixture_index = 0;
    CoreResult result;
    int exit_code = 1;

    kit_graph_struct_viewport_default(&viewport);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("kit_graph_struct Vulkan Validation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              960,
                              600,
                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    vk_renderer_config_set_defaults(&config);
    config.enable_validation = VK_FALSE;
    if (vk_renderer_init(&renderer, window, &config) != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed\n");
        goto cleanup_window;
    }

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_VULKAN,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_context_init failed: %d\n", (int)result.code);
        goto cleanup_renderer;
    }

    result = kit_render_attach_external_backend(&render_ctx, &renderer);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_render_attach_external_backend failed: %d\n", (int)result.code);
        goto cleanup_render_ctx;
    }

    result = kit_ui_context_init(&ui_ctx, &render_ctx);
    if (result.code != CORE_OK) {
        fprintf(stderr, "kit_ui_context_init failed: %d\n", (int)result.code);
        goto cleanup_render_ctx;
    }

    while (running) {
        input.mouse_pressed = 0;
        input.mouse_released = 0;
        wheel_y = 0;
        drag_dx = 0;
        drag_dy = 0;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    input.mouse_x = (float)event.motion.x;
                    input.mouse_y = (float)event.motion.y;
                    if (input.mouse_down) {
                        drag_dx += event.motion.x - last_x;
                        drag_dy += event.motion.y - last_y;
                    }
                    last_x = event.motion.x;
                    last_y = event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input.mouse_x = (float)event.button.x;
                        input.mouse_y = (float)event.button.y;
                        input.mouse_down = 1;
                        input.mouse_pressed = 1;
                        last_x = event.button.x;
                        last_y = event.button.y;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        input.mouse_x = (float)event.button.x;
                        input.mouse_y = (float)event.button.y;
                        input.mouse_down = 0;
                        input.mouse_released = 1;
                        last_x = event.button.x;
                        last_y = event.button.y;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    wheel_y = event.wheel.y;
                    break;
                default:
                    break;
            }
        }

        if (run_frame(&render_ctx,
                      &ui_ctx,
                      &input,
                      wheel_y,
                      &viewport,
                      &selected_node_id,
                      &has_selected,
                      drag_dx,
                      drag_dy,
                      &layout_mode,
                      &fixture_index) != 0) {
            goto cleanup_render_ctx;
        }
    }

    exit_code = 0;

cleanup_render_ctx:
    kit_render_context_shutdown(&render_ctx);
cleanup_renderer:
    vk_renderer_shutdown(&renderer);
cleanup_window:
    SDL_DestroyWindow(window);
cleanup:
    SDL_Quit();
    return exit_code;
}
