#!/bin/sh

set -eu

CLI_BIN="$1"
DB_PATH="$2"
INVALID_OUTPUT_PATH="${DB_PATH}.invalid.out"

rm -f "$DB_PATH"
rm -f "$INVALID_OUTPUT_PATH"

if "$CLI_BIN" list >"$INVALID_OUTPUT_PATH" 2>&1; then
  echo "list unexpectedly succeeded without required args" >&2
  exit 1
fi
case "$(cat "$INVALID_OUTPUT_PATH")" in
  *"usage:"*) ;;
  *)
    echo "invalid usage output did not print usage" >&2
    exit 1
    ;;
esac
rm -f "$INVALID_OUTPUT_PATH"

ADD_ONE_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "First Note" --body "hello memory console" --stable-id first-note --workspace shared --project mem_console --kind summary)"
case "$ADD_ONE_OUTPUT" in
  *"added id="*"stable_id=first-note"*) ;;
  *)
    echo "unexpected add output: $ADD_ONE_OUTPUT" >&2
    exit 1
    ;;
esac

ADD_TWO_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "Second Note" --body "other entry for search" --workspace shared --project mem_console --kind issue)"
case "$ADD_TWO_OUTPUT" in
  *"added id="*) ;;
  *)
    echo "unexpected second add output: $ADD_TWO_OUTPUT" >&2
    exit 1
    ;;
esac

ADD_THREE_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "Third Note" --body "transient detail for rollup" --workspace scratch --project sandbox --kind transient)"
case "$ADD_THREE_OUTPUT" in
  *"added id=3"*) ;;
  *)
    echo "unexpected third add output: $ADD_THREE_OUTPUT" >&2
    exit 1
    ;;
esac

UPDATE_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "Second Note" --body "other entry for search" --stable-id second-note --workspace shared --project mem_console --kind issue)"
case "$UPDATE_OUTPUT" in
  *"updated id=2"*) ;;
  *)
    echo "unexpected dedupe update output: $UPDATE_OUTPUT" >&2
    exit 1
    ;;
esac

PIN_OUTPUT="$("$CLI_BIN" pin --db "$DB_PATH" --id 1 --on)"
case "$PIN_OUTPUT" in
  *"pin id=1 on"*) ;;
  *)
    echo "unexpected pin output: $PIN_OUTPUT" >&2
    exit 1
    ;;
esac

CANONICAL_OUTPUT="$("$CLI_BIN" canonical --db "$DB_PATH" --id 2 --on)"
case "$CANONICAL_OUTPUT" in
  *"canonical id=2 on"*) ;;
  *)
    echo "unexpected canonical output: $CANONICAL_OUTPUT" >&2
    exit 1
    ;;
esac

LINK_ADD_OUTPUT="$("$CLI_BIN" link-add --db "$DB_PATH" --from 1 --to 2 --kind supports --weight 0.75 --note "initial link")"
case "$LINK_ADD_OUTPUT" in
  *"link added id=1 from=1 to=2 kind=supports"*) ;;
  *)
    echo "unexpected link-add output: $LINK_ADD_OUTPUT" >&2
    exit 1
    ;;
esac

if "$CLI_BIN" link-add --db "$DB_PATH" --from 1 --to 2 --kind supports >"${DB_PATH}.link_dup.out" 2>&1; then
  echo "duplicate link-add unexpectedly succeeded" >&2
  exit 1
fi
case "$(cat "${DB_PATH}.link_dup.out")" in
  *"UNIQUE"*|*"constraint"* ) ;;
  *)
    echo "duplicate link-add failure output missing expected marker: $(cat "${DB_PATH}.link_dup.out")" >&2
    exit 1
    ;;
esac
rm -f "${DB_PATH}.link_dup.out"

if "$CLI_BIN" link-add --db "$DB_PATH" --from 1 --to 1 --kind supports >"${DB_PATH}.link_self.out" 2>&1; then
  echo "self-loop link-add unexpectedly succeeded" >&2
  exit 1
