#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <app_bin> <frameworks_dir>" >&2
    exit 1
fi

APP_BIN="$1"
FRAMEWORKS_DIR="$2"
AWK_BIN="/usr/bin/awk"
GREP_BIN="/usr/bin/grep"
CP_BIN="/bin/cp"
CHMOD_BIN="/bin/chmod"
BASENAME_BIN="/usr/bin/basename"
OTOOL_BIN="/usr/bin/otool"
INSTALL_NAME_TOOL_BIN="/usr/bin/install_name_tool"
MKDIR_BIN="/bin/mkdir"
SEARCH_ROOTS_LIST="${PACKAGE_DEP_SEARCH_ROOTS:-/opt/homebrew:/usr/local}"

"$MKDIR_BIN" -p "$FRAMEWORKS_DIR"

WORK_TMP_DIR="${TMPDIR:-/tmp}/mapforge_bundle_dylibs.$$"
"$MKDIR_BIN" -p "$WORK_TMP_DIR"
QUEUE_FILE="$WORK_TMP_DIR/queue.txt"
SEEN_FILE="$WORK_TMP_DIR/seen.txt"
touch "$QUEUE_FILE" "$SEEN_FILE"

resolve_search_root_dep() {
    dep_base="$1"
    old_ifs="${IFS}"
    IFS=":"
    for search_root in $SEARCH_ROOTS_LIST; do
        [ -n "$search_root" ] || continue
        if [ -f "$search_root/lib/$dep_base" ]; then
            IFS="${old_ifs}"
            printf '%s\n' "$search_root/lib/$dep_base"
            return 0
        fi
    done
    IFS="${old_ifs}"
    return 1
}

cleanup() {
    rm -rf "$WORK_TMP_DIR" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

replacement_path_for() {
    current_file="$1"
    dep_base="$2"

    case "$current_file" in
        "$FRAMEWORKS_DIR"/*)
            printf '%s\n' "@loader_path/$dep_base"
            ;;
        */Contents/MacOS/*)
            printf '%s\n' "@loader_path/../Frameworks/$dep_base"
            ;;
        */Contents/Helpers/*)
            printf '%s\n' "@loader_path/../Frameworks/$dep_base"
            ;;
        */Contents/Resources/*)
            printf '%s\n' "@loader_path/../../Frameworks/$dep_base"
            ;;
        *)
            printf '%s\n' "@loader_path/$dep_base"
            ;;
    esac
}

echo "$APP_BIN" >>"$QUEUE_FILE"

while IFS= read -r current_file; do
    [ -n "$current_file" ] || continue
    if "$GREP_BIN" -Fxq "$current_file" "$SEEN_FILE"; then
        continue
    fi
    echo "$current_file" >>"$SEEN_FILE"

    deps="$("$OTOOL_BIN" -L "$current_file" | "$AWK_BIN" 'NR>1 {print $1}' | "$GREP_BIN" -E '^/opt/homebrew|^/usr/local|^@rpath/' || true)"
    if [ -z "$deps" ]; then
        continue
    fi

    echo "$deps" | while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        dep_base="$("$BASENAME_BIN" "$dep")"
        dep_dst="$FRAMEWORKS_DIR/$dep_base"
        dep_src="$dep"

        case "$dep" in
            @rpath/*)
                if [ -f "$FRAMEWORKS_DIR/$dep_base" ]; then
                    dep_src="$FRAMEWORKS_DIR/$dep_base"
                elif dep_src="$(resolve_search_root_dep "$dep_base")"; then
                    :
                else
                    echo "warning: unable to resolve $dep for $current_file" >&2
                    continue
                fi
                ;;
        esac

        if [ ! -f "$dep_dst" ]; then
            "$CP_BIN" -fL "$dep_src" "$dep_dst"
            "$CHMOD_BIN" u+w "$dep_dst"
            "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/$dep_base" "$dep_dst" || true
            echo "$dep_dst" >>"$QUEUE_FILE"
        fi

        replacement="$(replacement_path_for "$current_file" "$dep_base")"
        "$INSTALL_NAME_TOOL_BIN" -change "$dep" "$replacement" "$current_file"
    done
done <"$QUEUE_FILE"

# Vulkan loader uses runtime driver discovery; explicitly bundle MoltenVK so
# packaged apps do not depend on host Homebrew paths.
MOLTENVK_SRC="$(resolve_search_root_dep libMoltenVK.dylib || true)"
if [ -n "$MOLTENVK_SRC" ]; then
    MOLTENVK_DST="$FRAMEWORKS_DIR/libMoltenVK.dylib"
    if [ ! -f "$MOLTENVK_DST" ]; then
        "$CP_BIN" -fL "$MOLTENVK_SRC" "$MOLTENVK_DST"
        "$CHMOD_BIN" u+w "$MOLTENVK_DST"
    fi
    "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/libMoltenVK.dylib" "$MOLTENVK_DST" || true
fi

VULKAN_SRC="$(resolve_search_root_dep libvulkan.1.dylib || true)"
if [ -n "$VULKAN_SRC" ]; then
    VULKAN_DST="$FRAMEWORKS_DIR/libvulkan.1.dylib"
    if [ ! -f "$VULKAN_DST" ]; then
        "$CP_BIN" -fL "$VULKAN_SRC" "$VULKAN_DST"
        "$CHMOD_BIN" u+w "$VULKAN_DST"
    fi
    "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/libvulkan.1.dylib" "$VULKAN_DST" || true
fi

exit 0
