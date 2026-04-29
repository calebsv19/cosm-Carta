#include "kit_workspace_authoring.h"
#include "kit_workspace_authoring_ui.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct ActionMockContext {
    CoreResult execute_result;
    int close_called;
    const char *last_action_id;
} ActionMockContext;

static CoreResult mock_execute_action(void *host_context,
                                      const char *action_id,
                                      uint32_t *io_selected_pane_id,
                                      int *io_pending_apply) {
    ActionMockContext *ctx = (ActionMockContext *)host_context;
    assert(ctx != NULL);
    assert(io_selected_pane_id != NULL);
    assert(io_pending_apply != NULL);
    ctx->last_action_id = action_id;
    *io_selected_pane_id += 1u;
    *io_pending_apply = 1;
    return ctx->execute_result;
}

static void mock_close_picker(void *host_context) {
    ActionMockContext *ctx = (ActionMockContext *)host_context;
    assert(ctx != NULL);
    ctx->close_called += 1;
}

typedef struct TextStepMockContext {
    int current_step;
    int min_step;
    int max_step;
    int increment;
    int default_step;
    int persist_called;
} TextStepMockContext;

typedef struct SubmitMockContext {
    int draw_called;
    int rebuild_required;
    int acknowledge_called;
    CoreResult draw_result;
} SubmitMockContext;

static int mock_clamp_step(void *host_context, int target_step) {
    TextStepMockContext *ctx = (TextStepMockContext *)host_context;
    if (target_step < ctx->min_step) return ctx->min_step;
    if (target_step > ctx->max_step) return ctx->max_step;
    return target_step;
}

static int mock_current_step(void *host_context) {
    return ((TextStepMockContext *)host_context)->current_step;
}

static int mock_increment(void *host_context) {
    return ((TextStepMockContext *)host_context)->increment;
}

static int mock_default_step(void *host_context) {
    return ((TextStepMockContext *)host_context)->default_step;
}

static CoreResult mock_set_step(void *host_context, int step) {
    TextStepMockContext *ctx = (TextStepMockContext *)host_context;
    ctx->current_step = step;
    return core_result_ok();
}

static void mock_persist_step(void *host_context) {
    TextStepMockContext *ctx = (TextStepMockContext *)host_context;
    ctx->persist_called += 1;
}

static CoreResult mock_submit_draw_scene(void *host_context,
                                         const KitWorkspaceAuthoringRenderDeriveFrame *derive) {
    SubmitMockContext *ctx = (SubmitMockContext *)host_context;
    assert(ctx != NULL);
    assert(derive != NULL);
    ctx->draw_called += 1;
    assert(derive->width == 1280);
    assert(derive->height == 720);
    assert(derive->selected_pane_id == 3);
    assert(derive->pending_apply == 1);
    return ctx->draw_result;
}

static int mock_submit_rebuild_required(void *host_context) {
    SubmitMockContext *ctx = (SubmitMockContext *)host_context;
    assert(ctx != NULL);
    return ctx->rebuild_required;
}

static void mock_submit_acknowledge(void *host_context) {
    SubmitMockContext *ctx = (SubmitMockContext *)host_context;
    assert(ctx != NULL);
    ctx->acknowledge_called += 1;
}

static void test_root_bounds(void) {
    CorePaneRect bounds = kit_workspace_authoring_root_bounds(1920, 1080);
    assert(bounds.x == 0.0f);
    assert(bounds.y == 0.0f);
    assert(bounds.width == 1920.0f);
    assert(bounds.height == 1080.0f);
}

static void test_trigger_mapping(void) {
    assert(strcmp(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_TAB, 0u), "tab") == 0);
    assert(strcmp(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_ENTER, 0u), "enter") == 0);
    assert(strcmp(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2, 0u), "2") == 0);
    assert(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_H,
                                                    KIT_WORKSPACE_AUTHORING_MOD_SHIFT) == NULL);
    assert(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_H,
                                                    KIT_WORKSPACE_AUTHORING_MOD_ALT) == NULL);
    assert(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_V,
                                                    KIT_WORKSPACE_AUTHORING_MOD_SHIFT) == NULL);
    assert(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_V,
                                                    KIT_WORKSPACE_AUTHORING_MOD_ALT) == NULL);
    assert(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3,
                                                    KIT_WORKSPACE_AUTHORING_MOD_SHIFT) == NULL);
    assert(kit_workspace_authoring_trigger_from_key(KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3,
                                                    KIT_WORKSPACE_AUTHORING_MOD_ALT) == NULL);
}