fi
case "$(cat "${DB_PATH}.link_self.out")" in
  *"self-loop edges are not allowed"* ) ;;
  *)
    echo "self-loop link-add output missing expected marker: $(cat "${DB_PATH}.link_self.out")" >&2
    exit 1
    ;;
esac
rm -f "${DB_PATH}.link_self.out"

if "$CLI_BIN" link-add --db "$DB_PATH" --from 1 --to 2 --kind invalid_kind >"${DB_PATH}.link_kind.out" 2>&1; then
  echo "invalid kind link-add unexpectedly succeeded" >&2
  exit 1
fi
case "$(cat "${DB_PATH}.link_kind.out")" in
  *"invalid --kind value"* ) ;;
  *)
    echo "invalid kind link-add output missing expected marker: $(cat "${DB_PATH}.link_kind.out")" >&2
    exit 1
    ;;
esac
rm -f "${DB_PATH}.link_kind.out"

LINK_LIST_OUTPUT="$("$CLI_BIN" link-list --db "$DB_PATH" --item-id 1)"
case "$LINK_LIST_OUTPUT" in
  *"1 | 1 -> 2 | kind=supports"* ) ;;
  *)
    echo "link-list output missing added link: $LINK_LIST_OUTPUT" >&2
    exit 1
    ;;
esac

NEIGHBORS_OUTPUT="$("$CLI_BIN" neighbors --db "$DB_PATH" --item-id 1 --max-edges 16 --max-nodes 8)"
case "$NEIGHBORS_OUTPUT" in
  *"kind=supports"*"neighbor=2"* ) ;;
  *)
    echo "neighbors output missing expected linked neighbor: $NEIGHBORS_OUTPUT" >&2
    exit 1
    ;;
esac

NEIGHBORS_JSON_OUTPUT="$("$CLI_BIN" neighbors --db "$DB_PATH" --item-id 1 --format json)"
case "$NEIGHBORS_JSON_OUTPUT" in
  \[*\] ) ;;
  *)
    echo "neighbors json output is not a json array: $NEIGHBORS_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$NEIGHBORS_JSON_OUTPUT" in
  *"\"neighbor_id\":2"* ) ;;
  *)
    echo "neighbors json output missing neighbor id: $NEIGHBORS_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

LINK_UPDATE_OUTPUT="$("$CLI_BIN" link-update --db "$DB_PATH" --id 1 --kind references --weight 1.25 --note "updated link")"
case "$LINK_UPDATE_OUTPUT" in
  *"link updated id=1"*) ;;
  *)
    echo "unexpected link-update output: $LINK_UPDATE_OUTPUT" >&2
    exit 1
    ;;
esac

LINK_LIST_UPDATED_OUTPUT="$("$CLI_BIN" link-list --db "$DB_PATH" --item-id 1)"
case "$LINK_LIST_UPDATED_OUTPUT" in
  *"1 | 1 -> 2 | kind=references | weight=1.25 | note=updated link"* ) ;;
  *)
    echo "link-list output missing updated link values: $LINK_LIST_UPDATED_OUTPUT" >&2
    exit 1
    ;;
esac

LINK_REMOVE_OUTPUT="$("$CLI_BIN" link-remove --db "$DB_PATH" --id 1)"
case "$LINK_REMOVE_OUTPUT" in
  *"link removed id=1"*) ;;
  *)
    echo "unexpected link-remove output: $LINK_REMOVE_OUTPUT" >&2
    exit 1
    ;;
esac

LINK_LIST_AFTER_REMOVE_OUTPUT="$("$CLI_BIN" link-list --db "$DB_PATH" --item-id 1)"
case "$LINK_LIST_AFTER_REMOVE_OUTPUT" in
  *"no links for item_id=1"* ) ;;
  *)
    echo "link-list output expected no links after remove: $LINK_LIST_AFTER_REMOVE_OUTPUT" >&2
    exit 1
    ;;
