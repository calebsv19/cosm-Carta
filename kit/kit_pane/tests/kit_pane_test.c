#include "kit_pane.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int colors_equal(KitRenderColor a, KitRenderColor b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int resolve_theme_color(KitRenderContext *render_ctx,
                               CoreThemeColorToken token,
                               KitRenderColor *out_color) {
    CoreResult result = kit_render_resolve_theme_color(render_ctx, token, out_color);
    return result.code == CORE_OK ? 0 : 1;
}

static int test_chrome_draw_emits_expected_commands(void) {
    KitRenderContext render_ctx;
    KitRenderCommand commands[16];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitPaneStyle style;
    KitPaneChrome chrome;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        return 1;
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 16;
    command_buffer.count = 0;

    result = kit_render_begin_frame(&render_ctx, 800, 600, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    kit_pane_style_default(&style);
    chrome.pane_id = 42u;
    chrome.title = "Data View";
    chrome.bounds = (KitRenderRect){ 20.0f, 40.0f, 320.0f, 240.0f };
    chrome.state = KIT_PANE_STATE_FOCUSED;
    chrome.show_header = 1;
    chrome.show_id = 1;
    chrome.authoring_selected = 1;

    result = kit_pane_draw_chrome(&render_ctx, &frame, &style, &chrome);
    if (result.code != CORE_OK) {
        return 1;
    }

    if (command_buffer.count != 5u) {
        fprintf(stderr, "expected exactly 5 commands, got %zu\n", command_buffer.count);
        return 1;
    }
    if (command_buffer.commands[0].kind != KIT_RENDER_CMD_RECT ||
        command_buffer.commands[1].kind != KIT_RENDER_CMD_RECT ||
        command_buffer.commands[2].kind != KIT_RENDER_CMD_RECT ||
        command_buffer.commands[3].kind != KIT_RENDER_CMD_TEXT ||
        command_buffer.commands[4].kind != KIT_RENDER_CMD_TEXT) {
        return 1;
    }
    if (command_buffer.commands[3].data.text.text != chrome.title) {
        fprintf(stderr, "expected title text pointer to be borrowed directly\n");
        return 1;
    }
    if (strcmp(command_buffer.commands[4].data.text.text, "#42") != 0) {
        fprintf(stderr, "expected pane id label to be rendered as #42\n");
        return 1;
    }
    if (command_buffer.commands[3].data.text.color_token != CORE_THEME_COLOR_TEXT_PRIMARY ||
        command_buffer.commands[4].data.text.color_token != CORE_THEME_COLOR_TEXT_MUTED) {
        return 1;
    }
    {
        KitRenderColor accent_primary = {0};
        KitRenderColor surface_1 = {0};
        if (resolve_theme_color(&render_ctx, CORE_THEME_COLOR_ACCENT_PRIMARY, &accent_primary) != 0 ||
            resolve_theme_color(&render_ctx, CORE_THEME_COLOR_SURFACE_1, &surface_1) != 0) {
            return 1;
        }
        if (!colors_equal(command_buffer.commands[0].data.rect.color, accent_primary) ||
            !colors_equal(command_buffer.commands[1].data.rect.color, surface_1) ||
            !colors_equal(command_buffer.commands[2].data.rect.color, surface_1)) {
            return 1;
        }
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    return 0;
}

static int test_chrome_draw_clamps_style_values(void) {
    KitRenderContext render_ctx;
    KitRenderCommand commands[8];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitPaneStyle style;
    KitPaneChrome chrome;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        return 1;
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 8;
    command_buffer.count = 0;

    result = kit_render_begin_frame(&render_ctx, 800, 600, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    style.border_thickness = 99.0f;
    style.header_height = 999.0f;
    style.corner_radius = 99.0f;
    style.title_padding = 99.0f;
    style.splitter_thickness = 8.0f;

    chrome.pane_id = 7u;
    chrome.title = "Clamp";
    chrome.bounds = (KitRenderRect){ 20.0f, 40.0f, 80.0f, 24.0f };
    chrome.state = KIT_PANE_STATE_NORMAL;
    chrome.show_header = 1;
    chrome.show_id = 0;
    chrome.authoring_selected = 0;

    result = kit_pane_draw_chrome(&render_ctx, &frame, &style, &chrome);
    if (result.code != CORE_OK) {
        return 1;
    }

    if (command_buffer.count != 4u) {
        return 1;
    }
    if (command_buffer.commands[0].data.rect.corner_radius != 12.0f) {
        return 1;
    }
    if (command_buffer.commands[1].data.rect.rect.x != 28.0f ||
        command_buffer.commands[1].data.rect.rect.y != 48.0f ||
        command_buffer.commands[1].data.rect.rect.width != 64.0f ||
        command_buffer.commands[1].data.rect.rect.height != 8.0f) {
        return 1;
    }
    if (command_buffer.commands[2].data.rect.rect.height != 8.0f) {
        fprintf(stderr, "expected header height to clamp to inner rect height\n");
        return 1;
    }
    if (command_buffer.commands[3].data.text.origin.x != 48.0f ||
        command_buffer.commands[3].data.text.origin.y != 58.0f) {
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    return 0;
}

static int test_splitter_draw_state_changes(void) {
    KitRenderContext render_ctx;
    KitRenderCommand commands[8];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_IDE_GRAY,
                                     CORE_FONT_PRESET_IDE);
    if (result.code != CORE_OK) {
        return 1;
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 8;
    command_buffer.count = 0;

    result = kit_render_begin_frame(&render_ctx, 640, 480, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_draw_splitter(&render_ctx,
                                    &frame,
                                    (KitRenderRect){ 100.0f, 20.0f, 6.0f, 200.0f },
                                    0,
                                    0);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_draw_splitter(&render_ctx,
                                    &frame,
                                    (KitRenderRect){ 110.0f, 20.0f, 6.0f, 200.0f },
                                    1,
                                    0);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_draw_splitter(&render_ctx,
                                    &frame,
                                    (KitRenderRect){ 120.0f, 20.0f, 6.0f, 200.0f },
                                    1,
                                    1);
    if (result.code != CORE_OK) {
        return 1;
    }

    if (command_buffer.count != 3u) {
        fprintf(stderr, "expected 3 splitter commands, got %zu\n", command_buffer.count);
        return 1;
    }
    {
        KitRenderColor muted = {0};
        KitRenderColor primary = {0};
        KitRenderColor accent = {0};
        if (resolve_theme_color(&render_ctx, CORE_THEME_COLOR_TEXT_MUTED, &muted) != 0 ||
            resolve_theme_color(&render_ctx, CORE_THEME_COLOR_TEXT_PRIMARY, &primary) != 0 ||
            resolve_theme_color(&render_ctx, CORE_THEME_COLOR_ACCENT_PRIMARY, &accent) != 0) {
            return 1;
        }
        if (!colors_equal(command_buffer.commands[0].data.rect.color, muted) ||
            !colors_equal(command_buffer.commands[1].data.rect.color, primary) ||
            !colors_equal(command_buffer.commands[2].data.rect.color, accent)) {
            return 1;
        }
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    return 0;
}

static int test_splitter_interaction_tracks_hover_and_drag(void) {
    CorePaneNode nodes[3];
    CorePaneRect bounds = { 0.0f, 0.0f, 1000.0f, 600.0f };
    CorePaneRect current_bounds = {0};
    KitPaneSplitterInteraction interaction;
    CoreResult result;
    int hovered = 0;
    int active = 0;
    int changed = 0;

    nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.25f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = { 100.0f, 200.0f }
    };
    nodes[1] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 10u };
    nodes[2] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 11u };

    kit_pane_splitter_interaction_init(&interaction, 10.0f);

    result = kit_pane_splitter_interaction_set_hover(&interaction,
                                                     nodes,
                                                     3u,
                                                     0u,
                                                     bounds,
                                                     250.0f,
                                                     300.0f);
    if (result.code != CORE_OK) {
        return 1;
    }
    if (!kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }
    if (!hovered || active) {
        return 1;
    }

    result = kit_pane_splitter_interaction_begin_drag(&interaction,
                                                      nodes,
                                                      3u,
                                                      0u,
                                                      bounds,
                                                      250.0f,
                                                      300.0f);
    if (result.code != CORE_OK) {
        return 1;
    }

    current_bounds = interaction.drag_hit.splitter_bounds;
    result = kit_pane_splitter_interaction_set_hover(&interaction,
                                                     nodes,
                                                     3u,
                                                     0u,
                                                     bounds,
                                                     900.0f,
                                                     500.0f);
    if (result.code != CORE_OK) {
        return 1;
    }
    if (!kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }
    if (!hovered || !active) {
        return 1;
    }

    result = kit_pane_splitter_interaction_update_drag(&interaction,
                                                       nodes,
                                                       3u,
                                                       320.0f,
                                                       300.0f,
                                                       &changed);
    if (result.code != CORE_OK || !changed) {
        return 1;
    }
    if (nodes[0].ratio_01 <= 0.25f) {
        return 1;
    }
    if (!kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }
    if (!hovered || !active) {
        return 1;
    }
    if (current_bounds.x <= 245.0f) {
        fprintf(stderr, "expected splitter bounds to move with drag, x=%f\n", current_bounds.x);
        return 1;
    }

    kit_pane_splitter_interaction_end_drag(&interaction);
    if (kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }

    return 0;
}

static int test_splitter_interaction_accepts_cached_hits(void) {
    CorePaneNode nodes[7];
    CorePaneRect bounds = { 0.0f, 0.0f, 960.0f, 640.0f };
    CorePaneSplitterHit hits[4] = {0};
    KitPaneSplitterInteraction interaction;
    CorePaneRect current_bounds = {0};
    CoreResult result;
    uint32_t hit_count = 0u;
    int hovered = 0;
    int active = 0;
    int changed = 0;

    nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.50f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = { 120.0f, 120.0f }
    };
    nodes[1] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 2u,
        .axis = CORE_PANE_AXIS_VERTICAL,
        .ratio_01 = 0.50f,
        .child_a = 3u,
        .child_b = 4u,
        .constraints = { 80.0f, 80.0f }
    };
    nodes[2] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 20u };
    nodes[3] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 21u };
    nodes[4] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 22u };
    nodes[5] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 23u };
    nodes[6] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 24u };
    nodes[2].type = CORE_PANE_NODE_SPLIT;
    nodes[2].axis = CORE_PANE_AXIS_VERTICAL;
    nodes[2].ratio_01 = 0.50f;
    nodes[2].child_a = 5u;
    nodes[2].child_b = 6u;
    nodes[2].constraints.min_size_a = 80.0f;
    nodes[2].constraints.min_size_b = 80.0f;

    if (!core_pane_collect_splitter_hits(nodes, 7u, 0u, bounds, 12.0f, hits, 4u, &hit_count)) {
        return 1;
    }
    if (hit_count != 3u) {
        return 1;
    }

    kit_pane_splitter_interaction_init(&interaction, 12.0f);

    result = kit_pane_splitter_interaction_set_hover_from_hits(&interaction,
                                                               hits,
                                                               hit_count,
                                                               240.0f,
                                                               320.0f);
    if (result.code != CORE_OK) {
        return 1;
    }
    if (!kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }
    if (!hovered || active) {
        return 1;
    }

    result = kit_pane_splitter_interaction_begin_drag_from_hits(&interaction,
                                                                hits,
                                                                hit_count,
                                                                240.0f,
                                                                320.0f);
    if (result.code != CORE_OK) {
        return 1;
    }
    result = kit_pane_splitter_interaction_update_drag(&interaction,
                                                       nodes,
                                                       7u,
                                                       240.0f,
                                                       380.0f,
                                                       &changed);
    if (result.code != CORE_OK || !changed) {
        return 1;
    }
    if (nodes[1].ratio_01 <= 0.50f) {
        return 1;
    }
    kit_pane_splitter_interaction_end_drag(&interaction);
    return 0;
}

