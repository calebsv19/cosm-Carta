#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
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

require_bool() {
    case "$1" in
        true|false) return 0 ;;
        *)
            echo "render_saved_pin_route.sh: expected true|false but got '$1'" >&2
            exit 1
            ;;
    esac
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
            echo "render_saved_pin_route.sh: unsupported preset '$1'" >&2
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
            echo "render_saved_pin_route.sh: unsupported motion profile '$1'" >&2
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
            echo "render_saved_pin_route.sh: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [ -z "$REGION" ] || [ -z "$FROM_PIN" ] || [ -z "$TO_PIN" ] || [ -z "$OUT_DIR" ]; then
    echo "render_saved_pin_route.sh: --region, --from-pin, --to-pin, and --out are required" >&2
    usage >&2
    exit 1
fi

require_bool "$FRAMES"
require_bool "$PREVIEW"
require_bool "$FOLLOW_ROUTE"
apply_motion_profile "$MOTION_PROFILE"

case "$ORIENTATION_MODE" in
    heading_up)
        ROTATE_WITH_HEADING="true"
        ;;
    north_up)
        ROTATE_WITH_HEADING="false"
        ;;
    *)
        echo "render_saved_pin_route.sh: unsupported orientation mode '$ORIENTATION_MODE'" >&2
        exit 1
        ;;
esac

case "$RENDER_MODE" in
    map_route_marker|map_route|map_only) ;;
    *)
        echo "render_saved_pin_route.sh: invalid render mode '$RENDER_MODE'" >&2
        exit 1
        ;;
esac

case "$QUALITY_PROFILE" in
    runtime|final) ;;
    *)
        echo "render_saved_pin_route.sh: invalid quality profile '$QUALITY_PROFILE'" >&2
        exit 1
        ;;
esac

case "$PIXEL_SCALE" in
    ''|*[!0-9]*)
        echo "render_saved_pin_route.sh: pixel scale must be an integer >= 1" >&2
        exit 1
        ;;
esac
if [ "$PIXEL_SCALE" -lt 1 ]; then
    echo "render_saved_pin_route.sh: pixel scale must be >= 1" >&2
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

cat > "$JOB_PATH" <<EOF
{
  "version": 2,
  "type": "route_playback_render",
  "map_region": "$REGION",
  "from_pin": "$FROM_PIN",
  "to_pin": "$TO_PIN",
  "route": {
    "mode": "walking"
  },
  "camera": {
    "width": $WIDTH,
    "height": $HEIGHT,
    "zoom": $ZOOM,
    "follow_route": $FOLLOW_ROUTE,
    "rotate_with_heading": $ROTATE_WITH_HEADING
  },
  "playback": {
    "duration_seconds": $DURATION_SECONDS,
    "fps": $FPS,
    "start_paused": false,
    "heading": {
      "mode": "blended",
      "smoothing_tau_seconds": $HEADING_SMOOTHING_TAU_SECONDS,
      "lookahead_seconds": $HEADING_LOOKAHEAD_SECONDS,
      "measurement_window_seconds": $HEADING_MEASUREMENT_WINDOW_SECONDS,
      "max_turn_rate_deg_per_sec": $HEADING_MAX_TURN_RATE_DEG_PER_SEC
    }
  },
  "output": {
    "preview": $PREVIEW,
    "frames": $FRAMES,
    "frame_format": "bmp",
    "video_manifest": false,
    "render_mode": "$RENDER_MODE",
    "quality_profile": "$QUALITY_PROFILE",
    "pixel_scale": $PIXEL_SCALE,
    "stabilize_visible_zoom": true,
    "stabilize_tile_bands": true,
    "allow_tile_fallback": false,
    "simplify_route_screen_space": false
  }
}
EOF

if [ -n "$JOB_COPY_PATH" ]; then
    mkdir -p "$(dirname "$JOB_COPY_PATH")"
    cp "$JOB_PATH" "$JOB_COPY_PATH"
fi

MAPFORGE_REGIONS_DIR="${MAPFORGE_REGIONS_DIR:-$REPO_DIR/data/regions}" \
"$BINARY" --headless --job "$JOB_PATH" --out "$OUT_DIR"

printf 'saved-pin route export complete\n'
printf 'out=%s\n' "$OUT_DIR"
if [ -n "$JOB_COPY_PATH" ]; then
    printf 'job_copy=%s\n' "$JOB_COPY_PATH"
elif [ "$KEEP_JOB" = "true" ]; then
    printf 'job=%s\n' "$JOB_PATH"
fi