esac

LIST_OUTPUT="$("$CLI_BIN" list --db "$DB_PATH")"
LINE_COUNT="$(printf '%s\n' "$LIST_OUTPUT" | wc -l | tr -d ' ')"
if [ "$LINE_COUNT" -ne 3 ]; then
  echo "list output line count mismatch: $LINE_COUNT" >&2
  echo "$LIST_OUTPUT" >&2
  exit 1
fi
case "$LIST_OUTPUT" in
  *"First Note"* ) ;;
  *)
    echo "list output missing First Note: $LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$LIST_OUTPUT" in
  "1 | First Note"*) ;;
  *)
    echo "list output does not start with pinned First Note: $LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$LIST_OUTPUT" in
  *"Second Note | stable_id=second-note"* ) ;;
  *)
    echo "list output missing updated Second Note stable_id: $LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$LIST_OUTPUT" in
  *"Third Note"* ) ;;
  *)
    echo "list output missing Third Note before rollup: $LIST_OUTPUT" >&2
    exit 1
    ;;
esac

LIST_TSV_OUTPUT="$("$CLI_BIN" list --db "$DB_PATH" --format tsv)"
LIST_TSV_VALID_ROWS="$(printf '%s\n' "$LIST_TSV_OUTPUT" | awk -F'\t' 'NF==3 {count+=1} END {print count+0}')"
if [ "$LIST_TSV_VALID_ROWS" -ne 3 ]; then
  echo "list tsv output does not have expected 3-column rows: $LIST_TSV_OUTPUT" >&2
  exit 1
fi

LIST_JSON_OUTPUT="$("$CLI_BIN" list --db "$DB_PATH" --format json)"
case "$LIST_JSON_OUTPUT" in
  \[*\] ) ;;
  *)
    echo "list json output is not a json array: $LIST_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$LIST_JSON_OUTPUT" in
  *"\"title\":\"First Note\""* ) ;;
  *)
    echo "list json output missing First Note title: $LIST_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

FIND_MEMORY_OUTPUT="$("$CLI_BIN" find --db "$DB_PATH" --query memory)"
case "$FIND_MEMORY_OUTPUT" in
  *"First Note"* ) ;;
  *)
    echo "find output missing First Note: $FIND_MEMORY_OUTPUT" >&2
    exit 1
    ;;
esac

FIND_SEARCH_OUTPUT="$("$CLI_BIN" find --db "$DB_PATH" --query search)"
case "$FIND_SEARCH_OUTPUT" in
  *"Second Note"* ) ;;
  *)
    echo "find output missing Second Note: $FIND_SEARCH_OUTPUT" >&2
    exit 1
    ;;
esac

FIND_JSON_OUTPUT="$("$CLI_BIN" find --db "$DB_PATH" --query search --format json)"
case "$FIND_JSON_OUTPUT" in
  \[*\] ) ;;
  *)
    echo "find json output is not a json array: $FIND_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$FIND_JSON_OUTPUT" in
  *"\"title\":\"Second Note\""* ) ;;
  *)
    echo "find json output missing Second Note title: $FIND_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --query memory --limit 1)"
QUERY_LINE_COUNT="$(printf '%s\n' "$QUERY_OUTPUT" | wc -l | tr -d ' ')"
if [ "$QUERY_LINE_COUNT" -ne 1 ]; then
  echo "query output line count mismatch: $QUERY_LINE_COUNT" >&2
  echo "$QUERY_OUTPUT" >&2
  exit 1
fi
case "$QUERY_OUTPUT" in
  *"First Note"* ) ;;
  *)
    echo "query output missing First Note: $QUERY_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_PINNED_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --pinned-only --limit 10)"
