#!/usr/bin/env bash
set -euo pipefail

workspace="$(mktemp -d /tmp/mapforge_region_publish_safety.XXXXXX)"
trap 'rm -rf "$workspace"' EXIT

target_contract_helper="${TARGET_CONTRACT_HELPER:-../bin/desktop_release_target_contract.sh}"
target_triple="$(TARGET_ARCH="${TARGET_ARCH:-}" TARGET_OS="${TARGET_OS:-}" TARGET_VARIANT="${TARGET_VARIANT:-desktop-app}" "$target_contract_helper" get target_triple)"
tool_root="build/targets/${target_triple}/tools"
region_tool="${tool_root}/mapforge_region"

if [[ ! -x "$region_tool" ]]; then
    make tools-build >/dev/null
fi

expect_unsafe_out() {
    local out_path="$1"
    local reason="$2"
    local label="$3"
    local stderr_path="$workspace/${label}.stderr"
    if "$region_tool" --region unsafe_publish --osm "$workspace/missing.osm" --out "$out_path" --replace 2>"$stderr_path"; then
        echo "expected unsafe publish path rejection for $label" >&2
        exit 1
    fi
    grep -q "Refusing unsafe region publish path" "$stderr_path"
    grep -q "(${reason})" "$stderr_path"
    grep -q 'diagnostic_stage=publish' "$stderr_path"
}

expect_unsafe_out "/" "root" "root"
expect_unsafe_out "." "current_directory" "current_directory"
expect_unsafe_out "$workspace/../unsafe_publish" "parent_traversal" "parent_traversal"
expect_unsafe_out "~" "home_root" "tilde_home"

if [[ -n "${HOME:-}" ]]; then
    expect_unsafe_out "$HOME" "home_root" "home_exact"
fi

safe_stderr="$workspace/safe.stderr"
safe_out="$workspace/regions/safe_publish"
if "$region_tool" --region safe_publish --osm "$workspace/missing.osm" --out "$safe_out" --replace 2>"$safe_stderr"; then
    echo "expected missing source failure for safe publish path" >&2
    exit 1
fi
if grep -q "Refusing unsafe region publish path" "$safe_stderr"; then
    echo "safe publish path was rejected as unsafe" >&2
    exit 1
fi
grep -q 'diagnostic_stage=source_ingest' "$safe_stderr"

echo "region publish safety checks passed"
