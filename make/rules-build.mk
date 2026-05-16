app: $(TARGET)

$(SHARED_BUILD_DIR):
	@mkdir -p "$@"

$(CORE_BASE_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_BASE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_BASE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_BASE_DIR)/build/libcore_base.a" "$@"

$(CORE_IO_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_IO_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_IO_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_IO_DIR)/build/libcore_io.a" "$@"

$(CORE_DATA_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_DATA_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_DATA_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_DATA_DIR)/build/libcore_data.a" "$@"

$(CORE_SPACE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_SPACE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_SPACE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_SPACE_DIR)/build/libcore_space.a" "$@"

$(CORE_PACK_LIB): $(CORE_IO_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_PACK_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_PACK_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_PACK_DIR)/build/libcore_pack.a" "$@"

$(CORE_TIME_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_TIME_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_TIME_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_TIME_DIR)/build/libcore_time.a" "$@"

$(CORE_QUEUE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_QUEUE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_QUEUE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_QUEUE_DIR)/build/libcore_queue.a" "$@"

$(CORE_SCHED_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_SCHED_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_SCHED_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_SCHED_DIR)/build/libcore_sched.a" "$@"

$(CORE_JOBS_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_JOBS_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_JOBS_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_JOBS_DIR)/build/libcore_jobs.a" "$@"

$(CORE_WORKERS_LIB): $(CORE_QUEUE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_WORKERS_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_WORKERS_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_WORKERS_DIR)/build/libcore_workers.a" "$@"

$(CORE_WAKE_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_WAKE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_WAKE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_WAKE_DIR)/build/libcore_wake.a" "$@"

$(CORE_KERNEL_LIB): $(CORE_SCHED_LIB) $(CORE_JOBS_LIB) $(CORE_WAKE_LIB) $(CORE_QUEUE_LIB) $(CORE_TIME_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_KERNEL_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_KERNEL_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_KERNEL_DIR)/build/libcore_kernel.a" "$@"

$(CORE_TRACE_LIB): $(CORE_PACK_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_TRACE_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_TRACE_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_TRACE_DIR)/build/libcore_trace.a" "$@"

$(CORE_THEME_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_THEME_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_THEME_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_THEME_DIR)/build/libcore_theme.a" "$@"

$(CORE_FONT_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_FONT_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_FONT_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_FONT_DIR)/build/libcore_font.a" "$@"

$(CORE_VIEWPORT2D_LIB): $(CORE_BASE_LIB) | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(CORE_VIEWPORT2D_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(CORE_VIEWPORT2D_DIR)" CC="$(SHARED_CC)"
	@cp "$(CORE_VIEWPORT2D_DIR)/build/libcore_viewport2d.a" "$@"

$(KIT_RUNTIME_DIAG_LIB): | $(SHARED_BUILD_DIR)
	@$(MAKE) -C "$(KIT_RUNTIME_DIAG_DIR)" clean CC="$(SHARED_CC)"
	@$(MAKE) -C "$(KIT_RUNTIME_DIAG_DIR)" CC="$(SHARED_CC)"
	@cp "$(KIT_RUNTIME_DIAG_DIR)/build/libkit_runtime_diag.a" "$@"

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

$(KIT_RENDER_OBJ): $(KIT_RENDER_DIR)/src/kit_render.c
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

