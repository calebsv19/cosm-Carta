#!/bin/sh
set -eu

CLI_BIN="${1:?mem_cli path required}"
DB_PATH="${2:?database path required}"
FLOW_BIN="$(cd "$(dirname "$CLI_BIN")/.." && pwd)/tools/mem_agent_flow.sh"

extract_id() {
    printf '%s\n' "$1" | sed -n 's/^[a-z][a-z]* id=\([0-9][0-9]*\).*/\1/p' | head -n1
}

add_item() {
    "$CLI_BIN" add --db "$DB_PATH" --title "$1" --body "$2" --stable-id "$3" \
        --workspace codework --project "$4" --kind "$5"
}

rm -f "$DB_PATH"

COLLISION_BODY="Outcome: collision. Evidence: same bytes. Remaining boundary: preserve identity. Next: inspect."
COLLISION_OUTPUT="$(add_item "Collision Title" "$COLLISION_BODY" collision-record demo note)"
COLLISION_ID="$(extract_id "$COLLISION_OUTPUT")"
UPSERT_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "Collision Title" --body "$COLLISION_BODY" \
    --stable-id lane-head-demo-collision --upsert-stable-id --workspace codework --project demo --kind summary)"
UPSERT_ID="$(extract_id "$UPSERT_OUTPUT")"
if [ -z "$COLLISION_ID" ] || [ -z "$UPSERT_ID" ] || [ "$COLLISION_ID" = "$UPSERT_ID" ]; then
    echo "stable upsert reused a fingerprint collision: collision=$COLLISION_ID upsert=$UPSERT_ID" >&2
    exit 1
fi
case "$("$CLI_BIN" show --db "$DB_PATH" --id "$COLLISION_ID" --format json)" in
    *'"stable_id":"collision-record"'*) ;;
    *) echo "fingerprint collision source identity changed" >&2; exit 1 ;;
esac

ANCHOR_OUTPUT="$(add_item "Lane anchor" "durable lane anchor" lane-anchor demo plan)"
LATEST_ONE_OUTPUT="$(add_item "Receipt one" "receipt one" receipt-one demo milestone)"
LATEST_TWO_OUTPUT="$(add_item "Receipt two" "receipt two" receipt-two demo milestone)"
ANCHOR_ID="$(extract_id "$ANCHOR_OUTPUT")"
LATEST_ONE_ID="$(extract_id "$LATEST_ONE_OUTPUT")"
LATEST_TWO_ID="$(extract_id "$LATEST_TWO_OUTPUT")"
BODY_ONE="Outcome: accepted first state. Evidence: receipt $LATEST_ONE_ID. Remaining boundary: second state. Next: advance once material."
BODY_TWO="Outcome: accepted second state. Evidence: receipt $LATEST_TWO_ID. Remaining boundary: none. Next: monitor."

CREATE_OUTPUT="$("$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "$BODY_ONE" --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_ONE_ID" \
    --session-id lane-create --session-max-writes 1)"
HEAD_ID="$(extract_id "$CREATE_OUTPUT")"
case "$CREATE_OUTPUT" in
    "created id=$HEAD_ID stable_id=lane-head-demo-validation"*) ;;
    *) echo "unexpected atomic lane-head create output: $CREATE_OUTPUT" >&2; exit 1 ;;
esac

NOOP_OUTPUT="$("$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "$BODY_ONE" --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_ONE_ID" \
    --session-id lane-noop --session-max-writes 1)"
case "$NOOP_OUTPUT" in
    "unchanged id=$HEAD_ID stable_id=lane-head-demo-validation"*) ;;
    *) echo "exact lane-head rerun was not a no-op: $NOOP_OUTPUT" >&2; exit 1 ;;
esac
case "$("$CLI_BIN" audit-list --db "$DB_PATH" --session-id lane-noop --limit 10 --format json)" in
    '[]') ;;
    *) echo "exact lane-head rerun created audit noise" >&2; exit 1 ;;
esac
case "$("$CLI_BIN" event-list --db "$DB_PATH" --session-id lane-noop --limit 10 --format json)" in
    '[]') ;;
    *) echo "exact lane-head rerun created event noise" >&2; exit 1 ;;
esac

UPDATE_OUTPUT="$("$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "$BODY_TWO" --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_TWO_ID" \
    --session-id lane-update --session-max-writes 1)"
case "$UPDATE_OUTPUT" in
    "updated id=$HEAD_ID stable_id=lane-head-demo-validation"*) ;;
    *) echo "unexpected atomic lane-head update output: $UPDATE_OUTPUT" >&2; exit 1 ;;
esac
LINKS_AFTER_UPDATE="$("$CLI_BIN" link-list --db "$DB_PATH" --item-id "$HEAD_ID")"
case "$LINKS_AFTER_UPDATE" in
    *"$HEAD_ID -> $LATEST_TWO_ID | kind=summarizes | note=lane-head-v1:latest:validation"*) ;;
    *) echo "lane-head update missing desired latest link" >&2; exit 1 ;;
esac

BUDGET_FIRST_OUTPUT="$("$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "$BODY_ONE" --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_ONE_ID" \
    --session-id lane-repeat-budget --session-max-writes 1)"
case "$BUDGET_FIRST_OUTPUT" in
    "updated id=$HEAD_ID stable_id=lane-head-demo-validation"*) ;;
    *) echo "first lane-head write did not consume the shared session budget: $BUDGET_FIRST_OUTPUT" >&2; exit 1 ;;
