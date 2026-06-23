#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
. "$SCRIPT_DIR/saved_pin_common.sh"
SCRIPT_NAME="render_saved_pin_route.sh"
BINARY=${MAPFORGE_BINARY:-"$REPO_DIR/build/targets/macOS-arm64/toolchains/clang/bin/mapforge"}

REGION=""
FROM_PIN=""
TO_PIN=""
OUT_DIR=""
PRESET="balanced"
FRAMES="false"
PREVIEW="true"
RENDER_MODE="map_route_marker"
ORIENTATION_MODE="heading_up"
DURATION_SECONDS="12"
FPS="30"
WIDTH="1280"
HEIGHT="720"
ZOOM="15.0"
FOLLOW_ROUTE="true"
ROTATE_WITH_HEADING="true"
MOTION_PROFILE="balanced"
HEADING_SMOOTHING_TAU_SECONDS="0.35"
HEADING_LOOKAHEAD_SECONDS="2.0"
HEADING_MEASUREMENT_WINDOW_SECONDS="3.0"
HEADING_MAX_TURN_RATE_DEG_PER_SEC="90.0"
QUALITY_PROFILE="final"
PIXEL_SCALE="2"
KEEP_JOB="false"
JOB_COPY_PATH=""

usage() {
    cat <<'EOF'
Usage:
  render_saved_pin_route.sh --region <region> --from-pin <id-or-name> --to-pin <id-or-name> --out <dir> [options]

Required:
  --region <region>         Region id, for example: seattle
  --from-pin <id-or-name>   Saved pin id or exact name for route start
  --to-pin <id-or-name>     Saved pin id or exact name for route goal
  --out <dir>               Output run directory

Options:
  --preset <name>           balanced | zoomed_in | zoomed_out | frames
  --motion-profile <name>   responsive | balanced | cinematic
  --orientation-mode <name>
                           heading_up | north_up
  --frames <true|false>     Override frame export
  --preview <true|false>    Override preview export
  --render-mode <mode>      map_route_marker | map_route | map_only
  --duration <seconds>      Playback duration
  --fps <count>             Playback frames per second
  --width <px>              Camera width
  --height <px>             Camera height
  --zoom <value>            Camera zoom
  --follow <true|false>     Follow-route camera
  --rotate <true|false>     Rotate camera with heading
  --quality-profile <name>  runtime | final
  --pixel-scale <count>     Internal supersample factor (>=1)
  --write-job-copy <path>   Save the generated request JSON to a durable path
  --keep-job                Keep the temporary generated job JSON
  --help                    Show this usage text

Environment:
  MAPFORGE_BINARY           Override the mapforge binary path
  MAPFORGE_REGIONS_DIR      Override regions root (defaults to repo data/regions)
  MAPFORGE_RUNTIME_DIR      Runtime/private pin root used by headless default pin resolution
EOF
}

apply_preset() {
    case "$1" in
        balanced)
            ZOOM="15.0"
            DURATION_SECONDS="12"
            FPS="30"
            FRAMES="false"
            PREVIEW="true"
            ;;
        zoomed_in)
            ZOOM="16.5"
            DURATION_SECONDS="12"
            FPS="30"
            FRAMES="false"
            PREVIEW="true"
            ;;
        zoomed_out)
            ZOOM="14.0"
            DURATION_SECONDS="12"
            FPS="30"
            FRAMES="false"
            PREVIEW="true"
            ;;
        frames)
            ZOOM="15.0"
            DURATION_SECONDS="12"
            FPS="30"
            FRAMES="true"
            PREVIEW="true"
            ;;
        *)
            echo "$SCRIPT_NAME: unsupported preset '$1'" >&2
            exit 1
            ;;
    esac
}

