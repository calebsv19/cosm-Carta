# Shared shell helpers for MapForge saved-pin wrapper scripts.

mapforge_require_bool() {
    script_name=$1
    value=$2
    case "$value" in
        true|false) return 0 ;;
        *)
            echo "$script_name: expected true|false but got '$value'" >&2
            exit 1
            ;;
    esac
}

mapforge_validate_render_mode() {
    script_name=$1
    value=$2
    case "$value" in
        map_route_marker|map_route|map_only) return 0 ;;
        *)
            echo "$script_name: invalid render mode '$value'" >&2
            exit 1
            ;;
    esac
}

mapforge_validate_orientation_mode() {
    script_name=$1
    value=$2
    allow_both=$3
    case "$value" in
        heading_up|north_up) return 0 ;;
        both)
            if [ "$allow_both" = "true" ]; then
                return 0
            fi
            ;;
    esac
    echo "$script_name: unsupported orientation mode '$value'" >&2
    exit 1
}

mapforge_orientation_list() {
    case "$1" in
        heading_up) printf 'heading_up\n' ;;
        north_up) printf 'north_up\n' ;;
        both) printf 'heading_up north_up\n' ;;
        *) return 1 ;;
    esac
}

mapforge_zoom_level_to_preset() {
    script_name=$1
    value=$2
    case "$value" in
        close) printf 'zoomed_in\n' ;;
        balanced) printf 'balanced\n' ;;
        wide) printf 'zoomed_out\n' ;;
        *)
            echo "$script_name: unsupported --zoom-level '$value'" >&2
            exit 1
            ;;
    esac
}

mapforge_sanitize_nonce() {
    printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9'
}

mapforge_drop_id_valid() {
    value=$1
    printf '%s\n' "$value" | grep -Eq '^[a-z0-9][a-z0-9-]*--[a-z0-9][a-z0-9-]*--[0-9]{8}T[0-9]{6}Z--[a-z0-9]+$'
}

mapforge_local_artifact_path_reject_reason() {
    value=$1
    case "$value" in
        "")
            printf 'empty'
            return 0
            ;;
        /)
            printf 'root'
            return 0
            ;;
        .|./)
            printf 'current_directory'
            return 0
            ;;
        ..|../*|*/..|*/../*)
            printf 'parent_traversal'
            return 0
            ;;
    esac
    return 1
}

mapforge_require_local_artifact_path() {
    script_name=$1
    label=$2
    value=$3
    reason=$(mapforge_local_artifact_path_reject_reason "$value" || true)
    if [ -n "$reason" ]; then
        echo "$script_name: invalid local artifact path for $label: $reason" >&2
        echo "$script_name: path=$value" >&2
        exit 1
    fi
}

mapforge_operator_executable_reject_reason() {
    value=$1
    case "$value" in
        "")
            printf 'empty'
            return 0
            ;;
        */*)
            if [ ! -f "$value" ]; then
                printf 'missing_file'
                return 0
            fi
            if [ ! -x "$value" ]; then
                printf 'not_executable'
                return 0
            fi
            ;;
        *)
            if ! command -v "$value" >/dev/null 2>&1; then
                printf 'not_on_path'
                return 0
            fi
            ;;
    esac
    return 1
}

mapforge_operator_script_reject_reason() {
    value=$1
    case "$value" in
        "")
            printf 'empty'
            return 0
            ;;
    esac
    if [ ! -f "$value" ]; then
        printf 'missing_file'
        return 0
    fi
    if [ ! -r "$value" ]; then
        printf 'not_readable'
        return 0
    fi
    return 1
}

mapforge_require_operator_executable() {
    script_name=$1
    label=$2
    value=$3
    reason=$(mapforge_operator_executable_reject_reason "$value" || true)
    if [ -n "$reason" ]; then
        echo "$script_name: invalid trusted operator executable for $label: $reason" >&2
        echo "$script_name: path=$value" >&2
        echo "$script_name: request data must not control $label; set it only from the local operator environment" >&2
        exit 1
    fi
}

mapforge_require_operator_script_file() {
    script_name=$1
    label=$2
    value=$3
    reason=$(mapforge_operator_script_reject_reason "$value" || true)
    if [ -n "$reason" ]; then
        echo "$script_name: invalid trusted operator script for $label: $reason" >&2
        echo "$script_name: path=$value" >&2
        echo "$script_name: request data must not control $label; set it only from the local operator environment" >&2
        exit 1
    fi
}