case "$QUERY_PINNED_OUTPUT" in
  *"pinned=1"* ) ;;
  *)
    echo "query pinned output missing pinned row marker: $QUERY_PINNED_OUTPUT" >&2
    exit 1
    ;;
esac
case "$QUERY_PINNED_OUTPUT" in
  *"First Note"* ) ;;
  *)
    echo "query pinned output missing First Note: $QUERY_PINNED_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_SCOPE_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --workspace shared --project mem_console --kind summary --limit 10)"
case "$QUERY_SCOPE_OUTPUT" in
  *"First Note"* ) ;;
  *)
    echo "query scope output missing First Note: $QUERY_SCOPE_OUTPUT" >&2
    exit 1
    ;;
esac
case "$QUERY_SCOPE_OUTPUT" in
  *"Second Note"*|*"Third Note"* )
    echo "query scope output contains non-matching rows: $QUERY_SCOPE_OUTPUT" >&2
    exit 1
    ;;
  *) ;;
esac

QUERY_SCOPE_EMPTY_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --workspace does-not-exist --limit 10)"
if [ -n "$QUERY_SCOPE_EMPTY_OUTPUT" ]; then
  echo "query scope empty output expected no rows but got: $QUERY_SCOPE_EMPTY_OUTPUT" >&2
  exit 1
fi

ITEM_RETAG_OUTPUT="$("$CLI_BIN" item-retag --db "$DB_PATH" --id 3 --workspace shared --project mem_console)"
case "$ITEM_RETAG_OUTPUT" in
  *"item-retag id=3 workspace=shared project=mem_console kind=transient"* ) ;;
  *)
    echo "unexpected item-retag output: $ITEM_RETAG_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_RETAG_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --workspace shared --project mem_console --kind transient --limit 10)"
case "$QUERY_RETAG_OUTPUT" in
  *"Third Note"* ) ;;
  *)
    echo "query output missing retagged Third Note: $QUERY_RETAG_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_CANONICAL_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --canonical-only --limit 10)"
case "$QUERY_CANONICAL_OUTPUT" in
  *"canonical=1"* ) ;;
  *)
    echo "query canonical output missing canonical row marker: $QUERY_CANONICAL_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_JSON_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --query memory --limit 2 --format json)"
case "$QUERY_JSON_OUTPUT" in
  \[*\] ) ;;
  *)
    echo "query json output is not a json array: $QUERY_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$QUERY_JSON_OUTPUT" in
  *"\"title\":\"First Note\""* ) ;;
  *)
    echo "query json output missing First Note title: $QUERY_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

QUERY_TSV_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --canonical-only --limit 5 --format tsv)"
QUERY_TSV_VALID_ROWS="$(printf '%s\n' "$QUERY_TSV_OUTPUT" | awk -F'\t' 'NF==6 {count+=1} END {print count+0}')"
if [ "$QUERY_TSV_VALID_ROWS" -lt 1 ]; then
  echo "query tsv output does not have expected 6-column rows: $QUERY_TSV_OUTPUT" >&2
  exit 1
fi
case "$QUERY_CANONICAL_OUTPUT" in
  *"Second Note"* ) ;;
  *)
    echo "query canonical output missing Second Note: $QUERY_CANONICAL_OUTPUT" >&2
    exit 1
    ;;
esac

SHOW_OUTPUT="$("$CLI_BIN" show --db "$DB_PATH" --id 1)"
case "$SHOW_OUTPUT" in
  *"id: 1"* ) ;;
  *)
    echo "show output missing id: $SHOW_OUTPUT" >&2
    exit 1
    ;;
esac
case "$SHOW_OUTPUT" in
  *"title: First Note"* ) ;;
  *)
    echo "show output missing title: $SHOW_OUTPUT" >&2
    exit 1
    ;;
esac
case "$SHOW_OUTPUT" in
  *"body: hello memory console"* ) ;;
  *)
    echo "show output missing body: $SHOW_OUTPUT" >&2
    exit 1
    ;;
