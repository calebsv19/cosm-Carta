#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
STAGE_TOOL="${MAPFORGE_STAGE_TOOL:-$REPO_DIR/../skills/codework-visualizer-drop/scripts/stage_visualizer_run.py}"

RUN_DIR=""
DROP_ID=""
STAGING_ROOT="${REPO_DIR}/../_private_workspace_artifacts/codework_visualizer_runs"
WRITE_READY="false"
CONVERT_BMP_TO_PNG="true"
OVERWRITE="false"
SUMMARY=""

usage() {
    cat <<'EOF'
Usage:
  stage_saved_pin_visualizer_drop.sh --run-dir <dir> --drop-id <drop_id> [options]

Required:
  --run-dir <dir>          Completed MapForge headless run directory
  --drop-id <drop_id>      visualizer-run/v1 drop_id (must equal run_id)
                           format: <program>--<job-type>--<YYYYMMDDTHHMMSSZ>--<alnumnonce>

Options:
  --staging-root <dir>     Local staged-drop root
  --write-ready            Create READY in the local staged drop
  --overwrite              Replace an existing staged drop
  --keep-bmp               Keep BMP preview/output assets instead of converting to PNG
  --summary <text>         Optional summary override
  --help                   Show usage
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --run-dir)
            RUN_DIR=${2:-}
            shift 2
            ;;
        --drop-id)
            DROP_ID=${2:-}
            shift 2
            ;;
        --staging-root)
            STAGING_ROOT=${2:-}
            shift 2
            ;;
        --write-ready)
            WRITE_READY="true"
            shift
            ;;
        --overwrite)
            OVERWRITE="true"
            shift
            ;;
        --keep-bmp)
            CONVERT_BMP_TO_PNG="false"
            shift
            ;;
        --summary)
            SUMMARY=${2:-}
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "stage_saved_pin_visualizer_drop.sh: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [ -z "$RUN_DIR" ] || [ -z "$DROP_ID" ]; then
    echo "stage_saved_pin_visualizer_drop.sh: --run-dir and --drop-id are required" >&2
    usage >&2
    exit 1
fi

if ! printf '%s\n' "$DROP_ID" | grep -Eq '^[a-z0-9][a-z0-9-]*--[a-z0-9][a-z0-9-]*--[0-9]{8}T[0-9]{6}Z--[a-z0-9]+$'; then
    echo "stage_saved_pin_visualizer_drop.sh: drop_id must match <program>--<job-type>--<YYYYMMDDTHHMMSSZ>--<alnumnonce>" >&2
    exit 1
fi

if [ ! -d "$RUN_DIR" ]; then
    echo "stage_saved_pin_visualizer_drop.sh: run directory not found: $RUN_DIR" >&2
    exit 1
fi

if [ ! -f "$STAGE_TOOL" ]; then
    echo "stage_saved_pin_visualizer_drop.sh: stage tool not found: $STAGE_TOOL" >&2
    exit 1
fi

MANIFEST="$RUN_DIR/manifest.json"
SUMMARY_MD="$RUN_DIR/summary.md"
JOB_RESOLVED="$RUN_DIR/job.resolved.json"
COMMAND_TXT="$RUN_DIR/command.txt"
PREVIEW_BMP="$RUN_DIR/preview.bmp"
PLAYBACK_TRACE="$RUN_DIR/playback_trace.json"
SKILL_RESULT="$RUN_DIR/skill_result.json"
VIDEO_MP4="$RUN_DIR/video/route_preview.mp4"
FIRST_FRAME="$RUN_DIR/frames/frame_000001.bmp"

for path in "$MANIFEST" "$SUMMARY_MD" "$JOB_RESOLVED" "$COMMAND_TXT" "$PREVIEW_BMP"; do
    if [ ! -f "$path" ]; then
        echo "stage_saved_pin_visualizer_drop.sh: required artifact missing: $path" >&2
        exit 1
    fi
done

PRIMARY_SOURCE=""
PRIMARY_RELPATH=""
PRIMARY_MEDIA_TYPE=""
if [ -f "$VIDEO_MP4" ]; then
    PRIMARY_SOURCE="$VIDEO_MP4"
    PRIMARY_RELPATH="outputs/final/route_preview.mp4"
    PRIMARY_MEDIA_TYPE="video/mp4"
elif [ -f "$FIRST_FRAME" ]; then
    PRIMARY_SOURCE="$FIRST_FRAME"
    PRIMARY_RELPATH="outputs/final/frame_000001.bmp"
    PRIMARY_MEDIA_TYPE="image/bmp"
else
    PRIMARY_SOURCE="$PREVIEW_BMP"
    PRIMARY_RELPATH="outputs/final/preview.bmp"
    PRIMARY_MEDIA_TYPE="image/bmp"
fi

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_visualizer_drop.XXXXXX")
LOG_SOURCE="$TMP_DIR/run.log"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

{
    printf 'MapForge Visualizer Drop Log\n'
    printf 'Run Dir: %s\n\n' "$RUN_DIR"
    printf '== command.txt ==\n'
    cat "$COMMAND_TXT"
    printf '\n== summary.md ==\n'
    cat "$SUMMARY_MD"
} > "$LOG_SOURCE"

if [ -z "$SUMMARY" ]; then
    SUMMARY=$(python3 - <<'PY' "$SUMMARY_MD"
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text(encoding="utf-8").splitlines()
for line in text:
    line = line.strip()
    if line.startswith("From: ") or line.startswith("To: ") or line.startswith("Region: "):
        continue
summary = ""
for line in text:
    line = line.strip()
    if line.startswith("Status: "):
        status = line.split(":", 1)[1].strip()
        summary = f"MapForge saved-pin route export ({status})"
        break
print(summary or "MapForge saved-pin route export")
PY
)
fi

set -- \
    --drop-id "$DROP_ID" \
    --run-id "$DROP_ID" \
    --program map-forge \
    --job-type saved-pin-route \
    --status completed \
    --staging-root "$STAGING_ROOT" \
    --preview-source "$PREVIEW_BMP" \
    --log-source "$LOG_SOURCE" \
    --primary-output-source "$PRIMARY_SOURCE" \
    --primary-output-relpath "$PRIMARY_RELPATH" \
    --summary "$SUMMARY" \
    --output "$MANIFEST|outputs/metadata/manifest.json|run-manifest|application/json" \
    --output "$SUMMARY_MD|outputs/metadata/summary.md|run-summary|text/markdown" \
    --output "$JOB_RESOLVED|outputs/metadata/job.resolved.json|resolved-job|application/json"

if [ -f "$PLAYBACK_TRACE" ]; then
    set -- "$@" --output "$PLAYBACK_TRACE|outputs/metadata/playback_trace.json|playback-trace|application/json"
fi

if [ -f "$SKILL_RESULT" ]; then
    set -- "$@" --output "$SKILL_RESULT|outputs/metadata/skill_result.json|skill-result|application/json"
fi

if [ "$WRITE_READY" = "true" ]; then
    set -- "$@" --write-ready
fi
if [ "$OVERWRITE" = "true" ]; then
    set -- "$@" --overwrite
fi
if [ "$CONVERT_BMP_TO_PNG" = "true" ]; then
    set -- "$@" --convert-bmp-to-png
fi

python3 "$STAGE_TOOL" "$@"
