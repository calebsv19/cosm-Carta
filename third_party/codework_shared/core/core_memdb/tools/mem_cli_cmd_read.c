#include "mem_cli_cmd_read.h"

#include <stdio.h>

#include "core_memdb.h"
#include "mem_cli_args.h"
#include "mem_cli_db_helpers.h"
#include "mem_cli_output.h"

int cmd_health(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int tables_ok = 1;
    int indexes_ok = 1;
    int fts_ok = 1;
    int integrity_ok = 0;
    int overall_ok = 0;
    int i;
    int64_t schema_version = 0;
    int64_t fts_row_count = 0;
    CoreStr integrity_text = {0};
    static const char *kRequiredTables[] = {
        "mem_meta",
        "mem_item",
        "mem_tag",
        "mem_item_tag",
        "mem_link",
        "mem_item_fts",
        "mem_audit",
        "mem_event"
    };
    static const char *kRequiredIndexes[] = {
        "mem_item_fingerprint_idx",
        "mem_item_scope_idx",
        "mem_audit_session_idx",
        "mem_event_event_id_idx",
        "mem_event_ts_idx",
        "mem_event_type_idx",
        "mem_event_session_idx",
        "mem_link_from_idx",
        "mem_link_to_idx",
        "mem_link_kind_idx",
        "mem_link_unique_kind_edge"
    };

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format) || format == MEM_CLI_OUTPUT_TSV) {
        fprintf(stderr, "health: --format must be text or json\n");
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = query_single_i64(&db,
                              "SELECT CAST(value AS INTEGER) "
                              "FROM mem_meta WHERE key = 'schema_version';",
                              &schema_version);
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }
    result = query_single_i64(&db, "SELECT COUNT(*) FROM mem_item_fts;", &fts_row_count);
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }

    for (i = 0; i < (int)(sizeof(kRequiredTables) / sizeof(kRequiredTables[0])); ++i) {
        int exists = 0;
        result = sqlite_master_object_exists(&db, "table", kRequiredTables[i], &exists);
        if (result.code != CORE_OK) {
            print_core_error("health", result);
            goto cleanup;
        }
        if (!exists) {
            tables_ok = 0;
        }
    }
    for (i = 0; i < (int)(sizeof(kRequiredIndexes) / sizeof(kRequiredIndexes[0])); ++i) {
        int exists = 0;
        result = sqlite_master_object_exists(&db, "index", kRequiredIndexes[i], &exists);
        if (result.code != CORE_OK) {
            print_core_error("health", result);
            goto cleanup;
        }
        if (!exists) {
            indexes_ok = 0;
        }
    }

    result = core_memdb_prepare(&db, "PRAGMA integrity_check;", &stmt);
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }
    if (!has_row) {
        print_core_error("health", (CoreResult){ CORE_ERR_FORMAT, "integrity check returned no row" });
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 0, &integrity_text);
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }
    integrity_ok = core_str_eq(integrity_text, core_str_from_cstr("ok")) ? 1 : 0;
    result = core_memdb_stmt_finalize(&stmt);
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }

    fts_ok = fts_row_count >= 0 ? 1 : 0;
    overall_ok = (schema_version >= 6 && tables_ok && indexes_ok && fts_ok && integrity_ok) ? 1 : 0;

    if (format == MEM_CLI_OUTPUT_JSON) {
        printf("{\"ok\":%d,\"schema_version\":%lld,\"tables_ok\":%d,\"indexes_ok\":%d,\"fts_ok\":%d,\"integrity\":\"",
               overall_ok,
               (long long)schema_version,
               tables_ok,
               indexes_ok,
               fts_ok);
        print_str(integrity_text);
        printf("\"}\n");
    } else {
        printf("health: %s\n", overall_ok ? "ok" : "fail");
        printf("schema_version: %lld\n", (long long)schema_version);
        printf("tables_ok: %d\n", tables_ok);
        printf("indexes_ok: %d\n", indexes_ok);
        printf("fts_ok: %d\n", fts_ok);
        printf("integrity: ");
        print_str(integrity_text);
        printf("\n");
    }

    result = append_audit_entry(&db,
                                0,
                                "health",
                                overall_ok ? "ok" : "fail",
                                0,
                                0,
                                0,
                                0,
                                0,
                                "health check executed");
    if (result.code != CORE_OK) {
        print_core_error("health", result);
        goto cleanup;
    }

    if (!overall_ok) {
        goto cleanup;
    }

    (void)core_memdb_close(&db);
    return 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return 1;
}

