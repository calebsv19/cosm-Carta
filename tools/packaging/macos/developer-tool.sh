#!/bin/sh
# Run packaging tools directly; xcrun and /usr/bin shims can write global caches
# even with --no-cache. Match the default toolchain used by preparation builds.
set -eu

tool="${1:-}"
case "$tool" in otool|install_name_tool|lipo) ;; *) echo "unsupported packaging developer tool: $tool" >&2; exit 2 ;; esac
shift
developer="${DEVELOPER_DIR:-}"
if [ -z "$developer" ]; then developer="$(/usr/bin/xcode-select -p)"; fi
if [ -d "$developer/Contents/Developer" ]; then developer="$developer/Contents/Developer"; fi
for directory in "$developer/Toolchains/XcodeDefault.xctoolchain/usr/bin" "$developer/usr/bin"; do
    if [ -x "$directory/$tool" ]; then exec "$directory/$tool" "$@"; fi
done
echo "selected developer directory lacks packaging tool: $tool" >&2
exit 1