esac

SHOW_TWO_OUTPUT="$("$CLI_BIN" show --db "$DB_PATH" --id 2)"
case "$SHOW_TWO_OUTPUT" in
  *"stable_id: second-note"* ) ;;
  *)
    echo "show output missing dedupe-updated stable_id: $SHOW_TWO_OUTPUT" >&2
    exit 1
    ;;
esac
case "$SHOW_TWO_OUTPUT" in
  *"canonical: 1"* ) ;;
  *)
    echo "show output missing canonical flag: $SHOW_TWO_OUTPUT" >&2
    exit 1
    ;;
esac

SHOW_JSON_OUTPUT="$("$CLI_BIN" show --db "$DB_PATH" --id 1 --format json)"
case "$SHOW_JSON_OUTPUT" in
  \{*\} ) ;;
  *)
    echo "show json output is not a json object: $SHOW_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$SHOW_JSON_OUTPUT" in
  *"\"id\":1"* ) ;;
  *)
    echo "show json output missing id field: $SHOW_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$SHOW_JSON_OUTPUT" in
  *"\"title\":\"First Note\""* ) ;;
  *)
    echo "show json output missing title: $SHOW_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

SHOW_TSV_OUTPUT="$("$CLI_BIN" show --db "$DB_PATH" --id 1 --format tsv)"
SHOW_TSV_VALID_ROWS="$(printf '%s\n' "$SHOW_TSV_OUTPUT" | awk -F'\t' 'NF==11 {count+=1} END {print count+0}')"
if [ "$SHOW_TSV_VALID_ROWS" -ne 1 ]; then
  echo "show tsv output does not have expected 11 columns: $SHOW_TSV_OUTPUT" >&2
  exit 1
fi

ROLLUP_OUTPUT="$("$CLI_BIN" rollup --db "$DB_PATH" --before 9223372036854775807)"
case "$ROLLUP_OUTPUT" in
  *"rolled up 1 rows into id=4"*) ;;
  *)
    echo "unexpected rollup output: $ROLLUP_OUTPUT" >&2
    exit 1
    ;;
esac

POST_ROLLUP_LIST_OUTPUT="$("$CLI_BIN" list --db "$DB_PATH")"
POST_ROLLUP_LINE_COUNT="$(printf '%s\n' "$POST_ROLLUP_LIST_OUTPUT" | wc -l | tr -d ' ')"
if [ "$POST_ROLLUP_LINE_COUNT" -ne 3 ]; then
  echo "post-rollup list output line count mismatch: $POST_ROLLUP_LINE_COUNT" >&2
  echo "$POST_ROLLUP_LIST_OUTPUT" >&2
  exit 1
fi
case "$POST_ROLLUP_LIST_OUTPUT" in
  *"Third Note"* )
    echo "post-rollup list still includes archived Third Note: $POST_ROLLUP_LIST_OUTPUT" >&2
    exit 1
    ;;
  *) ;;
esac
case "$POST_ROLLUP_LIST_OUTPUT" in
  *"Rollup before 9223372036854775807 (1 items)"* ) ;;
  *)
    echo "post-rollup list missing summary row: $POST_ROLLUP_LIST_OUTPUT" >&2
    exit 1
    ;;
esac

POST_ROLLUP_FIND_OUTPUT="$("$CLI_BIN" find --db "$DB_PATH" --query transient)"
case "$POST_ROLLUP_FIND_OUTPUT" in
  *"Third Note"* )
    echo "post-rollup find still returns archived Third Note: $POST_ROLLUP_FIND_OUTPUT" >&2
    exit 1
    ;;
  *) ;;
esac
case "$POST_ROLLUP_FIND_OUTPUT" in
  *"Rollup before 9223372036854775807 (1 items)"* ) ;;
  *)
    echo "post-rollup find missing summary row: $POST_ROLLUP_FIND_OUTPUT" >&2
    exit 1
    ;;
