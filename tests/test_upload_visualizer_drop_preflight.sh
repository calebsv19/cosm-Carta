#!/bin/sh
set -eu

. "$(CDPATH= cd -- "$(dirname "$0")" && pwd)/headless_test_lib.sh"

HELPER="$MAPFORGE_TEST_REPO_DIR/scripts/upload_visualizer_drop.sh"
mapforge_test_setup_tmp "mapforge_upload_visualizer_preflight"
DROP_DIR="$TMP_DIR/drop"
FAKE_BIN="$TMP_DIR/bin"
STDOUT_CAPTURE="$TMP_DIR/stdout.txt"
STDERR_CAPTURE="$TMP_DIR/stderr.txt"
DROP_ID="map-forge--saved-pin-route--20260520T090000Z--safeupload"

trap mapforge_test_cleanup_tmp EXIT INT TERM

mkdir -p "$DROP_DIR" "$FAKE_BIN"
printf '{"drop_id":"%s"}\n' "$DROP_ID" > "$DROP_DIR/manifest.json"
printf 'abc  manifest.json\n' > "$DROP_DIR/SHA256SUMS"
printf 'ready\n' > "$DROP_DIR/READY"

cat > "$FAKE_BIN/ssh" <<'EOF'
#!/bin/sh
echo "ssh should not run during upload preflight tests" >&2
exit 99
EOF
cat > "$FAKE_BIN/rsync" <<'EOF'
#!/bin/sh
echo "rsync should not run during upload preflight tests" >&2
exit 99
EOF
chmod +x "$FAKE_BIN/ssh" "$FAKE_BIN/rsync"

if PATH="$FAKE_BIN:$PATH" /bin/sh "$HELPER" "$DROP_DIR" "bad;drop" > "$STDOUT_CAPTURE" 2> "$STDERR_CAPTURE"; then
    echo "expected invalid drop id to fail preflight" >&2
    exit 1
fi
mapforge_test_assert_grep 'preflight failed: invalid_drop_id' "$STDERR_CAPTURE"
if grep -q 'ssh should not run\|rsync should not run' "$STDERR_CAPTURE"; then
    echo "remote tools ran for invalid drop id" >&2
    exit 1
fi

if PATH="$FAKE_BIN:$PATH" REMOTE_ROOT="/tmp/root;rm" /bin/sh "$HELPER" "$DROP_DIR" "$DROP_ID" > "$STDOUT_CAPTURE" 2> "$STDERR_CAPTURE"; then
    echo "expected invalid remote root to fail preflight" >&2
    exit 1
fi
mapforge_test_assert_grep 'preflight failed: invalid_remote_root' "$STDERR_CAPTURE"
if grep -q 'ssh should not run\|rsync should not run' "$STDERR_CAPTURE"; then
    echo "remote tools ran for invalid remote root" >&2
    exit 1
fi

if PATH="$FAKE_BIN:$PATH" REMOTE_PREPARE="/tmp/prepare bad.sh" /bin/sh "$HELPER" "$DROP_DIR" "$DROP_ID" > "$STDOUT_CAPTURE" 2> "$STDERR_CAPTURE"; then
    echo "expected invalid remote prepare path to fail preflight" >&2
    exit 1
fi
mapforge_test_assert_grep 'preflight failed: invalid_remote_prepare' "$STDERR_CAPTURE"
if grep -q 'ssh should not run\|rsync should not run' "$STDERR_CAPTURE"; then
    echo "remote tools ran for invalid remote prepare path" >&2
    exit 1
fi

rm -f "$DROP_DIR/READY"
if PATH="$FAKE_BIN:$PATH" /bin/sh "$HELPER" "$DROP_DIR" "$DROP_ID" > "$STDOUT_CAPTURE" 2> "$STDERR_CAPTURE"; then
    echo "expected missing READY to fail preflight" >&2
    exit 1
fi
mapforge_test_assert_grep 'preflight failed: missing_required_artifact' "$STDERR_CAPTURE"
if grep -q 'ssh should not run\|rsync should not run' "$STDERR_CAPTURE"; then
    echo "remote tools ran for missing artifact" >&2
    exit 1
fi
printf 'ready\n' > "$DROP_DIR/READY"

PATH="$FAKE_BIN:$PATH" MAPFORGE_UPLOAD_DRY_RUN=true /bin/sh "$HELPER" "$DROP_DIR" "$DROP_ID" > "$STDOUT_CAPTURE" 2> "$STDERR_CAPTURE"
mapforge_test_assert_grep '^visualizer drop upload dry-run complete$' "$STDOUT_CAPTURE"
mapforge_test_assert_grep '^drop_id='"$DROP_ID"'$' "$STDOUT_CAPTURE"
mapforge_test_assert_grep '^remote_drop_dir=/srv/release-staging/codework-visualizer/'"$DROP_ID"'/''$' "$STDOUT_CAPTURE"

echo "upload visualizer drop preflight smoke passed"
