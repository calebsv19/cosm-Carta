package-desktop: tools-build graph-build
	@$(MAKE) --no-print-directory TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" app
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)" "$(PACKAGE_TOOLS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_BIN)" "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/mapforge-launcher"
	@chmod +x "$(PACKAGE_MACOS_DIR)/mapforge-launcher" "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		/usr/bin/iconutil -c icns -o "$(PACKAGE_BUNDLED_ICON_PATH)" "$(PACKAGE_APP_ICONSET_SRC)" || exit 1; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "warning: no app icon source found at $(PACKAGE_APP_ICON_SRC) or $(PACKAGE_APP_ICONSET_SRC)"; \
	fi
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" /bin/sh "$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/mapforge-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/assets" "$(PACKAGE_RESOURCES_DIR)/shared/assets" "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/regions"
	@cp -R assets/fonts "$(PACKAGE_RESOURCES_DIR)/assets/"
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@cp -R "$(SHARED_ROOT)/assets/fonts" "$(PACKAGE_RESOURCES_DIR)/shared/assets/"
	@cp "$(TOOL_TARGET)" "$(PACKAGE_TOOLS_DIR)/mapforge_region"
	@cp "$(REGION_VALIDATE_TARGET)" "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate"
	@cp "$(GRAPH_TARGET)" "$(PACKAGE_TOOLS_DIR)/mapforge_graph"
	@chmod +x "$(PACKAGE_TOOLS_DIR)/mapforge_region" "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" "$(PACKAGE_TOOLS_DIR)/mapforge_graph"
	@for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
		PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" /bin/sh "$(PACKAGE_DYLIB_BUNDLER)" "$$helper_tool" "$(PACKAGE_FRAMEWORKS_DIR)"; \
	done
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_RESOLVED_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_RESOLVED_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@echo "Region payload bundling disabled; app ships without embedded region packs."
	@for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-launcher"
	@for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$helper_tool"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/mapforge-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/mapforge-bin" || (echo "Missing mapforge-bin"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f "$(PACKAGE_RESOURCES_DIR)/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing bundled Montserrat"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/app.config.json" || (echo "Missing bundled app config"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_region" || (echo "Missing bundled mapforge_region tool"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" || (echo "Missing bundled mapforge_region_validate tool"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_graph" || (echo "Missing bundled mapforge_graph tool"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing runtime shader"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime state dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/regions" || (echo "Missing regions dir"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libSDL2-2.0.0.dylib" || (echo "Missing bundled SDL2 dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled MoltenVK dylib"; exit 1)
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		for packaged_bin in "$(PACKAGE_MACOS_DIR)/mapforge-bin" "$(PACKAGE_TOOLS_DIR)/mapforge_region" "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" "$(PACKAGE_TOOLS_DIR)/mapforge_graph"; do \
			actual_archs="$$(/usr/bin/lipo -archs "$$packaged_bin" 2>/dev/null || true)"; \
			case "$$actual_archs" in \
				*"$(TARGET_ARCH)"*) ;; \
				*) echo "arch mismatch for $$packaged_bin: expected $(TARGET_ARCH), got '$$actual_archs'"; exit 1 ;; \
			esac; \
		done; \
	fi
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)" || (echo "codesign verification failed"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@mkdir -p build/vulkan-rollout-package
	@MAPFORGE_PACKAGE_VALIDATION_LAYER_DYLIB="$(VULKAN_VALIDATION_DYLD_PATH)/libVkLayer_khronos_validation.dylib" MAPFORGE_PACKAGE_REQUIRE_VULKAN_ROLLOUT="$(VK_RUNTIME_AVAILABLE)" MAPFORGE_VULKAN_ROLLOUT_INITIAL_CAPTURE="$(abspath build/vulkan-rollout-package/initial.bmp)" MAPFORGE_VULKAN_ROLLOUT_RESIZED_CAPTURE="$(abspath build/vulkan-rollout-package/resized.bmp)" "$(PACKAGE_MACOS_DIR)/mapforge-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@if [ "$(VK_RUNTIME_AVAILABLE)" = "1" ]; then \
		python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)" --initial-capture build/vulkan-rollout-package/initial.bmp --resized-capture build/vulkan-rollout-package/resized.bmp; \
	fi
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop app sync complete."

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(DESKTOP_APP_DIR)"
	@echo "Removed desktop copy at $(DESKTOP_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"
