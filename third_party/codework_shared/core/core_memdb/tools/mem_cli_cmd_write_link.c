#include "mem_cli_cmd_write_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_memdb.h"
#include "mem_cli_args.h"
#include "mem_cli_db_helpers.h"
#include "mem_cli_output.h"

static const char *kApplyLinkEventSql =
    "INSERT INTO mem_link (id, from_item_id, to_item_id, kind, weight, note) "
    "VALUES ("
    "?1, "
    "COALESCE(CAST(json_extract(?2, '$.from_item_id') AS INTEGER), CAST(json_extract(?2, '$.from') AS INTEGER), 0), "
    "COALESCE(CAST(json_extract(?2, '$.to_item_id') AS INTEGER), CAST(json_extract(?2, '$.to') AS INTEGER), 0), "
    "COALESCE(json_extract(?2, '$.kind'), ''), "
    "CASE "
    "  WHEN json_type(?2, '$.weight') IS NOT NULL THEN CAST(json_extract(?2, '$.weight') AS REAL) "
    "  ELSE NULL "
    "END, "
    "CASE "
    "  WHEN json_type(?2, '$.note') IS NOT NULL THEN json_extract(?2, '$.note') "
    "  ELSE NULL "
    "END"
    ") "
    "ON CONFLICT(id) DO UPDATE SET "
    "from_item_id = excluded.from_item_id, "
    "to_item_id = excluded.to_item_id, "
    "kind = excluded.kind, "
    "weight = excluded.weight, "
    "note = excluded.note;";

static CoreResult build_link_event_payload_alloc(CoreMemDb *db,
                                                 int64_t from_item_id,
                                                 int64_t to_item_id,
                                                 const char *kind,
                                                 int has_weight,
                                                 double weight_value,
                                                 int has_note,
                                                 const char *note,
                                                 char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr json_text = {0};
    char *buffer = 0;

    if (!db || from_item_id <= 0 || to_item_id <= 0 || !kind || !out_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_json = 0;

    result = core_memdb_prepare(db,
                                "SELECT json_object("
                                "'from_item_id', ?1, "
                                "'to_item_id', ?2, "
                                "'kind', ?3, "
                                "'weight', CASE WHEN ?4 <> 0 THEN ?5 ELSE NULL END, "
                                "'note', CASE WHEN ?6 <> 0 THEN ?7 ELSE NULL END"
                                ");",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, from_item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 2, to_item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 3, kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 4, has_weight ? 1 : 0);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_weight) {
        result = core_memdb_stmt_bind_f64(&stmt, 5, weight_value);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 5);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 6, has_note ? 1 : 0);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_note) {
        result = core_memdb_stmt_bind_text(&stmt, 7, note ? note : "");
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 7);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "link payload query returned no row" };
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 0, &json_text);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    if (!json_text.data || json_text.len == 0u) {
        buffer = (char *)core_alloc(3u);
        if (!buffer) {
            result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto cleanup;
        }
        memcpy(buffer, "{}", 3u);
        *out_json = buffer;
        buffer = 0;
        result = core_result_ok();
        goto cleanup;
    }

    buffer = (char *)core_alloc(json_text.len + 1u);
    if (!buffer) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    memcpy(buffer, json_text.data, json_text.len);
    buffer[json_text.len] = '\0';
    *out_json = buffer;
    buffer = 0;
    result = core_result_ok();

cleanup:
    core_free(buffer);
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