mapforge_copy_file_if_requested() {
    src=$1
    dest=$2
    if [ -n "$dest" ]; then
        mapforge_require_local_artifact_path "mapforge_copy_file_if_requested" "copy destination" "$dest"
        mkdir -p "$(dirname "$dest")"
        cp "$src" "$dest"
    fi
}

mapforge_manifest_field() {
    manifest_path=$1
    field_path=$2
    python3 - "$manifest_path" "$field_path" <<'PY'
import json
import sys

manifest_path, field_path = sys.argv[1:]
try:
    with open(manifest_path, "r", encoding="utf-8") as f:
        value = json.load(f)
    for part in field_path.split("."):
        if not isinstance(value, dict):
            value = ""
            break
        value = value.get(part, "")
    if value is None or isinstance(value, (dict, list)):
        value = ""
    print(value)
except Exception:
    print("")
PY
}

mapforge_print_saved_pin_failure() {
    script_name=$1
    reason=$2
    run_dir=$3
    job_path=$4
    manifest_path=$5
    summary_path=$6

    echo "$script_name: saved-pin wrapper failure: $reason" >&2
    if [ -n "$run_dir" ]; then
        echo "$script_name: run_dir=$run_dir" >&2
    fi
    if [ -n "$job_path" ]; then
        echo "$script_name: job=$job_path" >&2
    fi
    if [ -n "$manifest_path" ]; then
        echo "$script_name: manifest=$manifest_path" >&2
    fi
    if [ -n "$summary_path" ]; then
        echo "$script_name: summary=$summary_path" >&2
    fi

    if [ -n "$manifest_path" ] && [ -f "$manifest_path" ]; then
        manifest_status=$(mapforge_manifest_field "$manifest_path" "status")
        failure_stage=$(mapforge_manifest_field "$manifest_path" "failure.stage")
        failure_code=$(mapforge_manifest_field "$manifest_path" "failure.code")
        failure_context=$(mapforge_manifest_field "$manifest_path" "failure.context")
        failure_error=$(mapforge_manifest_field "$manifest_path" "error")
        if [ -n "$manifest_status" ]; then
            echo "$script_name: manifest_status=$manifest_status" >&2
        fi
        if [ -n "$failure_stage" ]; then
            echo "$script_name: failure_stage=$failure_stage" >&2
        fi
        if [ -n "$failure_code" ]; then
            echo "$script_name: failure_code=$failure_code" >&2
        fi
        if [ -n "$failure_context" ]; then
            echo "$script_name: failure_context=$failure_context" >&2
        fi
        if [ -n "$failure_error" ]; then
            echo "$script_name: error=$failure_error" >&2
        fi
    fi
}

mapforge_require_complete_manifest() {
    script_name=$1
    run_dir=$2
    job_path=$3
    manifest_path=$4
    summary_path=$5

    if [ ! -f "$manifest_path" ]; then
        mapforge_print_saved_pin_failure "$script_name" "missing_manifest" "$run_dir" "$job_path" "$manifest_path" "$summary_path"
        return 1
    fi

    manifest_status=$(mapforge_manifest_field "$manifest_path" "status")
    if [ "$manifest_status" != "complete" ]; then
        mapforge_print_saved_pin_failure "$script_name" "manifest_not_complete" "$run_dir" "$job_path" "$manifest_path" "$summary_path"
        return 1
    fi

    return 0
}

mapforge_print_visualizer_failure() {
    script_name=$1
    reason=$2
    run_dir=$3
    drop_dir=$4
    manifest_path=$5
    preview_path=$6
    primary_path=$7
    log_path=$8

    echo "$script_name: visualizer wrapper failure: $reason" >&2
    if [ -n "$run_dir" ]; then
        echo "$script_name: run_dir=$run_dir" >&2
    fi
    if [ -n "$drop_dir" ]; then
        echo "$script_name: drop_dir=$drop_dir" >&2
    fi
    if [ -n "$manifest_path" ]; then
        echo "$script_name: manifest=$manifest_path" >&2
    fi
    if [ -n "$preview_path" ]; then
        echo "$script_name: preview=$preview_path" >&2
    fi
    if [ -n "$primary_path" ]; then
        echo "$script_name: primary_output=$primary_path" >&2
    fi
    if [ -n "$log_path" ]; then
        echo "$script_name: log_source=$log_path" >&2
    fi
}