static void test_entry_chord(void) {
    assert(kit_workspace_authoring_entry_chord_pressed(KIT_WORKSPACE_AUTHORING_KEY_C,
                                                       KIT_WORKSPACE_AUTHORING_MOD_ALT,
                                                       1,
                                                       1) == 1);
    assert(kit_workspace_authoring_entry_chord_pressed(KIT_WORKSPACE_AUTHORING_KEY_C,
                                                       KIT_WORKSPACE_AUTHORING_MOD_ALT |
                                                           KIT_WORKSPACE_AUTHORING_MOD_SHIFT,
                                                       1,
                                                       1) == 0);
    assert(kit_workspace_authoring_entry_chord_pressed(KIT_WORKSPACE_AUTHORING_KEY_C,
                                                       KIT_WORKSPACE_AUTHORING_MOD_ALT |
                                                           KIT_WORKSPACE_AUTHORING_MOD_CTRL,
                                                       1,
                                                       1) == 0);
    assert(kit_workspace_authoring_entry_chord_pressed(KIT_WORKSPACE_AUTHORING_KEY_C,
                                                       KIT_WORKSPACE_AUTHORING_MOD_ALT |
                                                           KIT_WORKSPACE_AUTHORING_MOD_GUI,
                                                       1,
                                                       1) == 0);
    assert(kit_workspace_authoring_entry_chord_pressed(KIT_WORKSPACE_AUTHORING_KEY_H,
                                                       KIT_WORKSPACE_AUTHORING_MOD_ALT,
                                                       1,
                                                       1) == 0);
}

static void test_execute_action_close_policy(void) {
    ActionMockContext mock_ctx = {0};
    KitWorkspaceAuthoringActionHooks hooks = {0};
    uint32_t selected = 7u;
    int pending_apply = 0;
    CoreResult result;

    hooks.execute_action = mock_execute_action;
    hooks.close_picker = mock_close_picker;
    mock_ctx.execute_result = core_result_ok();

    result = kit_workspace_authoring_execute_action(&mock_ctx,
                                                    &hooks,
                                                    "workspace.apply",
                                                    &selected,
                                                    &pending_apply,
                                                    0,
                                                    1,
                                                    0,
                                                    0);
    assert(result.code == CORE_OK);
    assert(selected == 8u);
    assert(pending_apply == 1);
    assert(mock_ctx.close_called == 1);
    assert(strcmp(mock_ctx.last_action_id, "workspace.apply") == 0);

    selected = 3u;
    pending_apply = 0;
    mock_ctx.close_called = 0;

    result = kit_workspace_authoring_execute_action(&mock_ctx,
                                                    &hooks,
                                                    "workspace.split_horizontal",
                                                    &selected,
                                                    &pending_apply,
                                                    1,
                                                    1,
                                                    0,
                                                    0);
    assert(result.code == CORE_OK);
    assert(mock_ctx.close_called == 0);
}

static void test_text_size_hooks(void) {
    TextStepMockContext text_ctx = {0};
    KitWorkspaceAuthoringTextStepHooks hooks = {0};
    int applied = 0;
    CoreResult result;

    text_ctx.current_step = 0;
    text_ctx.min_step = -4;
    text_ctx.max_step = 5;
    text_ctx.increment = 1;
    text_ctx.default_step = 0;

    hooks.clamp_step = mock_clamp_step;
    hooks.current_step = mock_current_step;
    hooks.step_increment = mock_increment;
    hooks.default_step = mock_default_step;
    hooks.set_step = mock_set_step;
    hooks.persist_step = mock_persist_step;

    result = kit_workspace_authoring_apply_text_size_step(&text_ctx, &hooks, 10, &applied);
    assert(result.code == CORE_OK);
    assert(applied == 5);
    assert(text_ctx.current_step == 5);
    assert(text_ctx.persist_called == 1);

    result = kit_workspace_authoring_adjust_text_size_step(&text_ctx, &hooks, -1, &applied);
    assert(result.code == CORE_OK);
    assert(applied == 4);
    assert(text_ctx.current_step == 4);

    result = kit_workspace_authoring_reset_text_size_step(&text_ctx, &hooks, &applied);
    assert(result.code == CORE_OK);
    assert(applied == 0);
    assert(text_ctx.current_step == 0);
}

static void test_overlay_button_layout(void) {
    KitWorkspaceAuthoringOverlayButton buttons[8];
    uint32_t count = 0u;

    count = kit_workspace_authoring_ui_build_overlay_buttons(1200, 0, 0, buttons, 8u);
    assert(count == 1u);
    assert(buttons[0].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE);
    assert(strcmp(buttons[0].label, "Enter Authoring") == 0);

    count = kit_workspace_authoring_ui_build_overlay_buttons(1200, 1, 1, buttons, 8u);
    assert(count == 4u);
    assert(buttons[0].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE);
    assert(strcmp(buttons[0].label, "Overlay: Pane") == 0);
    assert(buttons[1].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY);
    assert(buttons[2].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL);
    assert(buttons[3].id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD);

    count = kit_workspace_authoring_ui_build_overlay_buttons(1200, 1, 0, buttons, 8u);
    assert(count == 3u);
    assert(strcmp(buttons[0].label, "Overlay: Font/Theme") == 0);
}