static CoreResult build_link_update_event_payload_alloc(CoreMemDb *db,
                                                        int64_t link_id,
                                                        const char *kind,
                                                        int has_weight,
                                                        double weight_value,
                                                        int has_note,
                                                        const char *note,
                                                        char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr json_text = {0};
    char *buffer = 0;

    if (!db || link_id <= 0 || !kind || !out_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_json = 0;

    result = core_memdb_prepare(db,
                                "SELECT json_object("
                                "'from_item_id', from_item_id, "
                                "'to_item_id', to_item_id, "
                                "'kind', ?2, "
                                "'weight', CASE WHEN ?3 <> 0 THEN ?4 ELSE weight END, "
                                "'note', CASE WHEN ?5 <> 0 THEN ?6 ELSE note END"
                                ") "
                                "FROM mem_link "
                                "WHERE id = ?1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 2, kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 3, has_weight ? 1 : 0);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_weight) {
        result = core_memdb_stmt_bind_f64(&stmt, 4, weight_value);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 4);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 5, has_note ? 1 : 0);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_note) {
        result = core_memdb_stmt_bind_text(&stmt, 6, note ? note : "");
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 6);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_NOT_FOUND, "link update payload source not found" };
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 0, &json_text);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    if (!json_text.data || json_text.len == 0u) {
        buffer = (char *)core_alloc(3u);
        if (!buffer) {
            result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto cleanup;
        }
        memcpy(buffer, "{}", 3u);
        *out_json = buffer;
        buffer = 0;
        result = core_result_ok();
        goto cleanup;
    }

    buffer = (char *)core_alloc(json_text.len + 1u);
    if (!buffer) {
        result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        goto cleanup;
    }
    memcpy(buffer, json_text.data, json_text.len);
    buffer[json_text.len] = '\0';
    *out_json = buffer;
    buffer = 0;
    result = core_result_ok();

cleanup:
    core_free(buffer);
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

int cmd_link_add(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *from_text = find_flag_value(argc, argv, "--from");
    const char *to_text = find_flag_value(argc, argv, "--to");
    const char *kind = find_flag_value(argc, argv, "--kind");
    const char *weight_text = find_flag_value(argc, argv, "--weight");
    const char *note = find_flag_value(argc, argv, "--note");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    int64_t from_item_id = 0;
    int64_t to_item_id = 0;
    int64_t link_id = 0;
    double weight_value = 0.0;
    int has_weight = 0;
    int has_row = 0;
    int tx_started = 0;
    int from_exists = 0;
    int to_exists = 0;
    int exit_code = 1;
    char stable_id[128];
    char workspace_key[128];
    char project_key[128];
    char item_kind[128];
    char detail[192];
    char *event_payload = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path || !from_text || !to_text || !kind ||
        !parse_i64_arg(from_text, &from_item_id) ||
        !parse_i64_arg(to_text, &to_item_id)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!is_allowed_link_kind(kind)) {
        fprintf(stderr, "link-add: invalid --kind value '%s'\n", kind);
        return 1;
    }
    if (from_item_id == to_item_id) {
        fprintf(stderr, "link-add: self-loop edges are not allowed\n");
        return 1;
    }
    if (weight_text) {
        if (!parse_f64_arg(weight_text, &weight_value)) {
            fprintf(stderr, "link-add: invalid --weight value\n");
            return 1;
        }
        has_weight = 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_tx_begin(&db);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    tx_started = 1;

    result = item_exists_active(&db, from_item_id, &from_exists);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    if (!from_exists) {
        fprintf(stderr, "link-add: from item %lld not found\n", (long long)from_item_id);
        goto cleanup;
    }

    result = item_exists_active(&db, to_item_id, &to_exists);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    if (!to_exists) {
        fprintf(stderr, "link-add: to item %lld not found\n", (long long)to_item_id);
        goto cleanup;
    }

    result = fetch_item_audit_metadata(&db,
                                       from_item_id,
                                       stable_id,
                                       sizeof(stable_id),
                                       workspace_key,
                                       sizeof(workspace_key),
                                       project_key,
                                       sizeof(project_key),
                                       item_kind,
                                       sizeof(item_kind));
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }

    result = fetch_next_link_id(&db, &link_id);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }

    result = build_link_event_payload_alloc(&db,
                                            from_item_id,
                                            to_item_id,
                                            kind,
                                            has_weight,
                                            weight_value,
                                            note != 0 ? 1 : 0,
                                            note,
                                            &event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    result = append_event_entry(&db,
                                session_id,
                                "EdgeAdded",
                                from_item_id,
                                link_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&db, kApplyLinkEventSql, &stmt);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 2, event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    if (has_row) {
        print_core_error("link-add", (CoreResult){ CORE_ERR_FORMAT, "apply returned unexpected row" });
        goto cleanup;
    }

    (void)snprintf(detail,
                   sizeof(detail),
                   "link_id=%lld from=%lld to=%lld kind=%s",
                   (long long)link_id,
                   (long long)from_item_id,
                   (long long)to_item_id,
                   kind);
    result = append_audit_entry(&db,
                                session_id,
                                "link-add",
                                "ok",
                                from_item_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                detail);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }

    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) {
        print_core_error("link-add", result);
        goto cleanup;
    }
    tx_started = 0;

    printf("link added id=%lld from=%lld to=%lld kind=%s\n",
           (long long)link_id,
           (long long)from_item_id,
           (long long)to_item_id,
           kind);
    exit_code = 0;

