#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
REPO_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
SKILL_HELPER="$SCRIPT_DIR/run_saved_pin_route_skill.sh"
STAGE_HELPER="$SCRIPT_DIR/stage_saved_pin_visualizer_drop.sh"
UPLOAD_HELPER="$SCRIPT_DIR/upload_visualizer_drop.sh"

REGION=""
FROM_PIN=""
TO_PIN=""
OUT_ROOT=""
ZOOM_LEVEL="balanced"
MOTION_PROFILE="balanced"
ORIENTATION_MODE="heading_up"
RENDER_MODE="map_route_marker"
INCLUDE_VIDEO="true"
INCLUDE_FRAMES=""
PUBLISH="true"
KEEP_BMP="${MAPFORGE_KEEP_BMP:-false}"
STAGING_ROOT="${REPO_DIR}/../_private_workspace_artifacts/codework_visualizer_runs"
SITE_BASE_URL="${SITE_BASE_URL:-}"
DROP_TIMESTAMP=""
RESULT_JSON_PATH=""

usage() {
    cat <<'EOF'
Usage:
  render_saved_pin_visualizer_publish.sh --region <region> --from <pin> --to <pin> --out-root <dir> [options]

Required:
  --region <region>              Region id
  --from <pin>                   Saved start pin id or exact name
  --to <pin>                     Saved goal pin id or exact name
  --out-root <dir>               Parent directory for generated run folders

Options:
  --zoom-level <level>           close | balanced | wide
  --motion-profile <name>        responsive | balanced | cinematic
  --orientation-mode <mode>      heading_up | north_up | both
  --render-mode <mode>           map_route_marker | map_route | map_only
  --include-video <bool>         true | false
  --include-frames <bool>        true | false
  --publish <bool>               true | false
  --keep-bmp <bool>              true | false; skip BMP-to-PNG normalization
  --staging-root <dir>           Local staged-drop root
  --site-base-url <url>          Optional absolute website base URL
  --drop-timestamp <stamp>       Override UTC timestamp (YYYYMMDDTHHMMSSZ)
  --write-result-json <path>     Optional copy path for publish result JSON
  --help                         Show usage
EOF
}

require_bool() {
    case "$1" in
        true|false) return 0 ;;
        *)
            echo "render_saved_pin_visualizer_publish.sh: expected true|false but got '$1'" >&2
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
        --out-root)
            OUT_ROOT=${2:-}
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
        --publish)
            PUBLISH=${2:-}
            shift 2
            ;;
        --keep-bmp)
            KEEP_BMP=${2:-}
            shift 2
            ;;
        --staging-root)
            STAGING_ROOT=${2:-}
            shift 2
            ;;
        --site-base-url)
            SITE_BASE_URL=${2:-}
            shift 2
            ;;
        --drop-timestamp)
            DROP_TIMESTAMP=${2:-}
            shift 2
            ;;
        --write-result-json)
            RESULT_JSON_PATH=${2:-}
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "render_saved_pin_visualizer_publish.sh: unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [ -z "$REGION" ] || [ -z "$FROM_PIN" ] || [ -z "$TO_PIN" ] || [ -z "$OUT_ROOT" ]; then
    echo "render_saved_pin_visualizer_publish.sh: --region, --from, --to, and --out-root are required" >&2
    usage >&2
    exit 1
fi

require_bool "$INCLUDE_VIDEO"
require_bool "$PUBLISH"
require_bool "$KEEP_BMP"
if [ -n "$INCLUDE_FRAMES" ]; then
    require_bool "$INCLUDE_FRAMES"
fi

case "$ORIENTATION_MODE" in
    heading_up|north_up|both) ;;
    *)
        echo "render_saved_pin_visualizer_publish.sh: unsupported orientation mode '$ORIENTATION_MODE'" >&2
        exit 1
        ;;
esac

if [ -z "$DROP_TIMESTAMP" ]; then
    DROP_TIMESTAMP=$(date -u +"%Y%m%dT%H%M%SZ")