int cmd_audit_list(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    const char *limit_text = find_flag_value(argc, argv, "--limit");
    int64_t limit_value = 64;
    int has_row = 0;
    int row_count = 0;
    int exit_code = 1;
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (limit_text) {
        if (!parse_i64_arg(limit_text, &limit_value) || limit_value <= 0 || limit_value > 1000) {
            fprintf(stderr, "audit-list: --limit must be in range [1, 1000]\n");
            return 1;
        }
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_prepare(&db,
                                "SELECT id, "
                                "ts_ns, "
                                "COALESCE(session_id, ''), "
                                "action, "
                                "status, "
                                "CASE WHEN item_id IS NULL THEN '' ELSE CAST(item_id AS TEXT) END, "
                                "COALESCE(stable_id, ''), "
                                "workspace_key, "
                                "project_key, "
                                "kind, "
                                "detail "
                                "FROM mem_audit "
                                "WHERE (?1 IS NULL OR session_id = ?1) "
                                "ORDER BY id ASC "
                                "LIMIT ?2;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("audit-list", result);
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 1, session_id);
    if (result.code != CORE_OK) {
        print_core_error("audit-list", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 2, limit_value);
    if (result.code != CORE_OK) {
        print_core_error("audit-list", result);
        goto cleanup;
    }

    for (;;) {
        int64_t id_value = 0;
        int64_t ts_ns = 0;
        CoreStr session_text = {0};
        CoreStr action_text = {0};
        CoreStr status_text = {0};
        CoreStr item_id_text = {0};
        CoreStr stable_id_text = {0};
        CoreStr workspace_text = {0};
        CoreStr project_text = {0};
        CoreStr kind_text = {0};
        CoreStr detail_text = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &id_value);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 1, &ts_ns);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 2, &session_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 3, &action_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 4, &status_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 5, &item_id_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 6, &stable_id_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 7, &workspace_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 8, &project_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 9, &kind_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 10, &detail_text);
        if (result.code != CORE_OK) {
            print_core_error("audit-list", result);
            goto cleanup;
        }

        if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t%lld\t",
                   (long long)id_value,
                   (long long)ts_ns);
            print_tsv_sanitized_str(session_text);
            printf("\t");
            print_tsv_sanitized_str(action_text);
            printf("\t");
            print_tsv_sanitized_str(status_text);
            printf("\t");
            print_tsv_sanitized_str(item_id_text);
            printf("\t");
            print_tsv_sanitized_str(stable_id_text);
            printf("\t");
            print_tsv_sanitized_str(workspace_text);
            printf("\t");
            print_tsv_sanitized_str(project_text);
            printf("\t");
            print_tsv_sanitized_str(kind_text);
            printf("\t");
            print_tsv_sanitized_str(detail_text);
            printf("\n");
        } else if (format == MEM_CLI_OUTPUT_JSON) {
            if (row_count == 0) {
                printf("[");
            } else {
                printf(",");
            }
            printf("{\"id\":%lld,\"ts_ns\":%lld,\"session_id\":",
                   (long long)id_value,
                   (long long)ts_ns);
            print_json_escaped_str(session_text);
            printf(",\"action\":");
            print_json_escaped_str(action_text);
            printf(",\"status\":");
            print_json_escaped_str(status_text);
            printf(",\"item_id\":");
            print_json_escaped_str(item_id_text);
            printf(",\"stable_id\":");
            print_json_escaped_str(stable_id_text);
            printf(",\"workspace\":");
            print_json_escaped_str(workspace_text);
            printf(",\"project\":");
            print_json_escaped_str(project_text);
            printf(",\"kind\":");
            print_json_escaped_str(kind_text);
            printf(",\"detail\":");
            print_json_escaped_str(detail_text);
            printf("}");
        } else {
            printf("%lld | ts_ns=%lld | session=",
                   (long long)id_value,
                   (long long)ts_ns);
            print_str(session_text);
            printf(" | action=");
            print_str(action_text);
            printf(" | status=");
            print_str(status_text);
            printf(" | item_id=");
            print_str(item_id_text);
            printf(" | stable_id=");
            print_str(stable_id_text);
            printf(" | scope=");
            print_str(workspace_text);
            printf("/");
            print_str(project_text);
            printf("/");
            print_str(kind_text);
            printf(" | detail=");
            print_str(detail_text);
            printf("\n");
        }
        row_count += 1;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        if (row_count == 0) {
            printf("[]\n");
        } else {
            printf("]\n");
        }
    }
    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_event_list(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    const char *event_type_filter = find_flag_value(argc, argv, "--event-type");
    const char *limit_text = find_flag_value(argc, argv, "--limit");
    int64_t limit_value = 64;
    int has_row = 0;
    int row_count = 0;
    int exit_code = 1;
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (limit_text) {
        if (!parse_i64_arg(limit_text, &limit_value) || limit_value <= 0 || limit_value > 1000) {
            fprintf(stderr, "event-list: --limit must be in range [1, 1000]\n");
            return 1;
        }
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_prepare(&db,
                                "SELECT id, "
                                "ts_ns, "
                                "COALESCE(session_id, ''), "
                                "event_id, "
                                "event_type, "
                                "actor_type, "
                                "actor_id, "
                                "CASE WHEN item_id IS NULL THEN '' ELSE CAST(item_id AS TEXT) END, "
                                "CASE WHEN link_id IS NULL THEN '' ELSE CAST(link_id AS TEXT) END, "
                                "COALESCE(stable_id, ''), "
                                "workspace_key, "
                                "project_key, "
                                "kind, "
                                "payload_json "
                                "FROM mem_event "
                                "WHERE (?1 IS NULL OR session_id = ?1) "
                                "AND (?2 IS NULL OR event_type = ?2) "
                                "ORDER BY id ASC "
                                "LIMIT ?3;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-list", result);
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 1, session_id);
    if (result.code != CORE_OK) {
        print_core_error("event-list", result);
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 2, event_type_filter);
    if (result.code != CORE_OK) {
        print_core_error("event-list", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 3, limit_value);
    if (result.code != CORE_OK) {
        print_core_error("event-list", result);
        goto cleanup;
    }

    for (;;) {
        int64_t id_value = 0;
        int64_t ts_ns = 0;
        CoreStr session_text = {0};
        CoreStr event_id = {0};
        CoreStr event_type = {0};
        CoreStr actor_type = {0};
        CoreStr actor_id = {0};
        CoreStr item_id_text = {0};
        CoreStr link_id_text = {0};
        CoreStr stable_id_text = {0};
        CoreStr workspace_text = {0};
        CoreStr project_text = {0};
        CoreStr kind_text = {0};
        CoreStr payload_text = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &id_value);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 1, &ts_ns);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 2, &session_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 3, &event_id);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 4, &event_type);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 5, &actor_type);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 6, &actor_id);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 7, &item_id_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 8, &link_id_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 9, &stable_id_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 10, &workspace_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 11, &project_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 12, &kind_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 13, &payload_text);
        if (result.code != CORE_OK) {
            print_core_error("event-list", result);
            goto cleanup;
        }

        if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t%lld\t",
                   (long long)id_value,
                   (long long)ts_ns);
            print_tsv_sanitized_str(session_text);
            printf("\t");
            print_tsv_sanitized_str(event_id);
            printf("\t");
            print_tsv_sanitized_str(event_type);
            printf("\t");
            print_tsv_sanitized_str(actor_type);
            printf("\t");
            print_tsv_sanitized_str(actor_id);
            printf("\t");
            print_tsv_sanitized_str(item_id_text);
            printf("\t");
            print_tsv_sanitized_str(link_id_text);
            printf("\t");
            print_tsv_sanitized_str(stable_id_text);
            printf("\t");
            print_tsv_sanitized_str(workspace_text);
            printf("\t");
            print_tsv_sanitized_str(project_text);
            printf("\t");
            print_tsv_sanitized_str(kind_text);
            printf("\t");
            print_tsv_sanitized_str(payload_text);
            printf("\n");
        } else if (format == MEM_CLI_OUTPUT_JSON) {
            if (row_count == 0) {
                printf("[");
            } else {
                printf(",");
            }
            printf("{\"id\":%lld,\"ts_ns\":%lld,\"session_id\":",
                   (long long)id_value,
                   (long long)ts_ns);
            print_json_escaped_str(session_text);
            printf(",\"event_id\":");
            print_json_escaped_str(event_id);
            printf(",\"event_type\":");
            print_json_escaped_str(event_type);
            printf(",\"actor_type\":");
            print_json_escaped_str(actor_type);
            printf(",\"actor_id\":");
            print_json_escaped_str(actor_id);
            printf(",\"item_id\":");
            print_json_escaped_str(item_id_text);
            printf(",\"link_id\":");
            print_json_escaped_str(link_id_text);
            printf(",\"stable_id\":");
            print_json_escaped_str(stable_id_text);
            printf(",\"workspace\":");
            print_json_escaped_str(workspace_text);
            printf(",\"project\":");
            print_json_escaped_str(project_text);
            printf(",\"kind\":");
            print_json_escaped_str(kind_text);
            printf(",\"payload\":");
            print_json_escaped_str(payload_text);
            printf("}");
        } else {
            printf("%lld | ts_ns=%lld | type=",
                   (long long)id_value,
                   (long long)ts_ns);
            print_str(event_type);
            printf(" | event_id=");
            print_str(event_id);
            printf(" | actor=");
            print_str(actor_type);
            printf("/");
            print_str(actor_id);
            printf(" | session=");
            print_str(session_text);
            printf(" | item_id=");
            print_str(item_id_text);
            printf(" | link_id=");
            print_str(link_id_text);
            printf(" | stable_id=");
            print_str(stable_id_text);
            printf(" | scope=");
            print_str(workspace_text);
            printf("/");
            print_str(project_text);
            printf("/");
            print_str(kind_text);
            printf(" | payload=");
            print_str(payload_text);
            printf("\n");
        }
        row_count += 1;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        if (row_count == 0) {
            printf("[]\n");
        } else {
            printf("]\n");
        }
    }
    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_list(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int exit_code = 1;
    int row_count = 0;

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_prepare(&db,
                                "SELECT id, title, COALESCE(stable_id, '') "
                                "FROM mem_item "
                                "WHERE archived_ns IS NULL "
                                "ORDER BY pinned DESC, updated_ns DESC, id ASC;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("list", result);
        goto cleanup;
    }

    for (;;) {
        int64_t id_value = 0;
        CoreStr title = {0};
        CoreStr stable_id = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("list", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &id_value);
        if (result.code != CORE_OK) {
            print_core_error("list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 1, &title);
        if (result.code != CORE_OK) {
            print_core_error("list", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 2, &stable_id);
        if (result.code != CORE_OK) {
            print_core_error("list", result);
            goto cleanup;
        }

        if (format == MEM_CLI_OUTPUT_TEXT) {
            printf("%lld | ", (long long)id_value);
            print_str(title);
            if (stable_id.data && stable_id.len > 0u) {
                printf(" | stable_id=");
                print_str(stable_id);
            }
            printf("\n");
        } else if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t", (long long)id_value);
            print_tsv_sanitized_str(title);
            printf("\t");
            print_tsv_sanitized_str(stable_id);
            printf("\n");
        } else {
            if (row_count == 0) {
                printf("[");
            } else {
                printf(",");
            }
            printf("{\"id\":%lld,\"title\":", (long long)id_value);
            print_json_escaped_str(title);
            printf(",\"stable_id\":");
            print_json_escaped_str(stable_id);
            printf("}");
        }
        row_count += 1;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        if (row_count == 0) {
            printf("[]\n");
        } else {
            printf("]\n");
        }
    }

    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_find(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *query = find_flag_value(argc, argv, "--query");
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int exit_code = 1;
    int row_count = 0;

    if (!db_path || !query) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_prepare(&db,
                                "SELECT mem_item.id, mem_item.title "
                                "FROM mem_item "
                                "JOIN mem_item_fts ON mem_item_fts.rowid = mem_item.id "
                                "WHERE mem_item.archived_ns IS NULL AND mem_item_fts MATCH ?1 "
                                "ORDER BY mem_item.id ASC;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("find", result);
        goto cleanup;
    }

    result = core_memdb_stmt_bind_text(&stmt, 1, query);
    if (result.code != CORE_OK) {
        print_core_error("find", result);
        goto cleanup;
    }

    for (;;) {
        int64_t id_value = 0;
        CoreStr title = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("find", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &id_value);
        if (result.code != CORE_OK) {
            print_core_error("find", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 1, &title);
        if (result.code != CORE_OK) {
            print_core_error("find", result);
            goto cleanup;
        }

        if (format == MEM_CLI_OUTPUT_TEXT) {
            printf("%lld | ", (long long)id_value);
            print_str(title);
            printf("\n");
        } else if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t", (long long)id_value);
            print_tsv_sanitized_str(title);
            printf("\n");
        } else {
            if (row_count == 0) {
                printf("[");
            } else {
                printf(",");
            }
            printf("{\"id\":%lld,\"title\":", (long long)id_value);
            print_json_escaped_str(title);
            printf("}");
        }
        row_count += 1;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        if (row_count == 0) {
            printf("[]\n");
        } else {
            printf("]\n");
        }
    }

    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_query(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *query = find_flag_value(argc, argv, "--query");
    const char *limit_text = find_flag_value(argc, argv, "--limit");
    const char *offset_text = find_flag_value(argc, argv, "--offset");
    const char *workspace_filter = find_flag_value(argc, argv, "--workspace");
    const char *project_filter = find_flag_value(argc, argv, "--project");
    const char *kind_filter = find_flag_value(argc, argv, "--kind");
    int pinned_only = has_flag(argc, argv, "--pinned-only");
    int canonical_only = has_flag(argc, argv, "--canonical-only");
    int include_archived = has_flag(argc, argv, "--include-archived");
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    int use_fts = query && query[0] != '\0';
    int64_t limit_value = 32;
    int64_t offset_value = 0;
    int has_row = 0;
    int bind_index = 1;
    int exit_code = 1;
    int row_count = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;
    char sql[1536];

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }

    if (limit_text) {
        if (!parse_i64_arg(limit_text, &limit_value) || limit_value <= 0 || limit_value > 1000) {
            fprintf(stderr, "query: --limit must be in range [1, 1000]\n");
            return 1;
        }
    }
    if (offset_text) {
        if (!parse_i64_arg(offset_text, &offset_value) || offset_value < 0 || offset_value > 1000000) {
            fprintf(stderr, "query: --offset must be in range [0, 1000000]\n");
            return 1;
        }
    }

    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    sql[0] = '\0';
    if (use_fts) {
        if (!append_sql_fragment(
                sql,
                sizeof(sql),
                "SELECT mem_item.id, mem_item.title, COALESCE(mem_item.stable_id, ''), "
                "mem_item.pinned, mem_item.canonical, mem_item.updated_ns "
                "FROM mem_item "
                "JOIN mem_item_fts ON mem_item_fts.rowid = mem_item.id "
                "WHERE 1 = 1")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    } else {
        if (!append_sql_fragment(
                sql,
                sizeof(sql),
                "SELECT mem_item.id, mem_item.title, COALESCE(mem_item.stable_id, ''), "
                "mem_item.pinned, mem_item.canonical, mem_item.updated_ns "
                "FROM mem_item "
                "WHERE 1 = 1")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }

    if (!include_archived) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item.archived_ns IS NULL")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (pinned_only) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item.pinned = 1")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (canonical_only) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item.canonical = 1")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (use_fts) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item_fts MATCH ?")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (workspace_filter) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item.workspace_key = ?")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (project_filter) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item.project_key = ?")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (kind_filter) {
        if (!append_sql_fragment(sql, sizeof(sql), " AND mem_item.kind = ?")) {
            fprintf(stderr, "query: sql buffer overflow\n");
            goto cleanup;
        }
    }
    if (!append_sql_fragment(
            sql,
            sizeof(sql),
            " ORDER BY mem_item.canonical DESC, mem_item.pinned DESC, mem_item.updated_ns DESC, mem_item.id ASC")) {
        fprintf(stderr, "query: sql buffer overflow\n");
        goto cleanup;
    }
    if (!append_sql_fragment(sql, sizeof(sql), " LIMIT ? OFFSET ?")) {
        fprintf(stderr, "query: sql buffer overflow\n");
        goto cleanup;
    }

    result = core_memdb_prepare(&db, sql, &stmt);
    if (result.code != CORE_OK) {
        print_core_error("query", result);
        goto cleanup;
    }

    bind_index = 1;
    if (use_fts) {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, query);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    if (workspace_filter) {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, workspace_filter);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    if (project_filter) {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, project_filter);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    if (kind_filter) {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, kind_filter);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    result = core_memdb_stmt_bind_i64(&stmt, bind_index, limit_value);
    if (result.code != CORE_OK) {
        print_core_error("query", result);
        goto cleanup;
    }
    bind_index += 1;
    result = core_memdb_stmt_bind_i64(&stmt, bind_index, offset_value);
    if (result.code != CORE_OK) {
        print_core_error("query", result);
        goto cleanup;
    }

    for (;;) {
        int64_t id_value = 0;
        int64_t pinned_value = 0;
        int64_t canonical_value = 0;
        int64_t updated_ns = 0;
        CoreStr title = {0};
        CoreStr stable_id = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &id_value);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 1, &title);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 2, &stable_id);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 3, &pinned_value);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 4, &canonical_value);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 5, &updated_ns);
        if (result.code != CORE_OK) {
            print_core_error("query", result);
            goto cleanup;
        }

        if (format == MEM_CLI_OUTPUT_TEXT) {
            printf("%lld | pinned=%lld | canonical=%lld | updated_ns=%lld | ",
                   (long long)id_value,
                   (long long)pinned_value,
                   (long long)canonical_value,
                   (long long)updated_ns);
            print_str(title);
            if (stable_id.data && stable_id.len > 0u) {
                printf(" | stable_id=");
                print_str(stable_id);
            }
            printf("\n");
        } else if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t%lld\t%lld\t%lld\t",
                   (long long)id_value,
                   (long long)pinned_value,
                   (long long)canonical_value,
                   (long long)updated_ns);
            print_tsv_sanitized_str(title);
            printf("\t");
            print_tsv_sanitized_str(stable_id);
            printf("\n");
        } else {
            if (row_count == 0) {
                printf("[");
            } else {
                printf(",");
            }
            printf("{\"id\":%lld,\"pinned\":%lld,\"canonical\":%lld,\"updated_ns\":%lld,\"title\":",
                   (long long)id_value,
                   (long long)pinned_value,
                   (long long)canonical_value,
                   (long long)updated_ns);
            print_json_escaped_str(title);
            printf(",\"stable_id\":");
            print_json_escaped_str(stable_id);
            printf("}");
        }
        row_count += 1;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        if (row_count == 0) {
            printf("[]\n");
        } else {
            printf("]\n");
        }
    }

    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_show(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *id_text = find_flag_value(argc, argv, "--id");
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    int64_t item_id = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int exit_code = 1;

    if (!db_path || !id_text || !parse_i64_arg(id_text, &item_id)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_prepare(&db,
                                "SELECT id, "
                                "COALESCE(stable_id, ''), "
                                "title, "
                                "body, "
                                "fingerprint, "
                                "created_ns, "
                                "updated_ns, "
                                "pinned, "
                                "canonical, "
                                "CASE WHEN ttl_until_ns IS NULL THEN '' ELSE CAST(ttl_until_ns AS TEXT) END, "
                                "CASE WHEN archived_ns IS NULL THEN '' ELSE CAST(archived_ns AS TEXT) END "
                                "FROM mem_item WHERE id = ?1;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("show", result);
        goto cleanup;
    }

    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code != CORE_OK) {
        print_core_error("show", result);
        goto cleanup;
    }

    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("show", result);
        goto cleanup;
    }
    if (!has_row) {
        fprintf(stderr, "show: id %lld not found\n", (long long)item_id);
        goto cleanup;
    }

    {
        int64_t id_value = 0;
        int64_t created_ns = 0;
        int64_t updated_ns = 0;
        int64_t pinned = 0;
        int64_t canonical = 0;
        CoreStr stable_id = {0};
        CoreStr title = {0};
        CoreStr body = {0};
        CoreStr fingerprint = {0};
        CoreStr ttl_text = {0};
        CoreStr archived_text = {0};

        result = core_memdb_stmt_column_i64(&stmt, 0, &id_value);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 1, &stable_id);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 2, &title);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 3, &body);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 4, &fingerprint);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 5, &created_ns);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 6, &updated_ns);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 7, &pinned);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 8, &canonical);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 9, &ttl_text);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 10, &archived_text);
        if (result.code != CORE_OK) {
            print_core_error("show", result);
            goto cleanup;
        }

        if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t", (long long)id_value);
            print_tsv_sanitized_str(stable_id);
            printf("\t");
            print_tsv_sanitized_str(title);
            printf("\t");
            print_tsv_sanitized_str(body);
            printf("\t");
            print_tsv_sanitized_str(fingerprint);
            printf("\t%lld\t%lld\t%lld\t%lld\t",
                   (long long)created_ns,
                   (long long)updated_ns,
                   (long long)pinned,
                   (long long)canonical);
            print_tsv_sanitized_str(ttl_text);
            printf("\t");
            print_tsv_sanitized_str(archived_text);
            printf("\n");
            exit_code = 0;
            goto cleanup;
        }
        if (format == MEM_CLI_OUTPUT_JSON) {
            printf("{\"id\":%lld,", (long long)id_value);
            printf("\"stable_id\":");
            print_json_escaped_str(stable_id);
            printf(",\"title\":");
            print_json_escaped_str(title);
            printf(",\"body\":");
            print_json_escaped_str(body);
            printf(",\"fingerprint\":");
            print_json_escaped_str(fingerprint);
            printf(",\"created_ns\":%lld,", (long long)created_ns);
            printf("\"updated_ns\":%lld,", (long long)updated_ns);
            printf("\"pinned\":%lld,", (long long)pinned);
            printf("\"canonical\":%lld,", (long long)canonical);
            printf("\"ttl_until_ns\":");
            print_json_escaped_str(ttl_text);
            printf(",\"archived_ns\":");
            print_json_escaped_str(archived_text);
            printf("}\n");
            exit_code = 0;
            goto cleanup;
        }

        printf("id: %lld\n", (long long)id_value);
        printf("stable_id: ");
        print_str(stable_id);
        printf("\n");
        printf("title: ");
        print_str(title);
        printf("\n");
        printf("body: ");
        print_str(body);
        printf("\n");
        printf("fingerprint: ");
        print_str(fingerprint);
        printf("\n");
        printf("created_ns: %lld\n", (long long)created_ns);
        printf("updated_ns: %lld\n", (long long)updated_ns);
        printf("pinned: %lld\n", (long long)pinned);
        printf("canonical: %lld\n", (long long)canonical);
        printf("ttl_until_ns: ");
        print_str(ttl_text);
        printf("\n");
        printf("archived_ns: ");
        print_str(archived_text);
        printf("\n");
    }

    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