apply_motion_profile() {
    case "$1" in
        responsive)
            HEADING_SMOOTHING_TAU_SECONDS="0.22"
            HEADING_LOOKAHEAD_SECONDS="1.5"
            HEADING_MEASUREMENT_WINDOW_SECONDS="2.0"
            HEADING_MAX_TURN_RATE_DEG_PER_SEC="140.0"
            ;;
        balanced)
            HEADING_SMOOTHING_TAU_SECONDS="0.35"
            HEADING_LOOKAHEAD_SECONDS="2.0"
            HEADING_MEASUREMENT_WINDOW_SECONDS="3.0"
            HEADING_MAX_TURN_RATE_DEG_PER_SEC="90.0"
            ;;
        cinematic)
            HEADING_SMOOTHING_TAU_SECONDS="0.65"
            HEADING_LOOKAHEAD_SECONDS="3.5"
            HEADING_MEASUREMENT_WINDOW_SECONDS="4.5"
            HEADING_MAX_TURN_RATE_DEG_PER_SEC="45.0"
            ;;
        *)
            echo "$SCRIPT_NAME: unsupported motion profile '$1'" >&2
            exit 1
            ;;
    esac
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --region)
            REGION=${2:-}
            shift 2
            ;;
        --from-pin)
            FROM_PIN=${2:-}
            shift 2
            ;;
        --to-pin)
            TO_PIN=${2:-}
            shift 2
            ;;
        --out)
            OUT_DIR=${2:-}
            shift 2
            ;;
        --preset)
            PRESET=${2:-}
            apply_preset "$PRESET"
            shift 2
            ;;
        --motion-profile)
            MOTION_PROFILE=${2:-}
            apply_motion_profile "$MOTION_PROFILE"
            shift 2
            ;;
        --orientation-mode)
            ORIENTATION_MODE=${2:-}
            shift 2
            ;;
        --frames)
            FRAMES=${2:-}
            shift 2
            ;;
        --preview)
            PREVIEW=${2:-}
            shift 2
            ;;
        --render-mode)
            RENDER_MODE=${2:-}
            shift 2
            ;;
        --duration)
            DURATION_SECONDS=${2:-}
            shift 2
            ;;
        --fps)
            FPS=${2:-}
            shift 2
            ;;
        --width)
            WIDTH=${2:-}
            shift 2
            ;;
        --height)
            HEIGHT=${2:-}
            shift 2
            ;;
        --zoom)
            ZOOM=${2:-}
            shift 2
            ;;
        --follow)
            FOLLOW_ROUTE=${2:-}
            shift 2
            ;;
        --rotate)
            ROTATE_WITH_HEADING=${2:-}
            shift 2
            ;;
        --quality-profile)
            QUALITY_PROFILE=${2:-}
            shift 2
            ;;
        --pixel-scale)
            PIXEL_SCALE=${2:-}
            shift 2
            ;;
        --write-job-copy)
            JOB_COPY_PATH=${2:-}
            shift 2
            ;;
        --keep-job)
            KEEP_JOB="true"
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "$SCRIPT_NAME: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [ -z "$REGION" ] || [ -z "$FROM_PIN" ] || [ -z "$TO_PIN" ] || [ -z "$OUT_DIR" ]; then
    echo "$SCRIPT_NAME: --region, --from-pin, --to-pin, and --out are required" >&2
    usage >&2
    exit 1
fi
mapforge_require_local_artifact_path "$SCRIPT_NAME" "--out" "$OUT_DIR"
if [ -n "$JOB_COPY_PATH" ]; then
    mapforge_require_local_artifact_path "$SCRIPT_NAME" "--write-job-copy" "$JOB_COPY_PATH"
fi

mapforge_require_bool "$SCRIPT_NAME" "$FRAMES"
mapforge_require_bool "$SCRIPT_NAME" "$PREVIEW"
mapforge_require_bool "$SCRIPT_NAME" "$FOLLOW_ROUTE"
apply_motion_profile "$MOTION_PROFILE"
mapforge_validate_orientation_mode "$SCRIPT_NAME" "$ORIENTATION_MODE" false

case "$ORIENTATION_MODE" in
    heading_up)
        ROTATE_WITH_HEADING="true"
        ;;
    north_up)
        ROTATE_WITH_HEADING="false"
        ;;
esac

mapforge_validate_render_mode "$SCRIPT_NAME" "$RENDER_MODE"
mapforge_require_operator_executable "$SCRIPT_NAME" "MAPFORGE_BINARY" "$BINARY"

case "$QUALITY_PROFILE" in
    runtime|final) ;;
    *)
        echo "$SCRIPT_NAME: invalid quality profile '$QUALITY_PROFILE'" >&2
        exit 1
        ;;
esac

case "$PIXEL_SCALE" in
    ''|*[!0-9]*)
        echo "$SCRIPT_NAME: pixel scale must be an integer >= 1" >&2
        exit 1
        ;;
esac
if [ "$PIXEL_SCALE" -lt 1 ]; then
    echo "$SCRIPT_NAME: pixel scale must be >= 1" >&2
    exit 1
