RELEASE_MAKE := $(MAKE) --no-print-directory TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" BUILD_TOOLCHAIN="$(RELEASE_TOOLCHAIN)" PACKAGE_TOOLCHAIN="$(RELEASE_TOOLCHAIN)"

release-contract:
	@echo "Release contract:"
	@echo "  product_name: $(RELEASE_PRODUCT_NAME)"
	@echo "  program_key: $(RELEASE_PROGRAM_KEY)"
	@echo "  version_file: $(RELEASE_VERSION_FILE)"
	@echo "  version: $(RELEASE_VERSION)"
	@echo "  channel: $(RELEASE_CHANNEL)"
	@echo "  bundle_id: $(RELEASE_BUNDLE_ID)"
	@echo "  artifact_base: $(RELEASE_ARTIFACT_BASENAME)"
	@echo "  release_codesign_identity: $(RELEASE_CODESIGN_IDENTITY)"
	@echo "  sign_identity_set: $$( [ -n \"$(APPLE_SIGN_IDENTITY)\" ] && echo yes || echo no )"
	@echo "  notary_profile_set: $$( [ -n \"$(APPLE_NOTARY_PROFILE)\" ] && echo yes || echo no )"
	@echo "  team_id_set: $$( [ -n \"$(APPLE_TEAM_ID)\" ] && echo yes || echo no )"

release-clean:
	@rm -rf "$(RELEASE_DIR)"
	@echo "Release output cleaned: $(RELEASE_DIR)"

release-build:
	@$(RELEASE_MAKE) release-build-internal

release-bundle-audit:
	@$(RELEASE_MAKE) release-bundle-audit-internal

release-sign:
	@$(RELEASE_MAKE) release-sign-internal

release-verify:
	@$(RELEASE_MAKE) release-verify-internal

release-verify-signed:
	@$(RELEASE_MAKE) release-verify-signed-internal

release-notarize:
	@$(RELEASE_MAKE) release-notarize-internal

release-staple:
	@$(RELEASE_MAKE) release-staple-internal

release-verify-notarized:
	@$(RELEASE_MAKE) release-verify-notarized-internal

release-artifact:
	@$(RELEASE_MAKE) release-artifact-internal

release-distribute:
	@$(RELEASE_MAKE) release-distribute-internal

release-desktop-refresh:
	@$(RELEASE_MAKE) release-desktop-refresh-internal

release-build-internal: clean app
	@echo "Release build complete: $(TARGET)"

release-bundle-audit-internal: package-desktop-self-test
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "bundle id mismatch: expected $(RELEASE_BUNDLE_ID), got $$(cat "$(RELEASE_DIR)/bundle_id.txt")"; exit 1)
	@env -i HOME="$(HOME)" PATH="$(PATH)" "$(PACKAGE_MACOS_DIR)/mapforge-launcher" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@runtime_dir="$$(/usr/bin/grep '^MAPFORGE_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$runtime_dir" ]; then echo "runtime dir missing from print-config"; exit 1; fi; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac; \
	case "$$runtime_dir" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "runtime dir is not user-writable rooted: $$runtime_dir"; exit 1;; esac
	@theme_path="$$(/usr/bin/grep '^MAPFORGE_THEME_PERSIST_PATH=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$theme_path" ]; then echo "theme persist path missing from print-config"; exit 1; fi; \
	case "$$theme_path" in *"/Contents/Resources"*) echo "theme persist path incorrectly points into app bundle: $$theme_path"; exit 1;; esac; \
	case "$$theme_path" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "theme persist path is not user-writable rooted: $$theme_path"; exit 1;; esac
	@/usr/bin/grep -q '^MAPFORGE_SELECTED_REGIONS_REASON=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing regions-selection diagnostics"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/mapforge-bin" > "$(RELEASE_DIR)/otool_mapforge_bin.txt"
	@if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_mapforge_bin.txt"; then \
		echo "non-portable dylib dependency detected in $(PACKAGE_MACOS_DIR)/mapforge-bin"; \
		cat "$(RELEASE_DIR)/otool_mapforge_bin.txt"; \
		exit 1; \
	fi
	@for file in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		base="$$(/usr/bin/basename "$$file")"; \
		otool -L "$$file" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable dylib dependency detected in $$file"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
		base="$$(/usr/bin/basename "$$helper_tool")"; \
		otool -L "$$helper_tool" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable helper-tool dependency detected in $$helper_tool"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@echo "release-bundle-audit passed."

