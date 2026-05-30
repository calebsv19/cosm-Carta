#!/bin/sh
set -eu

DROP_DIR=${1:-}
DROP_ID=${2:-}
REMOTE_HOST=${REMOTE_HOST:-vps}
REMOTE_USER=${REMOTE_USER:-caleb}
REMOTE_ROOT=${REMOTE_ROOT:-/srv/release-staging/codework-visualizer}
REMOTE_PREPARE=${REMOTE_PREPARE:-/home/caleb/bin/release/prepare_visualizer_drop.sh}
REMOTE_IMPORT=${REMOTE_IMPORT:-/home/caleb/bin/release/import_staged_visualizer_drops.sh}

if [ -z "$DROP_DIR" ] || [ -z "$DROP_ID" ]; then
    echo "Usage: upload_visualizer_drop.sh <drop_dir> <drop_id>" >&2
    echo "Env overrides: REMOTE_HOST REMOTE_USER REMOTE_ROOT REMOTE_PREPARE REMOTE_IMPORT" >&2
    exit 1
fi

if [ ! -d "$DROP_DIR" ]; then
    echo "upload_visualizer_drop.sh: drop directory not found: $DROP_DIR" >&2
    exit 1
fi

if [ ! -f "$DROP_DIR/manifest.json" ] || [ ! -f "$DROP_DIR/SHA256SUMS" ] || [ ! -f "$DROP_DIR/READY" ]; then
    echo "upload_visualizer_drop.sh: drop is missing manifest.json, SHA256SUMS, or READY: $DROP_DIR" >&2
    exit 1
fi

REMOTE_TARGET="$REMOTE_ROOT/$DROP_ID/"
REMOTE_ADDR="$REMOTE_USER@$REMOTE_HOST"

ssh "$REMOTE_ADDR" "$REMOTE_PREPARE --drop-id '$DROP_ID'"
rsync -az --delete --exclude READY "$DROP_DIR/" "$REMOTE_ADDR:$REMOTE_TARGET"
rsync -az "$DROP_DIR/READY" "$REMOTE_ADDR:$REMOTE_TARGET/READY"
ssh "$REMOTE_ADDR" "$REMOTE_IMPORT --drop-id '$DROP_ID'"

echo "visualizer drop upload complete"
echo "drop_id=$DROP_ID"
echo "local_drop_dir=$DROP_DIR"
echo "remote_drop_dir=$REMOTE_TARGET"