cleanup:
    core_free(event_payload);
    (void)core_memdb_stmt_finalize(&stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_link_list(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *item_text = find_flag_value(argc, argv, "--item-id");
    int64_t item_id = 0;
    int has_row = 0;
    int item_exists = 0;
    int row_count = 0;
    int exit_code = 1;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path || !item_text || !parse_i64_arg(item_text, &item_id)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = item_exists_active(&db, item_id, &item_exists);
    if (result.code != CORE_OK) {
        print_core_error("link-list", result);
        goto cleanup;
    }
    if (!item_exists) {
        fprintf(stderr, "link-list: item %lld not found\n", (long long)item_id);
        goto cleanup;
    }

    result = core_memdb_prepare(&db,
                                "SELECT id, from_item_id, to_item_id, kind, "
                                "CASE WHEN weight IS NULL THEN '' ELSE CAST(weight AS TEXT) END, "
                                "COALESCE(note, '') "
                                "FROM mem_link "
                                "WHERE from_item_id = ?1 OR to_item_id = ?1 "
                                "ORDER BY id ASC;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("link-list", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code != CORE_OK) {
        print_core_error("link-list", result);
        goto cleanup;
    }

    for (;;) {
        int64_t link_id = 0;
        int64_t from_item_id = 0;
        int64_t to_item_id = 0;
        CoreStr kind = {0};
        CoreStr weight = {0};
        CoreStr note = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &link_id);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 1, &from_item_id);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 2, &to_item_id);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 3, &kind);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 4, &weight);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 5, &note);
        if (result.code != CORE_OK) {
            print_core_error("link-list", result);
            goto cleanup;
        }

        printf("%lld | %lld -> %lld | kind=",
               (long long)link_id,
               (long long)from_item_id,
               (long long)to_item_id);
        print_str(kind);
        if (weight.data && weight.len > 0u) {
            printf(" | weight=");
            print_str(weight);
        }
        if (note.data && note.len > 0u) {
            printf(" | note=");
            print_str(note);
        }
        printf("\n");
        row_count += 1;
    }

    if (row_count == 0) {
        printf("no links for item_id=%lld\n", (long long)item_id);
    }
    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_link_update(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *id_text = find_flag_value(argc, argv, "--id");
    const char *kind = find_flag_value(argc, argv, "--kind");
    const char *weight_text = find_flag_value(argc, argv, "--weight");
    const char *note = find_flag_value(argc, argv, "--note");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    int64_t link_id = 0;
    int64_t from_item_id = 0;
    int64_t to_item_id = 0;
    int64_t changed_rows = 0;
    double weight_value = 0.0;
    int has_weight = 0;
    int has_row = 0;
    int tx_started = 0;
    int exit_code = 1;
    char stable_id[128];
    char workspace_key[128];
    char project_key[128];
    char item_kind[128];
    char detail[192];
    char *event_payload = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path || !id_text || !kind || !parse_i64_arg(id_text, &link_id)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!is_allowed_link_kind(kind)) {
        fprintf(stderr, "link-update: invalid --kind value '%s'\n", kind);
        return 1;
    }
    if (weight_text) {
        if (!parse_f64_arg(weight_text, &weight_value)) {
            fprintf(stderr, "link-update: invalid --weight value\n");
            return 1;
        }
        has_weight = 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_tx_begin(&db);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    tx_started = 1;

    result = fetch_link_endpoints(&db, link_id, &from_item_id, &to_item_id);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    result = fetch_item_audit_metadata(&db,
                                       from_item_id,
                                       stable_id,
                                       sizeof(stable_id),
                                       workspace_key,
                                       sizeof(workspace_key),
                                       project_key,
                                       sizeof(project_key),
                                       item_kind,
                                       sizeof(item_kind));
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    result = build_link_update_event_payload_alloc(&db,
                                                   link_id,
                                                   kind,
                                                   has_weight,
                                                   weight_value,
                                                   note != 0 ? 1 : 0,
                                                   note,
                                                   &event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    result = append_event_entry(&db,
                                session_id,
                                "EdgeUpdated",
                                from_item_id,
                                link_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&db, kApplyLinkEventSql, &stmt);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 2, event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    if (has_row) {
        print_core_error("link-update", (CoreResult){ CORE_ERR_FORMAT, "apply returned unexpected row" });
        goto cleanup;
    }
    result = fetch_changes(&db, &changed_rows);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    if (changed_rows == 0) {
        fprintf(stderr, "link-update: id %lld not found\n", (long long)link_id);
        goto cleanup;
    }
    (void)snprintf(detail,
                   sizeof(detail),
                   "link_id=%lld from=%lld to=%lld kind=%s",
                   (long long)link_id,
                   (long long)from_item_id,
                   (long long)to_item_id,
                   kind);
    result = append_audit_entry(&db,
                                session_id,
                                "link-update",
                                "ok",
                                from_item_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                detail);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }

    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) {
        print_core_error("link-update", result);
        goto cleanup;
    }
    tx_started = 0;

    printf("link updated id=%lld\n", (long long)link_id);
    exit_code = 0;

