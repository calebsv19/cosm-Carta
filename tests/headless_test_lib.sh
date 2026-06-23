# Shared setup/assertion helpers for MapForge headless shell smoke tests.

MAPFORGE_TEST_SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
MAPFORGE_TEST_REPO_DIR=$(CDPATH= cd -- "$MAPFORGE_TEST_SCRIPT_DIR/.." && pwd)
MAPFORGE_TEST_BINARY=${MAPFORGE_BINARY:-"$MAPFORGE_TEST_REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}

mapforge_test_setup_tmp() {
    prefix=$1
    TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/${prefix}.XXXXXX")
}

mapforge_test_cleanup_tmp() {
    if [ -n "${TMP_DIR:-}" ]; then
        rm -rf "$TMP_DIR"
    fi
}

mapforge_test_install_demo_pins() {
    runtime_dir=$1
    region=${2:-seattle}
    mkdir -p "$runtime_dir/pins"
    cp "$MAPFORGE_TEST_REPO_DIR/data/pins/examples/demo.${region}.pins.json" \
        "$runtime_dir/pins/${region}.pins.local.json"
}

mapforge_test_assert_file() {
    test -f "$1"
}

mapforge_test_assert_dir() {
    test -d "$1"
}

mapforge_test_assert_grep() {
    pattern=$1
    path=$2
    grep -q "$pattern" "$path"
}

mapforge_test_assert_egrep() {
    pattern=$1
    path=$2
    grep -Eq "$pattern" "$path"
}

mapforge_test_assert_fixed_grep() {
    pattern=$1
    path=$2
    grep -F -q "$pattern" "$path"
}