esac

SHOW_THREE_OUTPUT="$("$CLI_BIN" show --db "$DB_PATH" --id 3)"
case "$SHOW_THREE_OUTPUT" in
  *"archived_ns: "* ) ;;
  *)
    echo "show output missing archived_ns field for rolled row: $SHOW_THREE_OUTPUT" >&2
    exit 1
    ;;
esac
ARCHIVED_VALUE="$(printf '%s\n' "$SHOW_THREE_OUTPUT" | awk -F': ' '/^archived_ns:/ {print $2; exit}')"
if [ -z "$ARCHIVED_VALUE" ]; then
  echo "rolled row was not archived: $SHOW_THREE_OUTPUT" >&2
  exit 1
fi

case "$SHOW_OUTPUT" in
  *"pinned: 1"* ) ;;
  *)
    echo "show output missing pinned flag: $SHOW_OUTPUT" >&2
    exit 1
    ;;
esac

if "$CLI_BIN" show --db "$DB_PATH" --id 999 >/dev/null 2>&1; then
  echo "show unexpectedly succeeded for a missing row" >&2
  exit 1
fi

HEALTH_JSON_OUTPUT="$("$CLI_BIN" health --db "$DB_PATH" --format json)"
case "$HEALTH_JSON_OUTPUT" in
  *"\"ok\":1"* ) ;;
  *)
    echo "health json output is not ok: $HEALTH_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

AUDIT_LIST_OUTPUT="$("$CLI_BIN" audit-list --db "$DB_PATH" --limit 256)"
case "$AUDIT_LIST_OUTPUT" in
  *"action=add"* ) ;;
  *)
    echo "audit-list missing add entries: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=health"* ) ;;
  *)
    echo "audit-list missing health entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=pin"* ) ;;
  *)
    echo "audit-list missing pin entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=canonical"* ) ;;
  *)
    echo "audit-list missing canonical entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=rollup"* ) ;;
  *)
    echo "audit-list missing rollup entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=link-add"* ) ;;
  *)
    echo "audit-list missing link-add entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=link-update"* ) ;;
  *)
    echo "audit-list missing link-update entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_LIST_OUTPUT" in
  *"action=link-remove"* ) ;;
  *)
    echo "audit-list missing link-remove entry: $AUDIT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_LIST_OUTPUT="$("$CLI_BIN" event-list --db "$DB_PATH" --limit 256)"
case "$EVENT_LIST_OUTPUT" in
  *"type=NodeCreated"* ) ;;
  *)
    echo "event-list missing NodeCreated entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=NodePinnedSet"* ) ;;
  *)
    echo "event-list missing NodePinnedSet entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=NodePinnedSet"*"updated_ns"* ) ;;
  *)
    echo "event-list pinned payload missing updated_ns marker: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=NodeCanonicalSet"* ) ;;
  *)
    echo "event-list missing NodeCanonicalSet entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=NodeMerged"* ) ;;
  *)
    echo "event-list missing NodeMerged entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=EdgeAdded"* ) ;;
  *)
    echo "event-list missing EdgeAdded entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=EdgeUpdated"* ) ;;
  *)
    echo "event-list missing EdgeUpdated entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_LIST_OUTPUT" in
  *"type=EdgeRemoved"* ) ;;
  *)
    echo "event-list missing EdgeRemoved entry: $EVENT_LIST_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_EDGE_JSON_OUTPUT="$("$CLI_BIN" event-list --db "$DB_PATH" --event-type EdgeAdded --format json)"
case "$EVENT_EDGE_JSON_OUTPUT" in
  \[*\] ) ;;
  *)
    echo "event-list json output is not a json array: $EVENT_EDGE_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_EDGE_JSON_OUTPUT" in
  *"\"event_type\":\"EdgeAdded\""* ) ;;
  *)
    echo "event-list json output missing filtered event type: $EVENT_EDGE_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_REPLAY_CHECK_OUTPUT="$("$CLI_BIN" event-replay-check --db "$DB_PATH")"
