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

"$MKDIR_BIN" -p "$FRAMEWORKS_DIR"

WORK_TMP_DIR="${TMPDIR:-/tmp}/mapforge_bundle_dylibs.$$"
"$MKDIR_BIN" -p "$WORK_TMP_DIR"
QUEUE_FILE="$WORK_TMP_DIR/queue.txt"
SEEN_FILE="$WORK_TMP_DIR/seen.txt"
touch "$QUEUE_FILE" "$SEEN_FILE"

cleanup() {
    rm -rf "$WORK_TMP_DIR" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

echo "$APP_BIN" >>"$QUEUE_FILE"

while IFS= read -r current_file; do
    [ -n "$current_file" ] || continue
    if "$GREP_BIN" -Fxq "$current_file" "$SEEN_FILE"; then
        continue
    fi
    echo "$current_file" >>"$SEEN_FILE"

    deps="$("$OTOOL_BIN" -L "$current_file" | "$AWK_BIN" 'NR>1 {print $1}' | "$GREP_BIN" -E '^/opt/homebrew|^/usr/local' || true)"
    if [ -z "$deps" ]; then
        continue
    fi

    echo "$deps" | while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        dep_base="$("$BASENAME_BIN" "$dep")"
        dep_dst="$FRAMEWORKS_DIR/$dep_base"

        if [ ! -f "$dep_dst" ]; then
            "$CP_BIN" -fL "$dep" "$dep_dst"
            "$CHMOD_BIN" u+w "$dep_dst"
            "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/$dep_base" "$dep_dst" || true
            echo "$dep_dst" >>"$QUEUE_FILE"
        fi

        if [ "$current_file" = "$APP_BIN" ]; then
            replacement="@executable_path/../Frameworks/$dep_base"
        else
            replacement="@loader_path/$dep_base"
        fi
        "$INSTALL_NAME_TOOL_BIN" -change "$dep" "$replacement" "$current_file"
    done
done <"$QUEUE_FILE"

exit 0
