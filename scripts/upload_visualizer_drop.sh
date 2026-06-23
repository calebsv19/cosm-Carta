#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$SCRIPT_DIR/saved_pin_common.sh"
SCRIPT_NAME="upload_visualizer_drop.sh"

DROP_DIR=${1:-}
DROP_ID=${2:-}
REMOTE_HOST=${REMOTE_HOST:-vps}
REMOTE_USER=${REMOTE_USER:-caleb}
REMOTE_ROOT=${REMOTE_ROOT:-/srv/release-staging/codework-visualizer}
REMOTE_PREPARE=${REMOTE_PREPARE:-/home/caleb/bin/release/prepare_visualizer_drop.sh}
REMOTE_IMPORT=${REMOTE_IMPORT:-/home/caleb/bin/release/import_staged_visualizer_drops.sh}
MAPFORGE_UPLOAD_DRY_RUN=${MAPFORGE_UPLOAD_DRY_RUN:-false}

upload_fail() {
    reason=$1
    detail=${2:-}
    echo "$SCRIPT_NAME: preflight failed: $reason" >&2
    if [ -n "$detail" ]; then
        echo "$SCRIPT_NAME: detail=$detail" >&2
    fi
    exit 1
}

upload_path_is_safe_absolute() {
    value=$1
    case "$value" in
        /*) ;;
        *) return 1 ;;
    esac
    case "$value" in
        *".."*|*"'"*|*'"'*|*' '*|*'	'*|*'$'*|*'`'*|*'|'*|*';'*|*'&'*|*'<'*|*'>'*|*'('*|*')'*|*'{'*|*'}'*|*'['*|*']'*|*'!'*)
            return 1
            ;;
    esac
    printf '%s\n' "$value" | grep -Eq '^/[A-Za-z0-9_./-]+$'
}

upload_host_is_safe() {
    value=$1
    case "$value" in
        ""|-*|*":"*|*"@"*|*".."*|*"'"*|*'"'*|*' '*|*'	'*|*'$'*|*'`'*|*'|'*|*';'*|*'&'*|*'<'*|*'>'*)
            return 1
            ;;
    esac
    printf '%s\n' "$value" | grep -Eq '^[A-Za-z0-9][A-Za-z0-9_.-]*$'
}

upload_user_is_safe() {
    value=$1
    case "$value" in
        ""|-*|*"@"*|*"'"*|*'"'*|*' '*|*'	'*|*'$'*|*'`'*|*'|'*|*';'*|*'&'*|*'<'*|*'>'*)
            return 1
            ;;
    esac
    printf '%s\n' "$value" | grep -Eq '^[A-Za-z0-9_][A-Za-z0-9_.-]*$'
}

if [ -z "$DROP_DIR" ] || [ -z "$DROP_ID" ]; then
    echo "Usage: upload_visualizer_drop.sh <drop_dir> <drop_id>" >&2
    echo "Env overrides: REMOTE_HOST REMOTE_USER REMOTE_ROOT REMOTE_PREPARE REMOTE_IMPORT" >&2
    exit 1
fi

mapforge_require_bool "$SCRIPT_NAME" "$MAPFORGE_UPLOAD_DRY_RUN"

if ! mapforge_drop_id_valid "$DROP_ID"; then
    upload_fail "invalid_drop_id" "$DROP_ID"
fi

if ! upload_host_is_safe "$REMOTE_HOST"; then
    upload_fail "invalid_remote_host" "$REMOTE_HOST"
fi

if ! upload_user_is_safe "$REMOTE_USER"; then
    upload_fail "invalid_remote_user" "$REMOTE_USER"
fi

if ! upload_path_is_safe_absolute "$REMOTE_ROOT"; then
    upload_fail "invalid_remote_root" "$REMOTE_ROOT"
fi

if ! upload_path_is_safe_absolute "$REMOTE_PREPARE"; then
    upload_fail "invalid_remote_prepare" "$REMOTE_PREPARE"
fi

if ! upload_path_is_safe_absolute "$REMOTE_IMPORT"; then
    upload_fail "invalid_remote_import" "$REMOTE_IMPORT"
fi

if [ ! -d "$DROP_DIR" ]; then
    upload_fail "missing_drop_dir" "$DROP_DIR"
fi

if [ ! -f "$DROP_DIR/manifest.json" ] || [ ! -f "$DROP_DIR/SHA256SUMS" ] || [ ! -f "$DROP_DIR/READY" ]; then
    upload_fail "missing_required_artifact" "$DROP_DIR"
fi

if [ "$MAPFORGE_UPLOAD_DRY_RUN" = "false" ]; then
    if ! command -v ssh >/dev/null 2>&1; then
        upload_fail "missing_ssh" "ssh"
    fi
    if ! command -v rsync >/dev/null 2>&1; then
        upload_fail "missing_rsync" "rsync"
    fi
fi

REMOTE_TARGET="$REMOTE_ROOT/$DROP_ID/"
REMOTE_ADDR="$REMOTE_USER@$REMOTE_HOST"

if [ "$MAPFORGE_UPLOAD_DRY_RUN" = "true" ]; then
    echo "visualizer drop upload dry-run complete"
    echo "drop_id=$DROP_ID"
    echo "local_drop_dir=$DROP_DIR"
    echo "remote_drop_dir=$REMOTE_TARGET"
    exit 0
fi

ssh "$REMOTE_ADDR" "$REMOTE_PREPARE --drop-id '$DROP_ID'"
rsync -az --delete --exclude READY "$DROP_DIR/" "$REMOTE_ADDR:$REMOTE_TARGET"
rsync -az "$DROP_DIR/READY" "$REMOTE_ADDR:$REMOTE_TARGET/READY"
ssh "$REMOTE_ADDR" "$REMOTE_IMPORT --drop-id '$DROP_ID'"

echo "visualizer drop upload complete"
echo "drop_id=$DROP_ID"
echo "local_drop_dir=$DROP_DIR"
echo "remote_drop_dir=$REMOTE_TARGET"
