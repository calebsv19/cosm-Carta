#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$SCRIPT_DIR/saved_pin_common.sh"
SCRIPT_NAME="build_route_video.sh"

RUN_DIR=${1:-}
OUTPUT_NAME=${2:-route_preview.mp4}
FPS_OVERRIDE=${3:-}

if [ -z "$RUN_DIR" ]; then
    echo "Usage: build_route_video.sh <run_dir> [output_name.mp4] [fps]" >&2
    exit 1
fi
mapforge_require_local_artifact_path "$SCRIPT_NAME" "run directory" "$RUN_DIR"

if [ ! -d "$RUN_DIR" ]; then
    echo "build_route_video.sh: run directory not found: $RUN_DIR" >&2
    exit 1
fi

FRAMES_DIR="$RUN_DIR/frames"
RESOLVED_JOB="$RUN_DIR/job.resolved.json"
VIDEO_DIR="$RUN_DIR/video"
OUTPUT_PATH="$VIDEO_DIR/$OUTPUT_NAME"
INPUT_LIST="$VIDEO_DIR/ffmpeg_input.txt"

if [ ! -d "$FRAMES_DIR" ]; then
    echo "build_route_video.sh: frames directory not found: $FRAMES_DIR" >&2
    exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "build_route_video.sh: ffmpeg is not available on PATH" >&2
    exit 1
fi

FPS="$FPS_OVERRIDE"
if [ -z "$FPS" ] && [ -f "$RESOLVED_JOB" ]; then
    FPS=$(python3 -c 'import json,sys
path=sys.argv[1]
with open(path, "r", encoding="utf-8") as f:
    data=json.load(f)
fps=data.get("playback", {}).get("resolved_fps") or data.get("playback", {}).get("fps")
print(int(fps) if fps else "")' "$RESOLVED_JOB")
fi
if [ -z "$FPS" ]; then
    FPS="30"
fi

mkdir -p "$VIDEO_DIR"

find "$FRAMES_DIR" -name 'frame_*.bmp' | sort | awk '{ printf("file '\''%s'\''\n", $0); }' > "$INPUT_LIST"

if [ ! -s "$INPUT_LIST" ]; then
    echo "build_route_video.sh: no frame_*.bmp files found in $FRAMES_DIR" >&2
    exit 1
fi

ffmpeg -y \
    -r "$FPS" \
    -f concat \
    -safe 0 \
    -i "$INPUT_LIST" \
    -vf "fps=$FPS" \
    -c:v libx264 \
    -preset slow \
    -crf 12 \
    -pix_fmt yuv420p \
    "$OUTPUT_PATH" \
    >/dev/null 2>&1

echo "route video build complete"
echo "run_dir=$RUN_DIR"
echo "video=$OUTPUT_PATH"
echo "fps=$FPS"
