#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE_MAKE="${ROOT_DIR}/make/package-macos.mk"
PATHS_MAKE="${ROOT_DIR}/make/paths.mk"
LAUNCHER_NATIVE="${ROOT_DIR}/tools/packaging/macos/mapforge-launcher.c"
LAUNCHER_SCRIPT="${ROOT_DIR}/tools/packaging/macos/mapforge-launcher"
INFO_PLIST="${ROOT_DIR}/tools/packaging/macos/Info.plist"

fail() { echo "Main Edit package contract check failed: $1" >&2; exit 1; }
check() { rg --fixed-strings --quiet -- "$1" "$2" || fail "missing '$1' in $2"; }

check "package-desktop-main-edit" "${PACKAGE_MAKE}"
check "package-desktop-main-edit-self-test" "${PACKAGE_MAKE}"
check "package-desktop-main-edit-refresh" "${PACKAGE_MAKE}"
check "MAIN_EDIT_APP_NAME := Carta Main Edit.app" "${PATHS_MAKE}"
check "MAIN_EDIT_BUNDLE_ID := com.cosm.carta.main-edit" "${PATHS_MAKE}"
check "MAIN_EDIT_RUNTIME_NAMESPACE := MapForge-Main-Edit" "${PATHS_MAKE}"
check "PACKAGE_PROFILE ?= standard" "${PATHS_MAKE}"
check "write-identity" "${PACKAGE_MAKE}"
check "verify-identity" "${PACKAGE_MAKE}"
check "Source changed during Main Edit packaging" "${PACKAGE_MAKE}"
check "Refusing canonical Desktop destination" "${PACKAGE_MAKE}"
check "process-audit" "${PACKAGE_MAKE}"
check 'helper_tool="$(PACKAGE_TOOLS_DIR)/$$helper_name"' "${PACKAGE_MAKE}"
check "-exec codesign" "${PACKAGE_MAKE}"
check "MAPFORGE_PACKAGE_PROFILE" "${LAUNCHER_NATIVE}"
check 'Application Support/${RUNTIME_NAMESPACE}' "${LAUNCHER_SCRIPT}"
check 'Library/Logs/${LOG_NAMESPACE}' "${LAUNCHER_SCRIPT}"
check 'ALLOW_DEV_REGIONS_FALLBACK=0' "${LAUNCHER_SCRIPT}"
check "<string>mapforge-launcher</string>" "${INFO_PLIST}"

echo "Main Edit package contract checks passed"