static int test_draw_rejects_invalid_inputs(void) {
    KitRenderContext render_ctx;
    KitRenderCommand commands[4];
    KitRenderCommandBuffer command_buffer;
    KitRenderFrame frame;
    KitPaneChrome chrome;
    CoreResult result;

    result = kit_render_context_init(&render_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_DAW_DEFAULT,
                                     CORE_FONT_PRESET_DAW_DEFAULT);
    if (result.code != CORE_OK) {
        return 1;
    }

    command_buffer.commands = commands;
    command_buffer.capacity = 4;
    command_buffer.count = 0;
    result = kit_render_begin_frame(&render_ctx, 320, 240, &command_buffer, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    chrome.pane_id = 1u;
    chrome.title = "Invalid";
    chrome.bounds = (KitRenderRect){ 0.0f, 0.0f, 0.0f, 20.0f };
    chrome.state = KIT_PANE_STATE_NORMAL;
    chrome.show_header = 1;
    chrome.show_id = 0;
    chrome.authoring_selected = 0;

    if (kit_pane_draw_chrome(NULL, &frame, NULL, &chrome).code != CORE_ERR_INVALID_ARG ||
        kit_pane_draw_chrome(&render_ctx, NULL, NULL, &chrome).code != CORE_ERR_INVALID_ARG ||
        kit_pane_draw_chrome(&render_ctx, &frame, NULL, NULL).code != CORE_ERR_INVALID_ARG ||
        kit_pane_draw_chrome(&render_ctx, &frame, NULL, &chrome).code != CORE_ERR_INVALID_ARG) {
        return 1;
    }

    if (kit_pane_draw_splitter(NULL, &frame, (KitRenderRect){ 0.0f, 0.0f, 4.0f, 20.0f }, 0, 0).code != CORE_ERR_INVALID_ARG ||
        kit_pane_draw_splitter(&render_ctx, NULL, (KitRenderRect){ 0.0f, 0.0f, 4.0f, 20.0f }, 0, 0).code != CORE_ERR_INVALID_ARG ||
        kit_pane_draw_splitter(&render_ctx, &frame, (KitRenderRect){ 0.0f, 0.0f, -1.0f, 20.0f }, 0, 0).code != CORE_ERR_INVALID_ARG) {
        return 1;
    }

    result = kit_render_end_frame(&render_ctx, &frame);
    if (result.code != CORE_OK) {
        return 1;
    }

    return 0;
}

static int test_cached_hit_interaction_edge_cases(void) {
    CorePaneNode nodes[3];
    CorePaneSplitterHit inactive_hits[1] = {0};
    CorePaneRect bounds = { 0.0f, 0.0f, 1000.0f, 600.0f };
    CorePaneRect current_bounds = {0};
    KitPaneSplitterInteraction interaction;
    CoreResult result;
    int hovered = 0;
    int active = 0;

    nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.25f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = { 100.0f, 200.0f }
    };
    nodes[1] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 10u };
    nodes[2] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 11u };

    kit_pane_splitter_interaction_init(&interaction, 10.0f);

    result = kit_pane_splitter_interaction_set_hover_from_hits(&interaction, NULL, 0u, 100.0f, 100.0f);
    if (result.code != CORE_ERR_NOT_FOUND) {
        return 1;
    }
    if (kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }

    result = kit_pane_splitter_interaction_set_hover_from_hits(&interaction, inactive_hits, 1u, 100.0f, 100.0f);
    if (result.code != CORE_ERR_NOT_FOUND) {
        return 1;
    }

    result = kit_pane_splitter_interaction_begin_drag(&interaction,
                                                      nodes,
                                                      3u,
                                                      0u,
                                                      bounds,
                                                      250.0f,
                                                      300.0f);
    if (result.code != CORE_OK) {
        return 1;
    }

    result = kit_pane_splitter_interaction_set_hover_from_hits(&interaction, NULL, 0u, 900.0f, 900.0f);
    if (result.code != CORE_OK) {
        return 1;
    }
    if (!kit_pane_splitter_interaction_current(&interaction, &current_bounds, &hovered, &active)) {
        return 1;
    }
    if (!hovered || !active) {
        return 1;
    }

    kit_pane_splitter_interaction_end_drag(&interaction);
    return 0;
}