static void test_overlay_hit_and_drop_intent(void) {
    KitWorkspaceAuthoringOverlayButton buttons[8];
    uint32_t count = 0u;
    KitWorkspaceAuthoringOverlayButtonId hit = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
    CorePaneRect rect = {100.0f, 50.0f, 400.0f, 300.0f};
    CorePaneRect ghost = {0};
    KitWorkspaceAuthoringDropIntent intent = KIT_WORKSPACE_AUTHORING_DROP_INTENT_NONE;

    count = kit_workspace_authoring_ui_build_overlay_buttons(1000, 1, 1, buttons, 8u);
    assert(count >= 1u);
    hit = kit_workspace_authoring_ui_overlay_hit_test(
        buttons,
        count,
        buttons[0].rect.x + 4.0f,
        buttons[0].rect.y + 4.0f);
    assert(hit == buttons[0].id);
    hit = kit_workspace_authoring_ui_overlay_hit_test(buttons, count, 2.0f, 2.0f);
    assert(hit == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE);

    intent = kit_workspace_authoring_ui_drop_intent_from_point(rect, rect.x + 3.0f, rect.y + 120.0f, &ghost);
    assert(intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_LEFT);
    assert(strcmp(kit_workspace_authoring_ui_drop_intent_label(intent), "LEFT") == 0);
    assert(fabsf(ghost.width - (rect.width - 2.0f) * 0.5f) < 0.001f);

    intent = kit_workspace_authoring_ui_drop_intent_from_point(rect, rect.x + rect.width - 2.0f, rect.y + 120.0f, &ghost);
    assert(intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_RIGHT);

    intent = kit_workspace_authoring_ui_drop_intent_from_point(rect, rect.x + 200.0f, rect.y + 2.0f, &ghost);
    assert(intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_TOP);

    intent = kit_workspace_authoring_ui_drop_intent_from_point(rect,
                                                               rect.x + 200.0f,
                                                               rect.y + rect.height - 2.0f,
                                                               &ghost);
    assert(intent == KIT_WORKSPACE_AUTHORING_DROP_INTENT_BOTTOM);
}

static void test_pane_overlay_visibility(void) {
    assert(kit_workspace_authoring_ui_pane_overlay_visible(0, 1, 0, 1) == 1);
    assert(kit_workspace_authoring_ui_pane_overlay_visible(1, 1, 0, 1) == 1);
    assert(kit_workspace_authoring_ui_pane_overlay_visible(1, 1, 1, 1) == 0);
}

static void test_render_derive_submit(void) {
    KitWorkspaceAuthoringRenderDeriveFrame derive = {0};
    KitWorkspaceAuthoringRenderSubmitOutcome outcome = {0};
    SubmitMockContext submit_ctx = {0};

    kit_workspace_authoring_ui_derive_frame(&derive,
                                            1280,
                                            720,
                                            3,
                                            1,
                                            2,
                                            3,
                                            4,
                                            1,
                                            0,
                                            1,
                                            1,
                                            333.0f,
                                            100.0f,
                                            500.0f,
                                            99u,
                                            2u,
                                            42,
                                            1);
    assert(derive.width == 1280);
    assert(derive.height == 720);
    assert(derive.selected_corner_group_has_key == 1);
    assert(derive.splitter_snap_corner_id == 99u);
    assert(derive.frame_index == 42);

    submit_ctx.draw_result = core_result_ok();
    submit_ctx.rebuild_required = 1;
    kit_workspace_authoring_ui_submit_frame(&submit_ctx,
                                            &derive,
                                            mock_submit_draw_scene,
                                            mock_submit_rebuild_required,
                                            mock_submit_acknowledge,
                                            &outcome);
    assert(outcome.draw_result.code == CORE_OK);
    assert(outcome.rebuild_acknowledged == 1u);
    assert(submit_ctx.draw_called == 1);
    assert(submit_ctx.acknowledge_called == 1);

    memset(&outcome, 0, sizeof(outcome));
    submit_ctx.draw_result = (CoreResult){ CORE_ERR_IO, "draw failed" };
    submit_ctx.rebuild_required = 1;
    kit_workspace_authoring_ui_submit_frame(&submit_ctx,
                                            &derive,
                                            mock_submit_draw_scene,
                                            mock_submit_rebuild_required,
                                            mock_submit_acknowledge,
                                            &outcome);
    assert(outcome.draw_result.code == CORE_ERR_IO);
    assert(outcome.rebuild_acknowledged == 0u);
}

int main(void) {
    test_root_bounds();
    test_trigger_mapping();
    test_entry_chord();
    test_execute_action_close_policy();
    test_text_size_hooks();
    test_overlay_button_layout();
    test_overlay_hit_and_drop_intent();
    test_pane_overlay_visibility();
    test_render_derive_submit();
    puts("kit_workspace_authoring tests passed");
    return 0;
}
