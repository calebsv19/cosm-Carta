#include "kit_pane.h"

#include <stdio.h>

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

    if (command_buffer.count < 5u) {
        fprintf(stderr, "expected at least 5 commands, got %zu\n", command_buffer.count);
        return 1;
    }
    if (command_buffer.commands[0].kind != KIT_RENDER_CMD_RECT) {
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

int main(void) {
    if (test_chrome_draw_emits_expected_commands() != 0) {
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

    puts("kit_pane tests passed");
    return 0;
}
