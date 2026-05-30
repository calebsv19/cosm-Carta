#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
RENDER_HELPER="$SCRIPT_DIR/render_saved_pin_route.sh"
VIDEO_HELPER="$SCRIPT_DIR/build_route_video.sh"

REGION=""
FROM_PIN=""
TO_PIN=""
OUT_DIR=""
ZOOM_LEVEL="balanced"
MOTION_PROFILE="balanced"
ORIENTATION_MODE="heading_up"
RENDER_MODE="map_route_marker"
INCLUDE_VIDEO="false"
INCLUDE_FRAMES=""
JOB_COPY_PATH=""
RESULT_COPY_PATH=""

usage() {
    cat <<'EOF'
Usage:
  run_saved_pin_route_skill.sh --region <region> --from <pin> --to <pin> --out <dir> [options]

Required:
  --region <region>       Region id
  --from <pin>            Saved start pin id or exact name
  --to <pin>              Saved goal pin id or exact name
  --out <dir>             Output run directory

Skill-facing options:
  --zoom-level <level>    close | balanced | wide
  --motion-profile <name> responsive | balanced | cinematic
  --orientation-mode <name>
                         heading_up | north_up
  --render-mode <mode>    map_route_marker | map_route | map_only
  --include-video <bool>  true | false
  --include-frames <bool> true | false
  --write-job-copy <path> Optional generated job copy path
  --write-result-copy <path>
                         Optional result JSON copy path outside the run dir
  --help                  Show usage

Environment:
  MAPFORGE_RUNTIME_DIR    Runtime/private pin root
  MAPFORGE_REGIONS_DIR    Regions root
  MAPFORGE_BINARY         Override the mapforge binary path
EOF
}

require_bool() {
    case "$1" in
        true|false) return 0 ;;
        *)
            echo "run_saved_pin_route_skill.sh: expected true|false but got '$1'" >&2
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
        --from)
            FROM_PIN=${2:-}
            shift 2
            ;;
        --to)
            TO_PIN=${2:-}
            shift 2
            ;;
        --out)
            OUT_DIR=${2:-}
            shift 2
            ;;
        --zoom-level)
            ZOOM_LEVEL=${2:-}
            shift 2
            ;;
        --motion-profile)
            MOTION_PROFILE=${2:-}
            shift 2
            ;;
        --orientation-mode)
            ORIENTATION_MODE=${2:-}
            shift 2
            ;;
        --render-mode)
            RENDER_MODE=${2:-}
            shift 2
            ;;
        --include-video)
            INCLUDE_VIDEO=${2:-}
            shift 2
            ;;
        --include-frames)
            INCLUDE_FRAMES=${2:-}
            shift 2
            ;;
        --write-job-copy)
            JOB_COPY_PATH=${2:-}
            shift 2
            ;;
        --write-result-copy)
            RESULT_COPY_PATH=${2:-}
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "run_saved_pin_route_skill.sh: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [ -z "$REGION" ] || [ -z "$FROM_PIN" ] || [ -z "$TO_PIN" ] || [ -z "$OUT_DIR" ]; then
    echo "run_saved_pin_route_skill.sh: --region, --from, --to, and --out are required" >&2
    usage >&2
    exit 1
fi

require_bool "$INCLUDE_VIDEO"

PRESET="balanced"
case "$ZOOM_LEVEL" in
    close)
        PRESET="zoomed_in"
        ;;
    balanced)
        PRESET="balanced"
        ;;
    wide)
        PRESET="zoomed_out"
        ;;
    *)
        echo "run_saved_pin_route_skill.sh: unsupported --zoom-level '$ZOOM_LEVEL'" >&2
        exit 1
        ;;
esac

FRAMES_FLAG="false"
if [ "$INCLUDE_VIDEO" = "true" ]; then
    FRAMES_FLAG="true"
elif [ -n "$INCLUDE_FRAMES" ]; then
    require_bool "$INCLUDE_FRAMES"
    FRAMES_FLAG="$INCLUDE_FRAMES"
fi