case "$EVENT_REPLAY_CHECK_OUTPUT" in
  *"status=ok"* ) ;;
  *)
    echo "event-replay-check did not report status=ok: $EVENT_REPLAY_CHECK_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_REPLAY_CHECK_JSON_OUTPUT="$("$CLI_BIN" event-replay-check --db "$DB_PATH" --format json)"
case "$EVENT_REPLAY_CHECK_JSON_OUTPUT" in
  *"\"ok\":1"* ) ;;
  *)
    echo "event-replay-check json output missing ok=1: $EVENT_REPLAY_CHECK_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

REPLAY_APPLY_DB_PATH="${DB_PATH}.replay_apply.sqlite"
rm -f "$REPLAY_APPLY_DB_PATH"
EVENT_REPLAY_APPLY_OUTPUT="$("$CLI_BIN" event-replay-apply --db "$DB_PATH" --out-db "$REPLAY_APPLY_DB_PATH")"
case "$EVENT_REPLAY_APPLY_OUTPUT" in
  *"status=ok"* ) ;;
  *)
    echo "event-replay-apply did not report status=ok: $EVENT_REPLAY_APPLY_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_REPLAY_APPLY_JSON_OUTPUT="$("$CLI_BIN" event-replay-apply --db "$DB_PATH" --out-db "$REPLAY_APPLY_DB_PATH" --format json)"
case "$EVENT_REPLAY_APPLY_JSON_OUTPUT" in
  *"\"ok\":1"* ) ;;
  *)
    echo "event-replay-apply json output missing ok=1: $EVENT_REPLAY_APPLY_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

REPLAY_APPLY_LIST_OUTPUT="$("$CLI_BIN" list --db "$REPLAY_APPLY_DB_PATH")"
case "$REPLAY_APPLY_LIST_OUTPUT" in
  *"First Note"* ) ;;
  *)
    echo "event-replay-apply target list missing First Note: $REPLAY_APPLY_LIST_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_BACKFILL_DRY_OUTPUT="$("$CLI_BIN" event-backfill --db "$DB_PATH" --dry-run)"
case "$EVENT_BACKFILL_DRY_OUTPUT" in
  *"dry_run=on"* ) ;;
  *)
    echo "event-backfill dry-run output missing dry_run marker: $EVENT_BACKFILL_DRY_OUTPUT" >&2
    exit 1
    ;;
esac

EVENT_BACKFILL_JSON_OUTPUT="$("$CLI_BIN" event-backfill --db "$DB_PATH" --format json)"
case "$EVENT_BACKFILL_JSON_OUTPUT" in
  *"\"ok\":1"* ) ;;
  *)
    echo "event-backfill json output missing ok=1: $EVENT_BACKFILL_JSON_OUTPUT" >&2
    exit 1
    ;;
esac
case "$EVENT_BACKFILL_JSON_OUTPUT" in
  *"\"events_written\":0"* ) ;;
  *)
    echo "event-backfill on fresh smoke DB should be idempotent (events_written=0): $EVENT_BACKFILL_JSON_OUTPUT" >&2
    exit 1
    ;;
esac

BUDGET_ONE_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "Budget Note One" --body "budget check one" --session-id smoke-session --session-max-writes 1)"
case "$BUDGET_ONE_OUTPUT" in
  *"added id="* ) ;;
  *)
    echo "budget first add unexpectedly failed: $BUDGET_ONE_OUTPUT" >&2
    exit 1
    ;;
esac

if "$CLI_BIN" add --db "$DB_PATH" --title "Budget Note Two" --body "budget check two" --session-id smoke-session --session-max-writes 1 >"${DB_PATH}.budget.out" 2>&1; then
  echo "budget second add unexpectedly succeeded" >&2
  exit 1