fi

mkdir -p "$OUT_ROOT"

TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mapforge_visualizer_publish.XXXXXX")
RESULT_JSON="$TMP_DIR/publish_result.json"
RUNS_JSONL="$TMP_DIR/runs.jsonl"

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

sanitize_nonce() {
    printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | tr -cd 'a-z0-9'
}

append_run_record() {
    python3 - <<'PY' "$RUNS_JSONL" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}" "${12}" "${13}"
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
record = {
    "orientation_mode": sys.argv[2],
    "run_dir": sys.argv[3],
    "drop_id": sys.argv[4],
    "stage_dir": sys.argv[5],
    "published": sys.argv[6] == "true",
    "manifest": sys.argv[7],
    "summary": sys.argv[8],
    "preview": sys.argv[9],
    "video": sys.argv[10],
    "preview_url": sys.argv[11],
    "video_url": sys.argv[12],
    "preview_rel_url": sys.argv[13],
    "video_rel_url": sys.argv[14],
}
with path.open("a", encoding="utf-8") as f:
    f.write(json.dumps(record) + "\n")
PY
}

ORIENTATION_LIST="heading_up"
if [ "$ORIENTATION_MODE" = "north_up" ]; then
    ORIENTATION_LIST="north_up"
elif [ "$ORIENTATION_MODE" = "both" ]; then
    ORIENTATION_LIST="heading_up north_up"
fi

BASE_NONCE=$(sanitize_nonce "${FROM_PIN}${TO_PIN}${ZOOM_LEVEL}${MOTION_PROFILE}")
if [ -z "$BASE_NONCE" ]; then
    BASE_NONCE="route"
fi

for orientation in $ORIENTATION_LIST; do
    if [ "$ORIENTATION_MODE" = "both" ]; then
        RUN_DIR="$OUT_ROOT/$orientation"
    else
        RUN_DIR="$OUT_ROOT"
    fi

    case "$orientation" in
        heading_up) ORIENTATION_NONCE="${BASE_NONCE}hu" ;;
        north_up) ORIENTATION_NONCE="${BASE_NONCE}nu" ;;
        *) ORIENTATION_NONCE="${BASE_NONCE}" ;;
    esac
    DROP_ID="map-forge--saved-pin-route--${DROP_TIMESTAMP}--${ORIENTATION_NONCE}"

    if [ -n "$INCLUDE_FRAMES" ]; then
        /bin/sh "$SKILL_HELPER" \
            --region "$REGION" \
            --from "$FROM_PIN" \
            --to "$TO_PIN" \
            --out "$RUN_DIR" \
            --zoom-level "$ZOOM_LEVEL" \
            --motion-profile "$MOTION_PROFILE" \
            --orientation-mode "$orientation" \
            --render-mode "$RENDER_MODE" \
            --include-video "$INCLUDE_VIDEO" \
            --include-frames "$INCLUDE_FRAMES" \
            > /dev/null
    else
        /bin/sh "$SKILL_HELPER" \
            --region "$REGION" \
            --from "$FROM_PIN" \
            --to "$TO_PIN" \
            --out "$RUN_DIR" \
            --zoom-level "$ZOOM_LEVEL" \
            --motion-profile "$MOTION_PROFILE" \
            --orientation-mode "$orientation" \
            --render-mode "$RENDER_MODE" \
            --include-video "$INCLUDE_VIDEO" \
            > /dev/null
    fi

    set -- \
        --run-dir "$RUN_DIR" \
        --drop-id "$DROP_ID" \
        --staging-root "$STAGING_ROOT" \
        --write-ready \
        --overwrite
    if [ "$KEEP_BMP" = "true" ]; then
        set -- "$@" --keep-bmp
    fi
    /bin/sh "$STAGE_HELPER" "$@" > /dev/null

    STAGE_DIR="$STAGING_ROOT/$DROP_ID"
    if [ "$KEEP_BMP" = "true" ]; then
        PREVIEW_REL_URL="/artifacts/map-forge/$DROP_ID/preview/preview.bmp"
    else
        PREVIEW_REL_URL="/artifacts/map-forge/$DROP_ID/preview/preview.png"
    fi

    VIDEO_PATH=""
    VIDEO_REL_URL=""
    if [ -f "$RUN_DIR/video/route_preview.mp4" ]; then
        VIDEO_PATH="$RUN_DIR/video/route_preview.mp4"
        VIDEO_REL_URL="/artifacts/map-forge/$DROP_ID/outputs/final/route_preview.mp4"
    fi

    PREVIEW_URL="$PREVIEW_REL_URL"
    FINAL_VIDEO_URL="$VIDEO_REL_URL"
    if [ -n "$SITE_BASE_URL" ]; then
        BASE=${SITE_BASE_URL%/}
        PREVIEW_URL="$BASE$PREVIEW_REL_URL"
        if [ -n "$VIDEO_REL_URL" ]; then
            FINAL_VIDEO_URL="$BASE$VIDEO_REL_URL"
        fi
    fi

    if [ "$PUBLISH" = "true" ]; then
        /bin/sh "$UPLOAD_HELPER" "$STAGE_DIR" "$DROP_ID" > /dev/null
    fi

    append_run_record \
        "$orientation" \
        "$RUN_DIR" \
        "$DROP_ID" \
        "$STAGE_DIR" \
        "$PUBLISH" \
        "$RUN_DIR/manifest.json" \
        "$RUN_DIR/summary.md" \
        "$RUN_DIR/preview.bmp" \
        "$VIDEO_PATH" \
        "$PREVIEW_URL" \
        "$FINAL_VIDEO_URL" \
        "$PREVIEW_REL_URL" \
        "$VIDEO_REL_URL"