if [ -n "$JOB_COPY_PATH" ]; then
    /bin/sh "$RENDER_HELPER" \
        --region "$REGION" \
        --from-pin "$FROM_PIN" \
        --to-pin "$TO_PIN" \
        --out "$OUT_DIR" \
        --preset "$PRESET" \
        --motion-profile "$MOTION_PROFILE" \
        --orientation-mode "$ORIENTATION_MODE" \
        --frames "$FRAMES_FLAG" \
        --render-mode "$RENDER_MODE" \
        --write-job-copy "$JOB_COPY_PATH"
else
    /bin/sh "$RENDER_HELPER" \
        --region "$REGION" \
        --from-pin "$FROM_PIN" \
        --to-pin "$TO_PIN" \
        --out "$OUT_DIR" \
        --preset "$PRESET" \
        --motion-profile "$MOTION_PROFILE" \
        --orientation-mode "$ORIENTATION_MODE" \
        --frames "$FRAMES_FLAG" \
        --render-mode "$RENDER_MODE"
fi

VIDEO_PATH=""
if [ "$INCLUDE_VIDEO" = "true" ]; then
    /bin/sh "$VIDEO_HELPER" "$OUT_DIR" >/dev/null
    VIDEO_PATH="$OUT_DIR/video/route_preview.mp4"
fi

RESULT_JSON="$OUT_DIR/skill_result.json"
python3 - <<'PY' "$OUT_DIR" "$REGION" "$FROM_PIN" "$TO_PIN" "$ZOOM_LEVEL" "$MOTION_PROFILE" "$ORIENTATION_MODE" "$RENDER_MODE" "$FRAMES_FLAG" "$INCLUDE_VIDEO" "$VIDEO_PATH" "$RESULT_JSON"
import json
import os
import sys

out_dir, region, from_pin, to_pin, zoom_level, motion_profile, orientation_mode, render_mode, include_frames, include_video, video_path, result_json = sys.argv[1:]

manifest_path = os.path.join(out_dir, "manifest.json")
summary_path = os.path.join(out_dir, "summary.md")
resolved_job_path = os.path.join(out_dir, "job.resolved.json")
preview_path = os.path.join(out_dir, "preview.bmp")
frames_dir = os.path.join(out_dir, "frames")

with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)

result = {
    "schema": "mapforge-saved-pin-skill-result/v1",
    "status": manifest.get("status", "unknown"),
    "region": region,
    "from_pin": from_pin,
    "to_pin": to_pin,
    "zoom_level": zoom_level,
    "motion_profile": motion_profile,
    "orientation_mode": orientation_mode,
    "render_mode": render_mode,
    "include_frames": include_frames == "true",
    "include_video": include_video == "true",
    "run_dir": out_dir,
    "manifest": manifest_path,
    "summary": summary_path,
    "resolved_job": resolved_job_path,
    "preview": preview_path if os.path.exists(preview_path) else "",
    "frames_dir": frames_dir if os.path.isdir(frames_dir) else "",
    "video": video_path if video_path and os.path.exists(video_path) else "",
}

with open(result_json, "w", encoding="utf-8") as f:
    json.dump(result, f, indent=2)
PY

if [ -n "$RESULT_COPY_PATH" ]; then
    mkdir -p "$(dirname "$RESULT_COPY_PATH")"
    cp "$RESULT_JSON" "$RESULT_COPY_PATH"
fi

echo "status=complete"
echo "schema=mapforge-saved-pin-skill-result/v1"
echo "run_dir=$OUT_DIR"
echo "zoom_level=$ZOOM_LEVEL"
echo "motion_profile=$MOTION_PROFILE"
echo "orientation_mode=$ORIENTATION_MODE"
echo "manifest=$OUT_DIR/manifest.json"
echo "summary=$OUT_DIR/summary.md"
echo "resolved_job=$OUT_DIR/job.resolved.json"
echo "preview=$OUT_DIR/preview.bmp"
echo "frames_dir=$OUT_DIR/frames"
if [ -n "$VIDEO_PATH" ]; then
    echo "video=$VIDEO_PATH"
else
    echo "video="
fi
echo "result_json=$RESULT_JSON"