fi
case "$(cat "${DB_PATH}.budget.out")" in
  *"budget exceeded"* ) ;;
  *)
    echo "budget failure output missing expected marker: $(cat "${DB_PATH}.budget.out")" >&2
    exit 1
    ;;
esac
rm -f "${DB_PATH}.budget.out"

AUDIT_SESSION_OUTPUT="$("$CLI_BIN" audit-list --db "$DB_PATH" --session-id smoke-session --limit 64)"
case "$AUDIT_SESSION_OUTPUT" in
  *"status=ok"* ) ;;
  *)
    echo "session audit output missing successful write: $AUDIT_SESSION_OUTPUT" >&2
    exit 1
    ;;
esac
case "$AUDIT_SESSION_OUTPUT" in
  *"status=fail"* ) ;;
  *)
    echo "session audit output missing budget-fail entry: $AUDIT_SESSION_OUTPUT" >&2
    exit 1
    ;;
esac

BATCH_INPUT_PATH="${DB_PATH}.batch.tsv"
cat >"$BATCH_INPUT_PATH" <<'EOF'
Batch One	batch body one
batch-stable	Batch Two	batch body two	shared	mem_console	batch
EOF

BATCH_ADD_OUTPUT="$("$CLI_BIN" batch-add --db "$DB_PATH" --input "$BATCH_INPUT_PATH" --workspace shared --project mem_console --kind batch --retry-attempts 2 --retry-delay-ms 0)"
case "$BATCH_ADD_OUTPUT" in
  *"batch-add: loaded=2 ok=2 failed=0"* ) ;;
  *)
    echo "batch-add output unexpected: $BATCH_ADD_OUTPUT" >&2
    exit 1
    ;;
esac

BATCH_SCOPE_OUTPUT="$("$CLI_BIN" query --db "$DB_PATH" --workspace shared --project mem_console --kind batch --limit 10)"
case "$BATCH_SCOPE_OUTPUT" in
  *"Batch One"* ) ;;
  *)
    echo "batch scope output missing Batch One: $BATCH_SCOPE_OUTPUT" >&2
    exit 1
    ;;
esac
case "$BATCH_SCOPE_OUTPUT" in
  *"Batch Two"* ) ;;
  *)
    echo "batch scope output missing Batch Two: $BATCH_SCOPE_OUTPUT" >&2
    exit 1
    ;;
esac
rm -f "$BATCH_INPUT_PATH"

BATCH_FAIL_INPUT_PATH="${DB_PATH}.batch_fail.tsv"
cat >"$BATCH_FAIL_INPUT_PATH" <<'EOF'
invalid-row
another-invalid-row
Batch Three	batch body three
EOF

if "$CLI_BIN" batch-add --db "$DB_PATH" --input "$BATCH_FAIL_INPUT_PATH" --continue-on-error --max-errors 1 >"${DB_PATH}.batch_fail.out" 2>&1; then
  echo "batch-add unexpectedly succeeded despite max-errors stop condition" >&2
  exit 1
fi
case "$(cat "${DB_PATH}.batch_fail.out")" in
  *"batch-add: loaded=0 ok=0 failed=1"* ) ;;
  *)
    echo "batch-add max-errors output missing expected summary: $(cat "${DB_PATH}.batch_fail.out")" >&2
    exit 1
    ;;
esac
case "$(cat "${DB_PATH}.batch_fail.out")" in
  *"stopped after reaching --max-errors=1"* ) ;;
  *)
    echo "batch-add max-errors output missing stop marker: $(cat "${DB_PATH}.batch_fail.out")" >&2
    exit 1
    ;;
esac
rm -f "$BATCH_FAIL_INPUT_PATH"
rm -f "${DB_PATH}.batch_fail.out"
rm -f "$REPLAY_APPLY_DB_PATH"

rm -f "$DB_PATH"
rm -f "$INVALID_OUTPUT_PATH"