release-sign-internal: release-bundle-audit-internal
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/mapforge-launcher"; \
		for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$helper_tool"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"; \
	else \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/mapforge-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/mapforge-launcher"; \
		for helper_tool in $(PACKAGED_HELPER_TOOLS); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$$helper_tool"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_APP_DIR)"; \
	fi
	@echo "release-sign complete."

release-verify-internal:
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-verify note: ad-hoc identity in use; skipping spctl Gatekeeper assessment"; \
	else \
		spctl_output="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; \
		spctl_status=$$?; \
		if [ $$spctl_status -ne 0 ]; then \
			if printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "internal error in Code Signing subsystem"; then \
				echo "release-verify note: spctl internal subsystem error on this host; codesign verification remains authoritative"; \
			else \
				printf '%s\n' "$$spctl_output"; \
				exit $$spctl_status; \
			fi; \
		else \
			printf '%s\n' "$$spctl_output"; \
		fi; \
	fi
	@echo "release-verify passed."

release-verify-signed-internal: release-sign-internal release-verify-internal
	@echo "release-verify-signed passed."

release-notarize-internal: release-sign-internal
	@if [ -z "$(APPLE_NOTARY_PROFILE)" ]; then \
		echo "APPLE_NOTARY_PROFILE is required for release-notarize"; \
		exit 1; \
	fi
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-notarize requires a real Developer ID signing identity (APPLE_SIGN_IDENTITY)"; \
		exit 1; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@submission_json="$$(xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json)"; \
	echo "$$submission_json" > "$(RELEASE_DIR)/notary_submit.json"; \
	status="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*"status"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | /usr/bin/tail -n 1)"; \
	if [ "$$status" != "Accepted" ]; then \
		submission_id="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*"id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | /usr/bin/head -n 1)"; \
		echo "release-notarize failed: status=$$status id=$$submission_id"; \
		if [ -n "$$submission_id" ]; then \
			xcrun notarytool log "$$submission_id" --keychain-profile "$(APPLE_NOTARY_PROFILE)" > "$(RELEASE_DIR)/notary_log_$$submission_id.json" || true; \
			echo "notary log: $(RELEASE_DIR)/notary_log_$$submission_id.json"; \
		fi; \
		exit 1; \
	fi
	@echo "release-notarize passed."

release-staple-internal:
	@attempt=1; \
	while [ $$attempt -le "$(STAPLE_MAX_ATTEMPTS)" ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)"; then \
			break; \
		fi; \
		if [ $$attempt -eq "$(STAPLE_MAX_ATTEMPTS)" ]; then \
			echo "release-staple failed after $$attempt attempts"; \
			exit 1; \
		fi; \
		echo "release-staple retry $$attempt/$(STAPLE_MAX_ATTEMPTS) in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep "$(STAPLE_RETRY_DELAY_SEC)"; \
		attempt=$$((attempt + 1)); \
	done
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-staple passed."

release-verify-notarized-internal: release-verify-internal
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact-internal:
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP).sha256"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "platform=$(RELEASE_PLATFORM)"; \
		echo "arch=$(RELEASE_ARCH)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "artifact=$(RELEASE_APP_ZIP)"; \
		echo "sha256_file=$(RELEASE_APP_ZIP).sha256"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute-internal: release-notarize-internal release-staple-internal release-verify-notarized-internal release-artifact-internal
	@echo "release-distribute passed."

release-desktop-refresh-internal: package-desktop
	@if [ ! -d "$(PACKAGE_APP_DIR)" ]; then \
		echo "release-desktop-refresh requires an existing built app at $(PACKAGE_APP_DIR)"; \
		echo "run release-distribute first"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Release app refreshed at $(DESKTOP_APP_DIR)"