static int test_splitter_interaction_rejects_invalid_update_paths(void) {
    CorePaneNode nodes[3];
    KitPaneSplitterInteraction interaction;
    CoreResult result;
    int changed = 99;
    float last_x;
    float last_y;

    nodes[0] = (CorePaneNode){
        .type = CORE_PANE_NODE_SPLIT,
        .id = 1u,
        .axis = CORE_PANE_AXIS_HORIZONTAL,
        .ratio_01 = 0.25f,
        .child_a = 1u,
        .child_b = 2u,
        .constraints = { 100.0f, 200.0f }
    };
    nodes[1] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 10u };
    nodes[2] = (CorePaneNode){ .type = CORE_PANE_NODE_LEAF, .id = 11u };

    kit_pane_splitter_interaction_init(&interaction, 10.0f);

    if (kit_pane_splitter_interaction_update_drag(&interaction, nodes, 3u, 10.0f, 10.0f, &changed).code != CORE_ERR_INVALID_ARG) {
        return 1;
    }

    interaction.drag_active = 1;
    interaction.drag_hit.active = 1;
    interaction.drag_hit.node_index = 0u;
    interaction.drag_hit.axis = CORE_PANE_AXIS_VERTICAL;
    interaction.drag_hit.parent_span = 1000.0f;
    interaction.drag_last_x = 250.0f;
    interaction.drag_last_y = 300.0f;
    last_x = interaction.drag_last_x;
    last_y = interaction.drag_last_y;

    result = kit_pane_splitter_interaction_update_drag(&interaction, nodes, 3u, 260.0f, 300.0f, &changed);
    if (result.code != CORE_ERR_INVALID_ARG) {
        return 1;
    }
    if (interaction.drag_last_x != last_x || interaction.drag_last_y != last_y) {
        fprintf(stderr, "expected invalid drag update to preserve last drag point\n");
        return 1;
    }

    interaction.drag_hit.axis = CORE_PANE_AXIS_HORIZONTAL;
    interaction.drag_hit.parent_span = INFINITY;
    result = kit_pane_splitter_interaction_update_drag(&interaction, nodes, 3u, 260.0f, 300.0f, &changed);
    if (result.code != CORE_ERR_INVALID_ARG) {
        return 1;
    }

    interaction.drag_hit.parent_span = 1000.0f;
    result = kit_pane_splitter_interaction_update_drag(&interaction, nodes, 3u, NAN, 300.0f, &changed);
    if (result.code != CORE_ERR_INVALID_ARG) {
        return 1;
    }

    return 0;
}

int main(void) {
    if (test_chrome_draw_emits_expected_commands() != 0) {
        return 1;
    }
    if (test_chrome_draw_clamps_style_values() != 0) {
        return 1;
    }
    if (test_splitter_draw_state_changes() != 0) {
        return 1;
    }
    if (test_splitter_interaction_tracks_hover_and_drag() != 0) {
        return 1;
    }
    if (test_splitter_interaction_accepts_cached_hits() != 0) {
        return 1;
    }
    if (test_draw_rejects_invalid_inputs() != 0) {
        return 1;
    }
    if (test_cached_hit_interaction_edge_cases() != 0) {
        return 1;
    }
    if (test_splitter_interaction_rejects_invalid_update_paths() != 0) {
        return 1;
    }

    puts("kit_pane tests passed");
    return 0;
}