static int id_in_set(const int64_t *ids, int count, int64_t value) {
    int i;
    if (!ids || count <= 0) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        if (ids[i] == value) {
            return 1;
        }
    }
    return 0;
}

int cmd_neighbors(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *item_text = find_flag_value(argc, argv, "--item-id");
    const char *kind_filter = find_flag_value(argc, argv, "--kind");
    const char *max_edges_text = find_flag_value(argc, argv, "--max-edges");
    const char *max_nodes_text = find_flag_value(argc, argv, "--max-nodes");
    int64_t item_id = 0;
    int64_t max_edges = 48;
    int64_t max_nodes = 24;
    int64_t seen_neighbor_ids[1024];
    int seen_neighbor_count = 0;
    int64_t row_count = 0;
    int has_row = 0;
    int item_exists = 0;
    int exit_code = 1;
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;

    if (!db_path || !item_text || !parse_i64_arg(item_text, &item_id)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (kind_filter && !is_allowed_link_kind(kind_filter)) {
        fprintf(stderr, "neighbors: invalid --kind value '%s'\n", kind_filter);
        return 1;
    }
    if (max_edges_text) {
        if (!parse_i64_arg(max_edges_text, &max_edges) || max_edges < 1 || max_edges > 4096) {
            fprintf(stderr, "neighbors: --max-edges must be in range [1, 4096]\n");
            return 1;
        }
    }
    if (max_nodes_text) {
        if (!parse_i64_arg(max_nodes_text, &max_nodes) || max_nodes < 1 || max_nodes > 1024) {
            fprintf(stderr, "neighbors: --max-nodes must be in range [1, 1024]\n");
            return 1;
        }
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = item_exists_active(&db, item_id, &item_exists);
    if (result.code != CORE_OK) {
        print_core_error("neighbors", result);
        goto cleanup;
    }
    if (!item_exists) {
        fprintf(stderr, "neighbors: item %lld not found\n", (long long)item_id);
        goto cleanup;
    }

    result = core_memdb_prepare(&db,
                                "SELECT "
                                "l.id, "
                                "l.from_item_id, "
                                "l.to_item_id, "
                                "l.kind, "
                                "CASE WHEN l.weight IS NULL THEN '' ELSE CAST(l.weight AS TEXT) END, "
                                "COALESCE(l.note, ''), "
                                "n.id, "
                                "n.title, "
                                "n.pinned, "
                                "n.canonical, "
                                "n.workspace_key, "
                                "n.project_key, "
                                "n.kind "
                                "FROM mem_link l "
                                "JOIN mem_item n "
                                "ON n.id = CASE WHEN l.from_item_id = ?1 THEN l.to_item_id ELSE l.from_item_id END "
                                "AND n.archived_ns IS NULL "
                                "WHERE (l.from_item_id = ?1 OR l.to_item_id = ?1) "
                                "AND (?2 = '' OR l.kind = ?2) "
                                "ORDER BY l.id ASC "
                                "LIMIT ?3;",
                                &stmt);
    if (result.code != CORE_OK) {
        print_core_error("neighbors", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code != CORE_OK) {
        print_core_error("neighbors", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 2, kind_filter ? kind_filter : "");
    if (result.code != CORE_OK) {
        print_core_error("neighbors", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 3, max_edges);
    if (result.code != CORE_OK) {
        print_core_error("neighbors", result);
        goto cleanup;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        printf("[");
    }

    for (;;) {
        int64_t link_id = 0;
        int64_t from_item_id = 0;
        int64_t to_item_id = 0;
        int64_t neighbor_id = 0;
        int64_t neighbor_pinned = 0;
        int64_t neighbor_canonical = 0;
        CoreStr link_kind = {0};
        CoreStr link_weight = {0};
        CoreStr link_note = {0};
        CoreStr neighbor_title = {0};
        CoreStr neighbor_workspace = {0};
        CoreStr neighbor_project = {0};
        CoreStr neighbor_kind = {0};

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &link_id);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 1, &from_item_id);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 2, &to_item_id);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 3, &link_kind);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 4, &link_weight);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 5, &link_note);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 6, &neighbor_id);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 7, &neighbor_title);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 8, &neighbor_pinned);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&stmt, 9, &neighbor_canonical);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 10, &neighbor_workspace);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 11, &neighbor_project);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 12, &neighbor_kind);
        if (result.code != CORE_OK) {
            print_core_error("neighbors", result);
            goto cleanup;
        }

        if (!id_in_set(seen_neighbor_ids, seen_neighbor_count, neighbor_id)) {
            if ((int64_t)seen_neighbor_count >= max_nodes) {
                continue;
            }
            seen_neighbor_ids[seen_neighbor_count++] = neighbor_id;
        }

        if (format == MEM_CLI_OUTPUT_JSON) {
            if (row_count > 0) {
                printf(",");
            }
            printf("{\"link_id\":%lld,\"from_item_id\":%lld,\"to_item_id\":%lld,\"kind\":",
                   (long long)link_id,
                   (long long)from_item_id,
                   (long long)to_item_id);
            print_json_escaped_str(link_kind);
            printf(",\"weight\":");
            print_json_escaped_str(link_weight);
            printf(",\"note\":");
            print_json_escaped_str(link_note);
            printf(",\"neighbor_id\":%lld,\"neighbor_title\":",
                   (long long)neighbor_id);
            print_json_escaped_str(neighbor_title);
            printf(",\"neighbor_pinned\":%lld,\"neighbor_canonical\":%lld,\"neighbor_workspace\":",
                   (long long)neighbor_pinned,
                   (long long)neighbor_canonical);
            print_json_escaped_str(neighbor_workspace);
            printf(",\"neighbor_project\":");
            print_json_escaped_str(neighbor_project);
            printf(",\"neighbor_kind\":");
            print_json_escaped_str(neighbor_kind);
            printf("}");
        } else if (format == MEM_CLI_OUTPUT_TSV) {
            printf("%lld\t%lld\t%lld\t",
                   (long long)link_id,
                   (long long)from_item_id,
                   (long long)to_item_id);
            print_tsv_sanitized_str(link_kind);
            printf("\t");
            print_tsv_sanitized_str(link_weight);
            printf("\t");
            print_tsv_sanitized_str(link_note);
            printf("\t%lld\t", (long long)neighbor_id);
            print_tsv_sanitized_str(neighbor_title);
            printf("\t%lld\t%lld\t",
                   (long long)neighbor_pinned,
                   (long long)neighbor_canonical);
            print_tsv_sanitized_str(neighbor_workspace);
            printf("\t");
            print_tsv_sanitized_str(neighbor_project);
            printf("\t");
            print_tsv_sanitized_str(neighbor_kind);
            printf("\n");
        } else {
            printf("%lld | %lld -> %lld | kind=",
                   (long long)link_id,
                   (long long)from_item_id,
                   (long long)to_item_id);
            print_str(link_kind);
            printf(" | neighbor=%lld | title=", (long long)neighbor_id);
            print_str(neighbor_title);
            if (neighbor_project.data && neighbor_project.len > 0u) {
                printf(" | project=");
                print_str(neighbor_project);
            }
            if (link_weight.data && link_weight.len > 0u) {
                printf(" | weight=");
                print_str(link_weight);
            }
            if (link_note.data && link_note.len > 0u) {
                printf(" | note=");
                print_str(link_note);
            }
            printf("\n");
        }

        row_count += 1;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        printf("]\n");
    } else if (row_count == 0) {
        printf("no neighbors for item_id=%lld\n", (long long)item_id);
    }

    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}