cleanup:
    core_free(event_payload);
    (void)core_memdb_stmt_finalize(&stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_link_remove(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *id_text = find_flag_value(argc, argv, "--id");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    int64_t link_id = 0;
    int64_t from_item_id = 0;
    int64_t to_item_id = 0;
    int64_t changed_rows = 0;
    int has_row = 0;
    int tx_started = 0;
    int exit_code = 1;
    char stable_id[128];
    char workspace_key[128];
    char project_key[128];
    char item_kind[128];
    char detail[192];
    char *event_payload = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path || !id_text || !parse_i64_arg(id_text, &link_id)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }
    result = core_memdb_tx_begin(&db);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    tx_started = 1;

    result = fetch_link_endpoints(&db, link_id, &from_item_id, &to_item_id);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    result = fetch_item_audit_metadata(&db,
                                       from_item_id,
                                       stable_id,
                                       sizeof(stable_id),
                                       workspace_key,
                                       sizeof(workspace_key),
                                       project_key,
                                       sizeof(project_key),
                                       item_kind,
                                       sizeof(item_kind));
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }

    result = fetch_link_snapshot_json_alloc(&db, link_id, &event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    result = append_event_entry(&db,
                                session_id,
                                "EdgeRemoved",
                                from_item_id,
                                link_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                event_payload);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&db, "DELETE FROM mem_link WHERE id = ?1;", &stmt);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    if (has_row) {
        print_core_error("link-remove", (CoreResult){ CORE_ERR_FORMAT, "delete returned unexpected row" });
        goto cleanup;
    }

    result = fetch_changes(&db, &changed_rows);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    if (changed_rows == 0) {
        fprintf(stderr, "link-remove: id %lld not found\n", (long long)link_id);
        goto cleanup;
    }
    (void)snprintf(detail,
                   sizeof(detail),
                   "link_id=%lld from=%lld to=%lld",
                   (long long)link_id,
                   (long long)from_item_id,
                   (long long)to_item_id);
    result = append_audit_entry(&db,
                                session_id,
                                "link-remove",
                                "ok",
                                from_item_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                detail);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }

    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) {
        print_core_error("link-remove", result);
        goto cleanup;
    }
    tx_started = 0;

    printf("link removed id=%lld\n", (long long)link_id);
    exit_code = 0;

cleanup:
    core_free(event_payload);
    (void)core_memdb_stmt_finalize(&stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    (void)core_memdb_close(&db);
    return exit_code;
}