esac
if "$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "$BODY_TWO" --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_TWO_ID" \
    --session-id lane-repeat-budget --session-max-writes 1 >/dev/null 2>&1; then
    echo "a second changed lane-head write bypassed the shared session budget" >&2
    exit 1
fi
case "$("$CLI_BIN" show --db "$DB_PATH" --id "$HEAD_ID" --format json)" in
    *"$BODY_ONE"*) ;;
    *) echo "rejected second lane-head write partially changed the projection" >&2; exit 1 ;;
esac
"$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "$BODY_TWO" --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_TWO_ID" \
    --session-id lane-budget-restore --session-max-writes 1 >/dev/null
case "$LINKS_AFTER_UPDATE" in
    *"$HEAD_ID -> $LATEST_ONE_ID | kind=summarizes | note=lane-head-v1:latest:validation"*)
        echo "lane-head update retained stale latest link" >&2; exit 1 ;;
    *) ;;
esac

if "$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane overlap \
    --title "Overlapping Lane Head" \
    --body "Outcome: reject overlap. Evidence: same receipts. Remaining boundary: preserve one owner. Next: use the established lane key." \
    --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_TWO_ID" >/dev/null 2>&1; then
    echo "lane-head create unexpectedly accepted an already-owned anchor/latest pair" >&2
    exit 1
fi
case "$("$CLI_BIN" query --db "$DB_PATH" --workspace codework --project demo --query '"lane-head-demo-overlap"' --limit 4 --format json)" in
    '[]') ;;
    *) echo "overlap rejection left a partial lane head" >&2; exit 1 ;;
esac

OTHER_ANCHOR_OUTPUT="$(add_item "Other anchor" "other anchor" other-anchor demo plan)"
OTHER_ANCHOR_ID="$(extract_id "$OTHER_ANCHOR_OUTPUT")"
"$CLI_BIN" link-add --db "$DB_PATH" --from "$HEAD_ID" --to "$OTHER_ANCHOR_ID" \
    --kind references --note operator-owned >/dev/null
if "$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "Outcome: conflict. Evidence: operator edge. Remaining boundary: preserve old state. Next: resolve." \
    --anchor-id "$OTHER_ANCHOR_ID" --latest-id "$LATEST_TWO_ID" >/dev/null 2>&1; then
    echo "lane-head update unexpectedly accepted a conflicting unrelated edge" >&2
    exit 1
fi
case "$("$CLI_BIN" show --db "$DB_PATH" --id "$HEAD_ID" --format json)" in
    *"$BODY_TWO"*) ;;
    *) echo "failed edge preflight partially changed the lane-head body" >&2; exit 1 ;;
esac

"$CLI_BIN" add --db "$DB_PATH" --title "budget fill" --body "budget fill" --stable-id budget-fill \
    --workspace codework --project demo --kind note --session-id exhausted --session-max-writes 1 >/dev/null
if "$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace codework --project demo --lane validation \
    --title "Validation Lane Head" --body "Outcome: budget. Evidence: exhausted. Remaining boundary: preserve old state. Next: retry separately." \
    --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_ONE_ID" --session-id exhausted --session-max-writes 1 >/dev/null 2>&1; then
    echo "lane-head update unexpectedly exceeded its session budget" >&2
    exit 1
fi
case "$("$CLI_BIN" show --db "$DB_PATH" --id "$HEAD_ID" --format json)" in
    *"$BODY_TWO"*) ;;
    *) echo "budget failure partially changed the lane head" >&2; exit 1 ;;
esac

WRONG_SCOPE_OUTPUT="$("$CLI_BIN" add --db "$DB_PATH" --title "Wrong scope" --body "wrong scope" \
    --stable-id lane-head-demo-scope-guard --workspace codework --project other --kind summary)"
WRONG_SCOPE_ID="$(extract_id "$WRONG_SCOPE_OUTPUT")"
if "$CLI_BIN" lane-head-upsert --db "$DB_PATH" --workspace codework --project demo --lane scope-guard \
    --stable-id lane-head-demo-scope-guard --title "Replacement" \
    --body "Outcome: reject. Evidence: wrong scope. Remaining boundary: none. Next: stop." \
    --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_ONE_ID" >/dev/null 2>&1; then
    echo "lane-head upsert unexpectedly stole an identity from another project" >&2
    exit 1
fi
case "$("$CLI_BIN" show --db "$DB_PATH" --id "$WRONG_SCOPE_ID" --format json)" in
    *'"title":"Wrong scope"'*) ;;
    *) echo "scope collision changed the original item" >&2; exit 1 ;;
esac

if "$FLOW_BIN" write-lane-head --db "$DB_PATH" --workspace another --project demo --lane unsupported \
    --title "Unsupported" --body "Outcome: reject. Evidence: workspace. Remaining boundary: none. Next: stop." \
    --anchor-id "$ANCHOR_ID" --latest-id "$LATEST_ONE_ID" >/dev/null 2>&1; then
    echo "Lane Head V1 unexpectedly accepted a non-codework workspace" >&2
    exit 1
fi

REPLAY_JSON="$("$CLI_BIN" event-replay-check --db "$DB_PATH" --format json)"
case "$REPLAY_JSON" in
    *'"ok":1'*'"item_field_mismatch":0'*'"link_field_mismatch":0'*) ;;
    *) echo "lane-head hardening events did not replay exactly: $REPLAY_JSON" >&2; exit 1 ;;
esac

rm -f "$DB_PATH"
