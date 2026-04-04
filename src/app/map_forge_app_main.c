#include "map_forge/map_forge_app_main.h"

#include <stdio.h>
#include <string.h>

#include "app/app.h"

typedef enum MapForgeWrapperError {
    MAP_FORGE_WRAPPER_ERROR_NONE = 0,
    MAP_FORGE_WRAPPER_ERROR_BOOTSTRAP_FAILED = 1,
    MAP_FORGE_WRAPPER_ERROR_CONFIG_LOAD_FAILED = 2,
    MAP_FORGE_WRAPPER_ERROR_STATE_SEED_FAILED = 3,
    MAP_FORGE_WRAPPER_ERROR_SUBSYSTEMS_INIT_FAILED = 4,
    MAP_FORGE_WRAPPER_ERROR_RUNTIME_START_FAILED = 5,
    MAP_FORGE_WRAPPER_ERROR_RUN_LOOP_FAILED = 6,
    MAP_FORGE_WRAPPER_ERROR_STAGE_TRANSITION_FAILED = 7
} MapForgeWrapperError;

static void map_forge_log_wrapper_error(const char *fn_name,
                                        MapForgeWrapperError wrapper_error,
                                        MapForgeAppStage stage,
                                        int exit_code,
                                        const char *detail) {
    fprintf(stderr,
            "map_forge: wrapper error fn=%s code=%d stage=%d exit_code=%d detail=%s\n",
            fn_name ? fn_name : "unknown",
            (int)wrapper_error,
            (int)stage,
            exit_code,
            detail ? detail : "n/a");
}

static bool map_forge_app_stage_transition(MapForgeAppContext *ctx,
                                           MapForgeAppStage expected,
                                           MapForgeAppStage next,
                                           const char *stage_name,
                                           const char *fn_name) {
    if (!ctx) {
        return false;
    }
    if (ctx->stage != expected) {
        fprintf(stderr,
                "map_forge: lifecycle stage order violation fn=%s stage=%s (expected=%d actual=%d next=%d)\n",
                fn_name ? fn_name : "unknown",
                stage_name ? stage_name : "unknown",
                (int)expected,
                (int)ctx->stage,
                (int)next);
        return false;
    }
    ctx->stage = next;
    return true;
}

bool map_forge_app_bootstrap(MapForgeAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->exit_code = 1;
    ctx->last_dispatch_exit_code = 1;
    ctx->wrapper_error = MAP_FORGE_WRAPPER_ERROR_NONE;
    ctx->stage = MAP_FORGE_APP_STAGE_INIT;
    return map_forge_app_stage_transition(ctx,
                                          MAP_FORGE_APP_STAGE_INIT,
                                          MAP_FORGE_APP_STAGE_BOOTSTRAPPED,
                                          "map_forge_app_bootstrap",
                                          __func__);
}

bool map_forge_app_config_load(MapForgeAppContext *ctx) {
    return map_forge_app_stage_transition(ctx,
                                          MAP_FORGE_APP_STAGE_BOOTSTRAPPED,
                                          MAP_FORGE_APP_STAGE_CONFIG_LOADED,
                                          "map_forge_app_config_load",
                                          __func__);
}

bool map_forge_app_state_seed(MapForgeAppContext *ctx) {
    return map_forge_app_stage_transition(ctx,
                                          MAP_FORGE_APP_STAGE_CONFIG_LOADED,
                                          MAP_FORGE_APP_STAGE_STATE_SEEDED,
                                          "map_forge_app_state_seed",
                                          __func__);
}

bool map_forge_app_subsystems_init(MapForgeAppContext *ctx) {
    return map_forge_app_stage_transition(ctx,
                                          MAP_FORGE_APP_STAGE_STATE_SEEDED,
                                          MAP_FORGE_APP_STAGE_SUBSYSTEMS_READY,
                                          "map_forge_app_subsystems_init",
                                          __func__);
}

bool map_forge_runtime_start(MapForgeAppContext *ctx) {
    return map_forge_app_stage_transition(ctx,
                                          MAP_FORGE_APP_STAGE_SUBSYSTEMS_READY,
                                          MAP_FORGE_APP_STAGE_RUNTIME_STARTED,
                                          "map_forge_runtime_start",
                                          __func__);
}

