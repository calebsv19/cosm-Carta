app: $(TARGET)

$(SHARED_BUILD_DIR):
	@mkdir -p "$@"

$(CORE_BASE_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_BASE_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_base" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_base"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_base/libcore_base.a" "$@"

$(CORE_IO_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_IO_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_io" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_io"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_io/libcore_io.a" "$@"

$(CORE_DATA_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_DATA_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_data" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_data"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_data/libcore_data.a" "$@"

$(CORE_SPACE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_SPACE_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_space" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_space"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_space/libcore_space.a" "$@"

$(CORE_PACK_LIB): $(CORE_IO_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_PACK_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_pack" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_pack"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_pack/libcore_pack.a" "$@"

$(CORE_TIME_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_TIME_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_time" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_time"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_time/libcore_time.a" "$@"

$(CORE_QUEUE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_QUEUE_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_queue" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_queue"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_queue/libcore_queue.a" "$@"

$(CORE_SCHED_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_SCHED_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_sched" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_sched"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_sched/libcore_sched.a" "$@"

$(CORE_JOBS_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_JOBS_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_jobs" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_jobs"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_jobs/libcore_jobs.a" "$@"

$(CORE_WORKERS_LIB): $(CORE_QUEUE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_WORKERS_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_workers" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_workers"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_workers/libcore_workers.a" "$@"

$(CORE_WAKE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_WAKE_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_wake" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_wake"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_wake/libcore_wake.a" "$@"

$(CORE_KERNEL_LIB): $(CORE_SCHED_LIB) $(CORE_JOBS_LIB) $(CORE_WAKE_LIB) $(CORE_QUEUE_LIB) $(CORE_TIME_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_KERNEL_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_kernel" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_kernel"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_kernel/libcore_kernel.a" "$@"

$(CORE_TRACE_LIB): $(CORE_PACK_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_TRACE_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_trace" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_trace"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_trace/libcore_trace.a" "$@"

$(CORE_HEADLESS_JOB_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_HEADLESS_JOB_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_headless_job" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_headless_job"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_headless_job/libcore_headless_job.a" "$@"

$(CORE_THEME_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_THEME_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_theme" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_theme"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_theme/libcore_theme.a" "$@"

$(CORE_FONT_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_FONT_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_font" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_font"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_font/libcore_font.a" "$@"

$(CORE_PANE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_PANE_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_pane" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_pane"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_pane/libcore_pane.a" "$@"

$(CORE_VIEWPORT2D_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_VIEWPORT2D_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_viewport2d" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/core_viewport2d"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/core_viewport2d/libcore_viewport2d.a" "$@"

$(KIT_RUNTIME_DIAG_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(KIT_RUNTIME_DIAG_DIR)" CC="$(SHARED_CC)" OBJ_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/kit_runtime_diag" BUILD_DIR="$(abspath $(SHARED_BUILD_DIR))/objects/kit_runtime_diag"
	@cp "$(abspath $(SHARED_BUILD_DIR))/objects/kit_runtime_diag/libkit_runtime_diag.a" "$@"

$(APP_COMPILER_STAMP): $(APP_COMPILER_DEP)
	@mkdir -p $(dir $@)
	@touch $@

$(TARGET): $(LINK_OBJS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) $^ -o $@ $(LDLIBS)

$(APP_OBJ_DIR)/%.o: src/%.c $(APP_COMPILER_STAMP)
	@mkdir -p $(dir $@)
	$(APP_CC) $(APP_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(HOST_BUILD_ROOT)/vk_renderer/%.o: $(VK_RENDERER_RESOLVED_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(HOST_BUILD_ROOT)/vk_runtime/%.o: $(VK_RUNTIME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -std=c11 $(VK_RUNTIME_VERSION_CFLAG) -MMD -MP -Iinclude -c $< -o $@

$(KIT_UI_OBJ): $(KIT_UI_DIR)/src/kit_ui.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_RENDER_OBJ): $(KIT_RENDER_DIR)/src/kit_render.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_RENDER_VALIDATION_OBJ): $(KIT_RENDER_DIR)/src/kit_render_command_validation.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_RENDER_BACKEND_NULL_OBJ): $(KIT_RENDER_DIR)/src/kit_render_backend_null.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_RENDER_BACKEND_VK_OBJ): $(KIT_RENDER_DIR)/src/kit_render_backend_vk.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_RENDER_EXTERNAL_TEXT_OBJ): $(KIT_RENDER_DIR)/src/kit_render_external_text.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_WORKSPACE_AUTHORING_OBJ): $(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_WORKSPACE_AUTHORING_UI_OVERLAY_OBJ): $(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(KIT_WORKSPACE_AUTHORING_UI_FONT_THEME_OBJ): $(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@


clean:
	rm -rf build

$(HOST_BUILD_ROOT)/kit_ui/%.o: $(KIT_UI_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -Iinclude -c $< -o $@

$(HOST_BUILD_ROOT)/kit_pane/%.o: $(KIT_PANE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CFLAGS) -MMD -MP -c $< -o $@