done

python3 - <<'PY' "$RUNS_JSONL" "$REGION" "$FROM_PIN" "$TO_PIN" "$ZOOM_LEVEL" "$MOTION_PROFILE" "$ORIENTATION_MODE" "$RENDER_MODE" "$INCLUDE_VIDEO" "$PUBLISH" "$RESULT_JSON"
import json
import sys
from pathlib import Path

runs_path = Path(sys.argv[1])
records = []
if runs_path.exists():
    for line in runs_path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            records.append(json.loads(line))

result = {
    "schema": "mapforge-saved-pin-visualizer-publish-result/v1",
    "region": sys.argv[2],
    "from_pin": sys.argv[3],
    "to_pin": sys.argv[4],
    "zoom_level": sys.argv[5],
    "motion_profile": sys.argv[6],
    "orientation_mode": sys.argv[7],
    "render_mode": sys.argv[8],
    "include_video": sys.argv[9] == "true",
    "published": sys.argv[10] == "true",
    "run_count": len(records),
    "runs": records,
}

out_path = Path(sys.argv[11])
out_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
PY

if [ -n "$RESULT_JSON_PATH" ]; then
    mkdir -p "$(dirname "$RESULT_JSON_PATH")"
    cp "$RESULT_JSON" "$RESULT_JSON_PATH"
fi

echo "status=complete"
echo "schema=mapforge-saved-pin-visualizer-publish-result/v1"
echo "result_json=${RESULT_JSON_PATH:-$RESULT_JSON}"
echo "run_count=$(wc -l < "$RUNS_JSONL" | tr -d ' ')"
python3 - <<'PY' "${RESULT_JSON_PATH:-$RESULT_JSON}"
import json
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))
for idx, run in enumerate(data.get("runs", []), start=1):
    prefix = f"run_{idx}"
    print(f"{prefix}_orientation={run.get('orientation_mode', '')}")
    print(f"{prefix}_drop_id={run.get('drop_id', '')}")
    print(f"{prefix}_run_dir={run.get('run_dir', '')}")
    print(f"{prefix}_preview_url={run.get('preview_url', '')}")
    print(f"{prefix}_video_url={run.get('video_url', '')}")
PY