int map_forge_app_run_loop(MapForgeAppContext *ctx) {
    if (!ctx) {
        return 1;
    }
    if (ctx->stage != MAP_FORGE_APP_STAGE_RUNTIME_STARTED) {
        fprintf(stderr,
                "map_forge: run loop invoked before runtime_start (stage=%d)\n",
                (int)ctx->stage);
        return 1;
    }
    ctx->legacy_delegate_invoked = 1;
    ctx->dispatch_count = 1;
    ctx->exit_code = app_run_legacy();
    ctx->last_dispatch_exit_code = ctx->exit_code;
    ctx->dispatch_succeeded = (ctx->exit_code == 0) ? 1 : 0;
    if (!map_forge_app_stage_transition(ctx,
                                        MAP_FORGE_APP_STAGE_RUNTIME_STARTED,
                                        MAP_FORGE_APP_STAGE_LOOP_COMPLETED,
                                        "map_forge_app_run_loop",
                                        __func__)) {
        ctx->wrapper_error = MAP_FORGE_WRAPPER_ERROR_STAGE_TRANSITION_FAILED;
        return 1;
    }
    return ctx->exit_code;
}

void map_forge_app_shutdown(MapForgeAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->shutdown_executed) {
        return;
    }
    ctx->shutdown_executed = 1;
    if (ctx->stage != MAP_FORGE_APP_STAGE_INIT &&
        ctx->stage != MAP_FORGE_APP_STAGE_SHUTDOWN_COMPLETED) {
        ctx->stage = MAP_FORGE_APP_STAGE_SHUTDOWN_COMPLETED;
    }
}

int map_forge_app_main(void) {
    MapForgeAppContext app;

    if (!map_forge_app_bootstrap(&app)) {
        app.wrapper_error = MAP_FORGE_WRAPPER_ERROR_BOOTSTRAP_FAILED;
        map_forge_log_wrapper_error(__func__,
                                    (MapForgeWrapperError)app.wrapper_error,
                                    app.stage,
                                    app.exit_code,
                                    "bootstrap failed");
        return app.exit_code;
    }
    if (!map_forge_app_config_load(&app)) {
        app.wrapper_error = MAP_FORGE_WRAPPER_ERROR_CONFIG_LOAD_FAILED;
        map_forge_log_wrapper_error(__func__,
                                    (MapForgeWrapperError)app.wrapper_error,
                                    app.stage,
                                    app.exit_code,
                                    "config load failed");
        map_forge_app_shutdown(&app);
        return app.exit_code;
    }
    if (!map_forge_app_state_seed(&app)) {
        app.wrapper_error = MAP_FORGE_WRAPPER_ERROR_STATE_SEED_FAILED;
        map_forge_log_wrapper_error(__func__,
                                    (MapForgeWrapperError)app.wrapper_error,
                                    app.stage,
                                    app.exit_code,
                                    "state seed failed");
        map_forge_app_shutdown(&app);
        return app.exit_code;
    }
    if (!map_forge_app_subsystems_init(&app)) {
        app.wrapper_error = MAP_FORGE_WRAPPER_ERROR_SUBSYSTEMS_INIT_FAILED;
        map_forge_log_wrapper_error(__func__,
                                    (MapForgeWrapperError)app.wrapper_error,
                                    app.stage,
                                    app.exit_code,
                                    "subsystems init failed");
        map_forge_app_shutdown(&app);
        return app.exit_code;
    }
    if (!map_forge_runtime_start(&app)) {
        app.wrapper_error = MAP_FORGE_WRAPPER_ERROR_RUNTIME_START_FAILED;
        map_forge_log_wrapper_error(__func__,
                                    (MapForgeWrapperError)app.wrapper_error,
                                    app.stage,
                                    app.exit_code,
                                    "runtime start failed");
        map_forge_app_shutdown(&app);
        return app.exit_code;
    }

    app.exit_code = map_forge_app_run_loop(&app);
    if (app.exit_code != 0 && app.wrapper_error == MAP_FORGE_WRAPPER_ERROR_NONE) {
        app.wrapper_error = MAP_FORGE_WRAPPER_ERROR_RUN_LOOP_FAILED;
        map_forge_log_wrapper_error(__func__,
                                    (MapForgeWrapperError)app.wrapper_error,
                                    app.stage,
                                    app.exit_code,
                                    "run loop failed");
    }
    map_forge_app_shutdown(&app);
    fprintf(stderr,
            "map_forge: wrapper exit stage=%d exit_code=%d dispatch_count=%d dispatch_ok=%d last_dispatch_exit=%d wrapper_error=%d\n",
            (int)app.stage,
            app.exit_code,
            app.dispatch_count,
            app.dispatch_succeeded,
            app.last_dispatch_exit_code,
            app.wrapper_error);
    return app.exit_code;
}
