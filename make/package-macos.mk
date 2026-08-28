package-desktop: tools-build graph-build
	@$(MAKE) --no-print-directory TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" app
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_HELPERS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $(PACKAGE_BUNDLE_ID)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleName $(PACKAGE_DISPLAY_NAME)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :CFBundleDisplayName string $(PACKAGE_DISPLAY_NAME)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $(RELEASE_VERSION)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :MapForgePackageProfile string $(PACKAGE_PROFILE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :MapForgeRuntimeNamespace string $(PACKAGE_RUNTIME_NAMESPACE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :MapForgeLogNamespace string $(PACKAGE_LOG_NAMESPACE)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@/usr/libexec/PlistBuddy -c "Add :MapForgeBuildLabel string $(PACKAGE_BUILD_LABEL)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(PACKAGE_BIN)" "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@cp "$(PACKAGE_LAUNCHER_SCRIPT_SRC)" "$(PACKAGE_LAUNCHER_SCRIPT_PATH)"
	@$(HOST_CC) -std=c11 -Os -Wall -Wextra -Werror -Wpedantic \
		-DMAPFORGE_PACKAGE_PROFILE='"$(PACKAGE_PROFILE)"' \
		-DMAPFORGE_RUNTIME_NAMESPACE='"$(PACKAGE_RUNTIME_NAMESPACE)"' \
		-DMAPFORGE_LOG_NAMESPACE='"$(PACKAGE_LOG_NAMESPACE)"' \
		-DMAPFORGE_BUILD_LABEL='"$(PACKAGE_BUILD_LABEL)"' \
		"$(PACKAGE_LAUNCHER_NATIVE_SRC)" -o "$(PACKAGE_MACOS_DIR)/mapforge-launcher"
	@chmod +x "$(PACKAGE_MACOS_DIR)/mapforge-launcher" "$(PACKAGE_MACOS_DIR)/mapforge-bin" "$(PACKAGE_LAUNCHER_SCRIPT_PATH)"
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
	@for helper_name in $(PACKAGED_HELPER_TOOL_NAMES); do \
		helper_tool="$(PACKAGE_TOOLS_DIR)/$$helper_name"; \
		PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" /bin/sh "$(PACKAGE_DYLIB_BUNDLER)" "$$helper_tool" "$(PACKAGE_FRAMEWORKS_DIR)"; \
	done
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_RESOLVED_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_RESOLVED_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@echo "Region payload bundling disabled; app ships without embedded region packs."
	@/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' \
		-exec codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none {} \;
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-bin"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-launcher"
	@for helper_name in $(PACKAGED_HELPER_TOOL_NAMES); do \
		helper_tool="$(PACKAGE_TOOLS_DIR)/$$helper_name"; \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$helper_tool"; \
	done
	@if [ "$(PACKAGE_EMBED_BUILD_IDENTITY)" = "1" ]; then \
		python3 "$(MEW1_TOOL)" write-identity \
			--output "$(PACKAGE_RESOURCES_DIR)/build_identity.json" \
			--source-root "$(CURDIR)" --binary "$(PACKAGE_MACOS_DIR)/mapforge-bin" \
			--profile "$(PACKAGE_PROFILE)" --program map_forge --product Carta \
			--version "$(RELEASE_VERSION)" --architecture "$(TARGET_ARCH)" \
			--toolchain "$(PACKAGE_TOOLCHAIN)" --build-label "$(PACKAGE_BUILD_LABEL)"; \
	fi
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/mapforge-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_LAUNCHER_SCRIPT_PATH)" || (echo "Missing launcher resource"; exit 1)
	@file "$(PACKAGE_MACOS_DIR)/mapforge-launcher" | rg -q 'Mach-O' || (echo "Launcher must be native Mach-O code"; exit 1)
	@file "$(PACKAGE_LAUNCHER_SCRIPT_PATH)" | rg -q 'shell script' || (echo "Launcher resource must be a shell script"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/mapforge-bin" || (echo "Missing mapforge-bin"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleShortVersionString' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(RELEASE_VERSION)" || (echo "Bundle version mismatch"; exit 1)
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_BUNDLE_ID)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_DISPLAY_NAME)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :MapForgePackageProfile' "$(PACKAGE_CONTENTS_DIR)/Info.plist")" = "$(PACKAGE_PROFILE)"
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f "$(PACKAGE_RESOURCES_DIR)/assets/fonts/Montserrat-Regular.ttf" || (echo "Missing bundled Montserrat"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/app.config.json" || (echo "Missing bundled app config"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_region" || (echo "Missing bundled mapforge_region tool"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_region_validate" || (echo "Missing bundled mapforge_region_validate tool"; exit 1)
	@test -x "$(PACKAGE_TOOLS_DIR)/mapforge_graph" || (echo "Missing bundled mapforge_graph tool"; exit 1)
	@test ! -d "$(PACKAGE_RESOURCES_DIR)/tools" || (echo "Executable tools must not be packaged under Resources"; exit 1)
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

package-desktop-main-edit:
	@test -f "$(MEW1_TOOL)" || (echo "Missing shared MEW1 helper: $(MEW1_TOOL)"; exit 1)
	@before="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	$(MAKE) package-desktop-smoke \
		DIST_DIR="$(MAIN_EDIT_DIST_DIR)" PACKAGE_APP_NAME="$(MAIN_EDIT_APP_NAME)" \
		PACKAGE_DISPLAY_NAME="$(MAIN_EDIT_DISPLAY_NAME)" PACKAGE_BUNDLE_ID="$(MAIN_EDIT_BUNDLE_ID)" \
		PACKAGE_PROFILE="$(MAIN_EDIT_PROFILE)" PACKAGE_RUNTIME_NAMESPACE="$(MAIN_EDIT_RUNTIME_NAMESPACE)" \
		PACKAGE_LOG_NAMESPACE="$(MAIN_EDIT_LOG_NAMESPACE)" PACKAGE_BUILD_LABEL="$(MAIN_EDIT_BUILD_LABEL)" \
		PACKAGE_EMBED_BUILD_IDENTITY=1 || exit 1; \
	after="$$(python3 "$(MEW1_TOOL)" fingerprint --repo "$(CURDIR)")"; \
	if [ "$$before" != "$$after" ]; then \
		rm -rf "$(MAIN_EDIT_APP_DIR)"; \
		echo "Source changed during Main Edit packaging; discarded generated package."; \
		exit 1; \
	fi
	@echo "Main Edit desktop package ready: $(MAIN_EDIT_APP_DIR)"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_BUNDLE_ID)"
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleDisplayName' "$(MAIN_EDIT_APP_DIR)/Contents/Info.plist")" = "$(MAIN_EDIT_DISPLAY_NAME)"
	@test -f "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json"
	@python3 "$(MEW1_TOOL)" verify-identity \
		--identity "$(MAIN_EDIT_APP_DIR)/Contents/Resources/build_identity.json" \
		--source-root "$(CURDIR)" --binary "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/mapforge-bin" \
		--profile "$(MAIN_EDIT_PROFILE)" --program map_forge --product Carta --version "$(RELEASE_VERSION)"
	@set -e; \
	fake_home="$(CURDIR)/$(MAIN_EDIT_SELF_TEST_DIR)/home"; \
	rm -rf "$$fake_home"; mkdir -p "$$fake_home"; \
	config="$$(HOME="$$fake_home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/mapforge-launcher" --print-config)"; \
	printf '%s\n' "$$config"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_PACKAGE_PROFILE=$(MAIN_EDIT_PROFILE)"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_RUNTIME_NAMESPACE=$(MAIN_EDIT_RUNTIME_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_LOG_NAMESPACE=$(MAIN_EDIT_LOG_NAMESPACE)"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_BUILD_LABEL=$(MAIN_EDIT_BUILD_LABEL)"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_ALLOW_DEV_REGIONS_FALLBACK=0"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_RUNTIME_DIR=$$fake_home/Library/Application Support/$(MAIN_EDIT_RUNTIME_NAMESPACE)/runtime"; \
	printf '%s\n' "$$config" | grep -Fqx "MAPFORGE_REGIONS_DIR=$$fake_home/Library/Application Support/$(MAIN_EDIT_RUNTIME_NAMESPACE)/regions"; \
	printf '%s\n' "$$config" | grep -Fqx "LOG_FILE=$$fake_home/Library/Logs/$(MAIN_EDIT_LOG_NAMESPACE)/launcher.log"
	@HOME="$(CURDIR)/$(MAIN_EDIT_SELF_TEST_DIR)/home" "$(MAIN_EDIT_APP_DIR)/Contents/MacOS/mapforge-launcher" --self-test
	@codesign --verify --deep --strict "$(MAIN_EDIT_APP_DIR)"
	@echo "package-desktop-main-edit-self-test passed."

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@test "$(MAIN_EDIT_DESKTOP_APP_DIR)" != "$(HOME)/Desktop/Carta.app" || (echo "Refusing canonical Desktop destination"; exit 1)
	@mkdir -p "$(dir $(MAIN_EDIT_PROCESS_RECEIPT))"
	@python3 "$(MEW1_TOOL)" process-audit --match "$(MAIN_EDIT_DISPLAY_NAME).app" --path "$(MAIN_EDIT_DESKTOP_APP_DIR)" > "$(MAIN_EDIT_PROCESS_RECEIPT)"
	@if grep -Fq '"running": true' "$(MAIN_EDIT_PROCESS_RECEIPT)"; then \
		echo "Refusing to replace a running $(MAIN_EDIT_APP_NAME); process receipt: $(MAIN_EDIT_PROCESS_RECEIPT)"; exit 1; \
	fi
	@mkdir -p "$$(dirname "$(MAIN_EDIT_DESKTOP_APP_DIR)")"
	@rm -rf "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(MAIN_EDIT_APP_DIR)" "$(MAIN_EDIT_DESKTOP_APP_DIR)"
	@echo "Refreshed $(MAIN_EDIT_APP_NAME) at $(MAIN_EDIT_DESKTOP_APP_DIR)"

main-edit-package-contract-checks:
	@./tests/run_main_edit_package_contract_checks.sh

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