fi

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_saved_pin_job.XXXXXX")
JOB_PATH="$TMP_DIR/generated_saved_pin_job.json"

cleanup() {
    if [ "$KEEP_JOB" != "true" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT INT TERM

python3 - "$JOB_PATH" \
    "$REGION" \
    "$FROM_PIN" \
    "$TO_PIN" \
    "$WIDTH" \
    "$HEIGHT" \
    "$ZOOM" \
    "$FOLLOW_ROUTE" \
    "$ROTATE_WITH_HEADING" \
    "$DURATION_SECONDS" \
    "$FPS" \
    "$HEADING_SMOOTHING_TAU_SECONDS" \
    "$HEADING_LOOKAHEAD_SECONDS" \
    "$HEADING_MEASUREMENT_WINDOW_SECONDS" \
    "$HEADING_MAX_TURN_RATE_DEG_PER_SEC" \
    "$PREVIEW" \
    "$FRAMES" \
    "$RENDER_MODE" \
    "$QUALITY_PROFILE" \
    "$PIXEL_SCALE" <<'PY'
import json
import sys
from pathlib import Path

(
    job_path,
    region,
    from_pin,
    to_pin,
    width,
    height,
    zoom,
    follow_route,
    rotate_with_heading,
    duration_seconds,
    fps,
    heading_smoothing_tau_seconds,
    heading_lookahead_seconds,
    heading_measurement_window_seconds,
    heading_max_turn_rate_deg_per_sec,
    preview,
    frames,
    render_mode,
    quality_profile,
    pixel_scale,
) = sys.argv[1:]

def as_bool(value):
    if value == "true":
        return True
    if value == "false":
        return False
    raise ValueError(f"expected boolean true|false, got {value!r}")

job = {
    "version": 2,
    "type": "route_playback_render",
    "map_region": region,
    "from_pin": from_pin,
    "to_pin": to_pin,
    "route": {
        "mode": "walking",
    },
    "camera": {
        "width": int(width),
        "height": int(height),
        "zoom": float(zoom),
        "follow_route": as_bool(follow_route),
        "rotate_with_heading": as_bool(rotate_with_heading),
    },
    "playback": {
        "duration_seconds": float(duration_seconds),
        "fps": int(fps),
        "start_paused": False,
        "heading": {
            "mode": "blended",
            "smoothing_tau_seconds": float(heading_smoothing_tau_seconds),
            "lookahead_seconds": float(heading_lookahead_seconds),
            "measurement_window_seconds": float(heading_measurement_window_seconds),
            "max_turn_rate_deg_per_sec": float(heading_max_turn_rate_deg_per_sec),
        },
    },
    "output": {
        "preview": as_bool(preview),
        "frames": as_bool(frames),
        "frame_format": "bmp",
        "video_manifest": False,
        "render_mode": render_mode,
        "quality_profile": quality_profile,
        "pixel_scale": int(pixel_scale),
        "stabilize_visible_zoom": True,
        "stabilize_tile_bands": True,
        "allow_tile_fallback": False,
        "simplify_route_screen_space": False,
    },
}

Path(job_path).write_text(json.dumps(job, indent=2) + "\n", encoding="utf-8")
PY

mapforge_copy_file_if_requested "$JOB_PATH" "$JOB_COPY_PATH"

MAPFORGE_REGIONS_DIR=${MAPFORGE_REGIONS_DIR:-$REPO_DIR/data/regions}
export MAPFORGE_REGIONS_DIR
if ! "$BINARY" --headless --job "$JOB_PATH" --out "$OUT_DIR"; then
    mapforge_print_saved_pin_failure \
        "$SCRIPT_NAME" \
        "binary_failed" \
        "$OUT_DIR" \
        "$JOB_PATH" \
        "$OUT_DIR/manifest.json" \
        "$OUT_DIR/summary.md"
    exit 1
fi

if ! mapforge_require_complete_manifest \
    "$SCRIPT_NAME" \
    "$OUT_DIR" \
    "$JOB_PATH" \
    "$OUT_DIR/manifest.json" \
    "$OUT_DIR/summary.md"; then
    exit 1
fi

printf 'saved-pin route export complete\n'
printf 'out=%s\n' "$OUT_DIR"
if [ -n "$JOB_COPY_PATH" ]; then
    printf 'job_copy=%s\n' "$JOB_COPY_PATH"
elif [ "$KEEP_JOB" = "true" ]; then
    printf 'job=%s\n' "$JOB_PATH"
fi
