#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_memdb.h"
#include "mem_cli_args.h"
#include "mem_cli_cmd_event.h"
#include "mem_cli_db_helpers.h"
#include "mem_cli_output.h"

int cmd_event_replay_apply(int argc, char **argv) {
    const char *source_db_path = find_flag_value(argc, argv, "--db");
    const char *target_db_path = find_flag_value(argc, argv, "--out-db");
    const char *limit_text = find_flag_value(argc, argv, "--limit-events");
    int64_t limit_events = 0;
    int64_t source_event_count = 0;
    int64_t applied_count = 0;
    int64_t skipped_count = 0;
    int64_t dependency_skip_count = 0;
    int64_t source_item_count = 0;
    int64_t target_item_count = 0;
    int64_t source_link_count = 0;
    int64_t target_link_count = 0;
    int64_t item_missing = 0;
    int64_t item_extra = 0;
    int64_t link_missing = 0;
    int64_t link_extra = 0;
    int64_t item_field_mismatch = 0;
    int64_t link_field_mismatch = 0;
    int status_ok = 0;
    int tx_started = 0;
    int source_attached = 0;
    int has_row = 0;
    int exit_code = 1;
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb target_db = {0};
    CoreMemStmt attach_stmt = {0};
    CoreMemStmt event_stmt = {0};
    CoreMemStmt apply_item_snapshot_stmt = {0};
    CoreMemStmt set_pinned_stmt = {0};
    CoreMemStmt set_canonical_stmt = {0};
    CoreMemStmt apply_link_snapshot_stmt = {0};
    CoreMemStmt remove_link_stmt = {0};
    CoreResult result;

    if (!source_db_path || !target_db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (strcmp(source_db_path, target_db_path) == 0) {
        fprintf(stderr, "event-replay-apply: --db and --out-db must be different paths\n");
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (format == MEM_CLI_OUTPUT_TSV) {
        fprintf(stderr, "event-replay-apply: --format must be text or json\n");
        return 1;
    }
    if (limit_text) {
        if (!parse_i64_arg(limit_text, &limit_events) || limit_events <= 0 || limit_events > 200000) {
            fprintf(stderr, "event-replay-apply: --limit-events must be in range [1, 200000]\n");
            return 1;
        }
    }
    if (!open_db_or_fail(target_db_path, &target_db)) {
        return 1;
    }

    result = core_memdb_prepare(&target_db, "ATTACH DATABASE ?1 AS replay_src;", &attach_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&attach_stmt, 1, source_db_path);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&attach_stmt, &has_row);
    if (result.code != CORE_OK || has_row) {
        if (result.code == CORE_OK) {
            result = (CoreResult){ CORE_ERR_FORMAT, "event-replay-apply attach returned unexpected row" };
        }
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_stmt_finalize(&attach_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    source_attached = 1;

    result = core_memdb_tx_begin(&target_db);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    tx_started = 1;

    result = core_memdb_exec(&target_db, "DELETE FROM mem_item_tag;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_exec(&target_db, "DELETE FROM mem_tag;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_exec(&target_db, "DELETE FROM mem_link;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_exec(&target_db, "DELETE FROM mem_item;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_exec(&target_db, "DELETE FROM mem_item_fts;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&target_db,
                                "INSERT INTO mem_item ("
                                "id, stable_id, title, body, fingerprint, workspace_key, project_key, kind, "
                                "created_ns, updated_ns, pinned, canonical, ttl_until_ns, archived_ns"
                                ") VALUES ("
                                "?1, "
                                "NULLIF(COALESCE(json_extract(?2, '$.stable_id'), ?3, ''), ''), "
                                "COALESCE(json_extract(?2, '$.title'), ''), "
                                "COALESCE(json_extract(?2, '$.body'), ''), "
                                "COALESCE(json_extract(?2, '$.fingerprint'), ''), "
                                "COALESCE(json_extract(?2, '$.workspace_key'), ?4, ''), "
                                "COALESCE(json_extract(?2, '$.project_key'), ?5, ''), "
                                "COALESCE(json_extract(?2, '$.kind'), ?6, ''), "
                                "COALESCE(CAST(json_extract(?2, '$.created_ns') AS INTEGER), 0), "
                                "COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), 0), "
                                "COALESCE(CAST(json_extract(?2, '$.pinned') AS INTEGER), 0), "
                                "COALESCE(CAST(json_extract(?2, '$.canonical') AS INTEGER), 0), "
                                "CASE "
                                "  WHEN json_type(?2, '$.ttl_until_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.ttl_until_ns') AS INTEGER) "
                                "  ELSE NULL "
                                "END, "
                                "CASE "
                                "  WHEN json_type(?2, '$.archived_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.archived_ns') AS INTEGER) "
                                "  ELSE NULL "
                                "END"
                                ") "
                                "ON CONFLICT(id) DO UPDATE SET "
                                "stable_id = COALESCE(NULLIF(json_extract(?2, '$.stable_id'), ''), stable_id), "
                                "title = COALESCE(json_extract(?2, '$.title'), title), "
                                "body = COALESCE(json_extract(?2, '$.body'), body), "
                                "fingerprint = COALESCE(json_extract(?2, '$.fingerprint'), fingerprint), "
                                "workspace_key = COALESCE(json_extract(?2, '$.workspace_key'), workspace_key), "
                                "project_key = COALESCE(json_extract(?2, '$.project_key'), project_key), "
                                "kind = COALESCE(json_extract(?2, '$.kind'), kind), "
                                "created_ns = COALESCE(CAST(json_extract(?2, '$.created_ns') AS INTEGER), created_ns), "
                                "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), updated_ns), "
                                "pinned = COALESCE(CAST(json_extract(?2, '$.pinned') AS INTEGER), pinned), "
                                "canonical = COALESCE(CAST(json_extract(?2, '$.canonical') AS INTEGER), canonical), "
                                "ttl_until_ns = CASE "
                                "  WHEN json_type(?2, '$.ttl_until_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.ttl_until_ns') AS INTEGER) "
                                "  ELSE ttl_until_ns "
                                "END, "
                                "archived_ns = CASE "
                                "  WHEN json_type(?2, '$.archived_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.archived_ns') AS INTEGER) "
                                "  ELSE archived_ns "
                                "END;",
                                &apply_item_snapshot_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&target_db,
                                "UPDATE mem_item "
                                "SET pinned = COALESCE(CAST(json_extract(?2, '$.value') AS INTEGER), pinned), "
                                "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), updated_ns) "
                                "WHERE id = ?1;",
                                &set_pinned_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&target_db,
                                "UPDATE mem_item "
                                "SET canonical = COALESCE(CAST(json_extract(?2, '$.value') AS INTEGER), canonical), "
                                "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), updated_ns) "
                                "WHERE id = ?1;",
                                &set_canonical_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&target_db,
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
                                "note = excluded.note;",
                                &apply_link_snapshot_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&target_db,
                                "DELETE FROM mem_link WHERE id = ?1;",
                                &remove_link_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }

    if (limit_events > 0) {
        result = core_memdb_prepare(&target_db,
                                    "SELECT event_type, "
                                    "COALESCE(item_id, 0), "
                                    "COALESCE(link_id, 0), "
                                    "COALESCE(stable_id, ''), "
                                    "workspace_key, "
                                    "project_key, "
                                    "kind, "
                                    "payload_json "
                                    "FROM replay_src.mem_event "
                                    "ORDER BY id ASC "
                                    "LIMIT ?1;",
                                    &event_stmt);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_bind_i64(&event_stmt, 1, limit_events);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
    } else {
        result = core_memdb_prepare(&target_db,
                                    "SELECT event_type, "
                                    "COALESCE(item_id, 0), "
                                    "COALESCE(link_id, 0), "
                                    "COALESCE(stable_id, ''), "
                                    "workspace_key, "
                                    "project_key, "
                                    "kind, "
                                    "payload_json "
                                    "FROM replay_src.mem_event "
                                    "ORDER BY id ASC;",
                                    &event_stmt);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
    }

    for (;;) {
        CoreStr event_type = {0};
        CoreStr stable_text = {0};
        CoreStr workspace_text = {0};
        CoreStr project_text = {0};
        CoreStr kind_text = {0};
        CoreStr payload_text = {0};
        int64_t item_id = 0;
        int64_t link_id = 0;
        int step_has_row = 0;
        int64_t changed_rows = 0;

        result = core_memdb_stmt_step(&event_stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_text(&event_stmt, 0, &event_type);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&event_stmt, 1, &item_id);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&event_stmt, 2, &link_id);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 3, &stable_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 4, &workspace_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 5, &project_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 6, &kind_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 7, &payload_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-apply", result);
            goto cleanup;
        }

        source_event_count += 1;

        if (core_str_equals_cstr(event_type, "NodeCreated") ||
            core_str_equals_cstr(event_type, "NodeBodyUpdated") ||
            core_str_equals_cstr(event_type, "NodeMerged") ||
            core_str_equals_cstr(event_type, "NodeMetadataPatched")) {
            if (item_id <= 0) {
                skipped_count += 1;
                continue;
            }
            result = core_memdb_stmt_bind_i64(&apply_item_snapshot_stmt, 1, item_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_item_snapshot_stmt, 2, core_str_data_or_empty(payload_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_item_snapshot_stmt, 3, core_str_data_or_empty(stable_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_item_snapshot_stmt, 4, core_str_data_or_empty(workspace_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_item_snapshot_stmt, 5, core_str_data_or_empty(project_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_item_snapshot_stmt, 6, core_str_data_or_empty(kind_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&apply_item_snapshot_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event-replay-apply item snapshot returned unexpected row" };
                }
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&apply_item_snapshot_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            applied_count += 1;
            continue;
        }

        if (core_str_equals_cstr(event_type, "NodePinnedSet")) {
            if (item_id <= 0) {
                skipped_count += 1;
                continue;
            }
            result = core_memdb_stmt_bind_i64(&set_pinned_stmt, 1, item_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&set_pinned_stmt, 2, core_str_data_or_empty(payload_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&set_pinned_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event-replay-apply set_pinned returned unexpected row" };
                }
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&set_pinned_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = fetch_changes(&target_db, &changed_rows);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            if (changed_rows == 0) {
                dependency_skip_count += 1;
                skipped_count += 1;
            } else {
                applied_count += 1;
            }
            continue;
        }

        if (core_str_equals_cstr(event_type, "NodeCanonicalSet")) {
            if (item_id <= 0) {
                skipped_count += 1;
                continue;
            }
            result = core_memdb_stmt_bind_i64(&set_canonical_stmt, 1, item_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&set_canonical_stmt, 2, core_str_data_or_empty(payload_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&set_canonical_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event-replay-apply set_canonical returned unexpected row" };
                }
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&set_canonical_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = fetch_changes(&target_db, &changed_rows);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            if (changed_rows == 0) {
                dependency_skip_count += 1;
                skipped_count += 1;
            } else {
                applied_count += 1;
            }
            continue;
        }

        if (core_str_equals_cstr(event_type, "EdgeAdded") ||
            core_str_equals_cstr(event_type, "EdgeUpdated")) {
            if (link_id <= 0) {
                skipped_count += 1;
                continue;
            }
            result = core_memdb_stmt_bind_i64(&apply_link_snapshot_stmt, 1, link_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_link_snapshot_stmt, 2, core_str_data_or_empty(payload_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&apply_link_snapshot_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event-replay-apply link snapshot returned unexpected row" };
                }
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&apply_link_snapshot_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            applied_count += 1;
            continue;
        }

        if (core_str_equals_cstr(event_type, "EdgeRemoved")) {
            if (link_id <= 0) {
                skipped_count += 1;
                continue;
            }
            result = core_memdb_stmt_bind_i64(&remove_link_stmt, 1, link_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&remove_link_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event-replay-apply remove_link returned unexpected row" };
                }
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&remove_link_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-apply", result);
                goto cleanup;
            }
            applied_count += 1;
            continue;
        }

        skipped_count += 1;
    }

    result = core_memdb_exec(&target_db,
                             "INSERT INTO mem_item_fts (rowid, title, body) "
                             "SELECT id, title, body "
                             "FROM mem_item;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }

    result = query_single_i64(&target_db, "SELECT COUNT(*) FROM replay_src.mem_item;", &source_item_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db, "SELECT COUNT(*) FROM mem_item;", &target_item_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db, "SELECT COUNT(*) FROM replay_src.mem_link;", &source_link_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db, "SELECT COUNT(*) FROM mem_link;", &target_link_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db,
                              "SELECT COUNT(*) "
                              "FROM replay_src.mem_item AS source "
                              "LEFT JOIN mem_item AS target ON target.id = source.id "
                              "WHERE target.id IS NULL;",
                              &item_missing);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db,
                              "SELECT COUNT(*) "
                              "FROM mem_item AS target "
                              "LEFT JOIN replay_src.mem_item AS source ON source.id = target.id "
                              "WHERE source.id IS NULL;",
                              &item_extra);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db,
                              "SELECT COUNT(*) "
                              "FROM replay_src.mem_link AS source "
                              "LEFT JOIN mem_link AS target ON target.id = source.id "
                              "WHERE target.id IS NULL;",
                              &link_missing);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db,
                              "SELECT COUNT(*) "
                              "FROM mem_link AS target "
                              "LEFT JOIN replay_src.mem_link AS source ON source.id = target.id "
                              "WHERE source.id IS NULL;",
                              &link_extra);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db,
                              "SELECT COUNT(*) "
                              "FROM replay_src.mem_item AS source "
                              "JOIN mem_item AS target ON target.id = source.id "
                              "WHERE NOT ("
                              "source.title = target.title "
                              "AND source.body = target.body "
                              "AND source.fingerprint = target.fingerprint "
                              "AND source.stable_id IS target.stable_id "
                              "AND source.workspace_key = target.workspace_key "
                              "AND source.project_key = target.project_key "
                              "AND source.kind = target.kind "
                              "AND source.created_ns = target.created_ns "
                              "AND source.updated_ns = target.updated_ns "
                              "AND source.pinned = target.pinned "
                              "AND source.canonical = target.canonical "
                              "AND source.ttl_until_ns IS target.ttl_until_ns "
                              "AND source.archived_ns IS target.archived_ns"
                              ");",
                              &item_field_mismatch);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    result = query_single_i64(&target_db,
                              "SELECT COUNT(*) "
                              "FROM replay_src.mem_link AS source "
                              "JOIN mem_link AS target ON target.id = source.id "
                              "WHERE NOT ("
                              "source.from_item_id = target.from_item_id "
                              "AND source.to_item_id = target.to_item_id "
                              "AND source.kind = target.kind "
                              "AND ("
                              "  (source.weight IS NULL AND target.weight IS NULL) "
                              "  OR (source.weight IS NOT NULL AND target.weight IS NOT NULL AND ABS(source.weight - target.weight) < 0.000001)"
                              ") "
                              "AND source.note IS target.note"
                              ");",
                              &link_field_mismatch);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }

    status_ok = (item_missing == 0 &&
                 item_extra == 0 &&
                 link_missing == 0 &&
                 link_extra == 0 &&
                 item_field_mismatch == 0 &&
                 link_field_mismatch == 0) ? 1 : 0;

    result = core_memdb_tx_commit(&target_db);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-apply", result);
        goto cleanup;
    }
    tx_started = 0;

    if (format == MEM_CLI_OUTPUT_JSON) {
        CoreStr source_db_text = { source_db_path, strlen(source_db_path) };
        CoreStr target_db_text = { target_db_path, strlen(target_db_path) };
        printf("{\"ok\":%d", status_ok);
        printf(",\"source_db\":");
        print_json_escaped_str(source_db_text);
        printf(",\"target_db\":");
        print_json_escaped_str(target_db_text);
        printf(",\"events\":%lld", (long long)source_event_count);
        printf(",\"applied\":%lld", (long long)applied_count);
        printf(",\"skipped\":%lld", (long long)skipped_count);
        printf(",\"dependency_skips\":%lld", (long long)dependency_skip_count);
        printf(",\"source_item_count\":%lld", (long long)source_item_count);
        printf(",\"target_item_count\":%lld", (long long)target_item_count);
        printf(",\"source_link_count\":%lld", (long long)source_link_count);
        printf(",\"target_link_count\":%lld", (long long)target_link_count);
        printf(",\"item_missing\":%lld", (long long)item_missing);
        printf(",\"item_extra\":%lld", (long long)item_extra);
        printf(",\"link_missing\":%lld", (long long)link_missing);
        printf(",\"link_extra\":%lld", (long long)link_extra);
        printf(",\"item_field_mismatch\":%lld", (long long)item_field_mismatch);
        printf(",\"link_field_mismatch\":%lld", (long long)link_field_mismatch);
        printf("}\n");
    } else {
        printf("event-replay-apply: status=%s events=%lld applied=%lld skipped=%lld dependency_skips=%lld "
               "items(source=%lld target=%lld missing=%lld extra=%lld field_mismatch=%lld) "
               "links(source=%lld target=%lld missing=%lld extra=%lld field_mismatch=%lld) "
               "target=%s\n",
               status_ok ? "ok" : "drift",
               (long long)source_event_count,
               (long long)applied_count,
               (long long)skipped_count,
               (long long)dependency_skip_count,
               (long long)source_item_count,
               (long long)target_item_count,
               (long long)item_missing,
               (long long)item_extra,
               (long long)item_field_mismatch,
               (long long)source_link_count,
               (long long)target_link_count,
               (long long)link_missing,
               (long long)link_extra,
               (long long)link_field_mismatch,
               target_db_path);
    }

    exit_code = status_ok ? 0 : 2;

cleanup:
    (void)core_memdb_stmt_finalize(&remove_link_stmt);
    (void)core_memdb_stmt_finalize(&apply_link_snapshot_stmt);
    (void)core_memdb_stmt_finalize(&set_canonical_stmt);
    (void)core_memdb_stmt_finalize(&set_pinned_stmt);
    (void)core_memdb_stmt_finalize(&apply_item_snapshot_stmt);
    (void)core_memdb_stmt_finalize(&event_stmt);
    (void)core_memdb_stmt_finalize(&attach_stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&target_db);
    }
    if (source_attached) {
        (void)core_memdb_exec(&target_db, "DETACH DATABASE replay_src;");
    }
    (void)core_memdb_close(&target_db);
    return exit_code;
}

int cmd_event_replay_check(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *limit_text = find_flag_value(argc, argv, "--limit-events");
    int64_t limit_events = 0;
    int64_t event_count = 0;
    int64_t applied_count = 0;
    int64_t skipped_count = 0;
    int64_t live_item_count = 0;
    int64_t replay_item_count = 0;
    int64_t live_link_count = 0;
    int64_t replay_link_count = 0;
    int64_t item_missing = 0;
    int64_t item_extra = 0;
    int64_t link_missing = 0;
    int64_t link_extra = 0;
    int64_t item_field_mismatch = 0;
    int64_t link_field_mismatch = 0;
    int status_ok = 0;
    int has_row = 0;
    int exit_code = 1;
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt event_stmt = {0};
    CoreMemStmt ensure_item_stmt = {0};
    CoreMemStmt apply_item_snapshot_stmt = {0};
    CoreMemStmt set_pinned_stmt = {0};
    CoreMemStmt set_canonical_stmt = {0};
    CoreMemStmt ensure_link_stmt = {0};
    CoreMemStmt apply_link_snapshot_stmt = {0};
    CoreMemStmt remove_link_stmt = {0};
    CoreResult result;

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (format == MEM_CLI_OUTPUT_TSV) {
        fprintf(stderr, "event-replay-check: --format must be text or json\n");
        return 1;
    }
    if (limit_text) {
        if (!parse_i64_arg(limit_text, &limit_events) || limit_events <= 0 || limit_events > 200000) {
            fprintf(stderr, "event-replay-check: --limit-events must be in range [1, 200000]\n");
            return 1;
        }
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_exec(&db,
                             "CREATE TEMP TABLE IF NOT EXISTS mem_replay_item ("
                             "id INTEGER PRIMARY KEY, "
                             "stable_id TEXT NOT NULL DEFAULT '', "
                             "workspace_key TEXT NOT NULL DEFAULT '', "
                             "project_key TEXT NOT NULL DEFAULT '', "
                             "kind TEXT NOT NULL DEFAULT '', "
                             "title TEXT NOT NULL DEFAULT '', "
                             "body TEXT NOT NULL DEFAULT '', "
                             "fingerprint TEXT NOT NULL DEFAULT '', "
                             "created_ns INTEGER NOT NULL DEFAULT 0, "
                             "updated_ns INTEGER NOT NULL DEFAULT 0, "
                             "pinned INTEGER NOT NULL DEFAULT 0, "
                             "canonical INTEGER NOT NULL DEFAULT 0, "
                             "ttl_until_ns INTEGER, "
                             "archived_ns INTEGER"
                             ");");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_exec(&db,
                             "CREATE TEMP TABLE IF NOT EXISTS mem_replay_link ("
                             "id INTEGER PRIMARY KEY, "
                             "from_item_id INTEGER NOT NULL DEFAULT 0, "
                             "to_item_id INTEGER NOT NULL DEFAULT 0, "
                             "kind TEXT NOT NULL DEFAULT '', "
                             "weight REAL, "
                             "note TEXT"
                             ");");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_exec(&db, "DELETE FROM mem_replay_item;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_exec(&db, "DELETE FROM mem_replay_link;");
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&db,
                                "INSERT INTO mem_replay_item (id, stable_id, workspace_key, project_key, kind) "
                                "VALUES (?1, COALESCE(?2, ''), COALESCE(?3, ''), COALESCE(?4, ''), COALESCE(?5, '')) "
                                "ON CONFLICT(id) DO NOTHING;",
                                &ensure_item_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db,
                                "UPDATE mem_replay_item SET "
                                "title = COALESCE(json_extract(?2, '$.title'), title), "
                                "body = COALESCE(json_extract(?2, '$.body'), body), "
                                "fingerprint = COALESCE(json_extract(?2, '$.fingerprint'), fingerprint), "
                                "stable_id = COALESCE(json_extract(?2, '$.stable_id'), stable_id), "
                                "workspace_key = COALESCE(json_extract(?2, '$.workspace_key'), workspace_key), "
                                "project_key = COALESCE(json_extract(?2, '$.project_key'), project_key), "
                                "kind = COALESCE(json_extract(?2, '$.kind'), kind), "
                                "created_ns = COALESCE(CAST(json_extract(?2, '$.created_ns') AS INTEGER), created_ns), "
                                "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), updated_ns), "
                                "pinned = COALESCE(CAST(json_extract(?2, '$.pinned') AS INTEGER), pinned), "
                                "canonical = COALESCE(CAST(json_extract(?2, '$.canonical') AS INTEGER), canonical), "
                                "ttl_until_ns = CASE "
                                "  WHEN json_type(?2, '$.ttl_until_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.ttl_until_ns') AS INTEGER) "
                                "  ELSE ttl_until_ns "
                                "END, "
                                "archived_ns = CASE "
                                "  WHEN json_type(?2, '$.archived_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.archived_ns') AS INTEGER) "
                                "  ELSE archived_ns "
                                "END "
                                "WHERE id = ?1;",
                                &apply_item_snapshot_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db,
                                "UPDATE mem_replay_item "
                                "SET pinned = COALESCE(CAST(json_extract(?2, '$.value') AS INTEGER), pinned), "
                                "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), updated_ns) "
                                "WHERE id = ?1;",
                                &set_pinned_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db,
                                "UPDATE mem_replay_item "
                                "SET canonical = COALESCE(CAST(json_extract(?2, '$.value') AS INTEGER), canonical), "
                                "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), updated_ns) "
                                "WHERE id = ?1;",
                                &set_canonical_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db,
                                "INSERT INTO mem_replay_link (id) VALUES (?1) "
                                "ON CONFLICT(id) DO NOTHING;",
                                &ensure_link_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db,
                                "UPDATE mem_replay_link SET "
                                "from_item_id = COALESCE(CAST(json_extract(?2, '$.from_item_id') AS INTEGER), CAST(json_extract(?2, '$.from') AS INTEGER), from_item_id), "
                                "to_item_id = COALESCE(CAST(json_extract(?2, '$.to_item_id') AS INTEGER), CAST(json_extract(?2, '$.to') AS INTEGER), to_item_id), "
                                "kind = COALESCE(json_extract(?2, '$.kind'), kind), "
                                "weight = CASE "
                                "  WHEN json_type(?2, '$.weight') IS NOT NULL THEN CAST(json_extract(?2, '$.weight') AS REAL) "
                                "  ELSE weight "
                                "END, "
                                "note = CASE "
                                "  WHEN json_type(?2, '$.note') IS NOT NULL THEN json_extract(?2, '$.note') "
                                "  ELSE note "
                                "END "
                                "WHERE id = ?1;",
                                &apply_link_snapshot_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db,
                                "DELETE FROM mem_replay_link WHERE id = ?1;",
                                &remove_link_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }

    if (limit_events > 0) {
        result = core_memdb_prepare(&db,
                                    "SELECT event_type, "
                                    "COALESCE(item_id, 0), "
                                    "COALESCE(link_id, 0), "
                                    "COALESCE(stable_id, ''), "
                                    "workspace_key, "
                                    "project_key, "
                                    "kind, "
                                    "payload_json "
                                    "FROM mem_event "
                                    "ORDER BY id ASC "
                                    "LIMIT ?1;",
                                    &event_stmt);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_bind_i64(&event_stmt, 1, limit_events);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
    } else {
        result = core_memdb_prepare(&db,
                                    "SELECT event_type, "
                                    "COALESCE(item_id, 0), "
                                    "COALESCE(link_id, 0), "
                                    "COALESCE(stable_id, ''), "
                                    "workspace_key, "
                                    "project_key, "
                                    "kind, "
                                    "payload_json "
                                    "FROM mem_event "
                                    "ORDER BY id ASC;",
                                    &event_stmt);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
    }

    for (;;) {
        CoreStr event_type = {0};
        CoreStr stable_text = {0};
        CoreStr workspace_text = {0};
        CoreStr project_text = {0};
        CoreStr kind_text = {0};
        CoreStr payload_text = {0};
        int64_t item_id = 0;
        int64_t link_id = 0;
        int row_applied = 0;
        int step_has_row = 0;

        result = core_memdb_stmt_step(&event_stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_text(&event_stmt, 0, &event_type);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&event_stmt, 1, &item_id);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&event_stmt, 2, &link_id);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 3, &stable_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 4, &workspace_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 5, &project_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 6, &kind_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&event_stmt, 7, &payload_text);
        if (result.code != CORE_OK) {
            print_core_error("event-replay-check", result);
            goto cleanup;
        }

        event_count += 1;

        if (core_str_equals_cstr(event_type, "NodeCreated") ||
            core_str_equals_cstr(event_type, "NodeBodyUpdated") ||
            core_str_equals_cstr(event_type, "NodeMerged") ||
            core_str_equals_cstr(event_type, "NodeMetadataPatched")) {
            if (item_id <= 0) {
                skipped_count += 1;
                continue;
            }

            result = core_memdb_stmt_bind_i64(&ensure_item_stmt, 1, item_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 2, core_str_data_or_empty(stable_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 3, core_str_data_or_empty(workspace_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 4, core_str_data_or_empty(project_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 5, core_str_data_or_empty(kind_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&ensure_item_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event replay ensure_item returned unexpected row" };
                }
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&ensure_item_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }

            result = core_memdb_stmt_bind_i64(&apply_item_snapshot_stmt, 1, item_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_item_snapshot_stmt, 2, core_str_data_or_empty(payload_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&apply_item_snapshot_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event replay apply_item_snapshot returned unexpected row" };
                }
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&apply_item_snapshot_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            row_applied = 1;
        } else if (core_str_equals_cstr(event_type, "NodePinnedSet") ||
                   core_str_equals_cstr(event_type, "NodeCanonicalSet")) {
            if (item_id <= 0) {
                skipped_count += 1;
                continue;
            }

            result = core_memdb_stmt_bind_i64(&ensure_item_stmt, 1, item_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 2, core_str_data_or_empty(stable_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 3, core_str_data_or_empty(workspace_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 4, core_str_data_or_empty(project_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&ensure_item_stmt, 5, core_str_data_or_empty(kind_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&ensure_item_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event replay ensure_item returned unexpected row" };
                }
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&ensure_item_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }

            if (core_str_equals_cstr(event_type, "NodePinnedSet")) {
                result = core_memdb_stmt_bind_i64(&set_pinned_stmt, 1, item_id);
                if (result.code != CORE_OK) {
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
                result = core_memdb_stmt_bind_text(&set_pinned_stmt, 2, core_str_data_or_empty(payload_text));
                if (result.code != CORE_OK) {
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
                result = core_memdb_stmt_step(&set_pinned_stmt, &step_has_row);
                if (result.code != CORE_OK || step_has_row) {
                    if (result.code == CORE_OK) {
                        result = (CoreResult){ CORE_ERR_FORMAT, "event replay set_pinned returned unexpected row" };
                    }
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
                result = core_memdb_stmt_reset(&set_pinned_stmt);
                if (result.code != CORE_OK) {
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
            } else {
                result = core_memdb_stmt_bind_i64(&set_canonical_stmt, 1, item_id);
                if (result.code != CORE_OK) {
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
                result = core_memdb_stmt_bind_text(&set_canonical_stmt, 2, core_str_data_or_empty(payload_text));
                if (result.code != CORE_OK) {
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
                result = core_memdb_stmt_step(&set_canonical_stmt, &step_has_row);
                if (result.code != CORE_OK || step_has_row) {
                    if (result.code == CORE_OK) {
                        result = (CoreResult){ CORE_ERR_FORMAT, "event replay set_canonical returned unexpected row" };
                    }
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
                result = core_memdb_stmt_reset(&set_canonical_stmt);
                if (result.code != CORE_OK) {
                    print_core_error("event-replay-check", result);
                    goto cleanup;
                }
            }
            row_applied = 1;
        } else if (core_str_equals_cstr(event_type, "EdgeAdded") ||
                   core_str_equals_cstr(event_type, "EdgeUpdated")) {
            if (link_id <= 0) {
                skipped_count += 1;
                continue;
            }

            result = core_memdb_stmt_bind_i64(&ensure_link_stmt, 1, link_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&ensure_link_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event replay ensure_link returned unexpected row" };
                }
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&ensure_link_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }

            result = core_memdb_stmt_bind_i64(&apply_link_snapshot_stmt, 1, link_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_link_snapshot_stmt, 2, core_str_data_or_empty(payload_text));
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&apply_link_snapshot_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event replay apply_link_snapshot returned unexpected row" };
                }
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&apply_link_snapshot_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            row_applied = 1;
        } else if (core_str_equals_cstr(event_type, "EdgeRemoved")) {
            if (link_id <= 0) {
                skipped_count += 1;
                continue;
            }
            result = core_memdb_stmt_bind_i64(&remove_link_stmt, 1, link_id);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&remove_link_stmt, &step_has_row);
            if (result.code != CORE_OK || step_has_row) {
                if (result.code == CORE_OK) {
                    result = (CoreResult){ CORE_ERR_FORMAT, "event replay remove_link returned unexpected row" };
                }
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&remove_link_stmt);
            if (result.code != CORE_OK) {
                print_core_error("event-replay-check", result);
                goto cleanup;
            }
            row_applied = 1;
        } else {
            skipped_count += 1;
            continue;
        }

        if (row_applied) {
            applied_count += 1;
        }
    }

    result = query_single_i64(&db, "SELECT COUNT(*) FROM mem_item;", &live_item_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db, "SELECT COUNT(*) FROM mem_replay_item;", &replay_item_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db, "SELECT COUNT(*) FROM mem_link;", &live_link_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db, "SELECT COUNT(*) FROM mem_replay_link;", &replay_link_count);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db,
                              "SELECT COUNT(*) "
                              "FROM mem_item AS live "
                              "LEFT JOIN mem_replay_item AS replay ON replay.id = live.id "
                              "WHERE replay.id IS NULL;",
                              &item_missing);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db,
                              "SELECT COUNT(*) "
                              "FROM mem_replay_item AS replay "
                              "LEFT JOIN mem_item AS live ON live.id = replay.id "
                              "WHERE live.id IS NULL;",
                              &item_extra);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db,
                              "SELECT COUNT(*) "
                              "FROM mem_link AS live "
                              "LEFT JOIN mem_replay_link AS replay ON replay.id = live.id "
                              "WHERE replay.id IS NULL;",
                              &link_missing);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db,
                              "SELECT COUNT(*) "
                              "FROM mem_replay_link AS replay "
                              "LEFT JOIN mem_link AS live ON live.id = replay.id "
                              "WHERE live.id IS NULL;",
                              &link_extra);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db,
                              "SELECT COUNT(*) "
                              "FROM mem_item AS live "
                              "JOIN mem_replay_item AS replay ON replay.id = live.id "
                              "WHERE NOT ("
                              "live.title = replay.title "
                              "AND live.body = replay.body "
                              "AND live.fingerprint = replay.fingerprint "
                              "AND COALESCE(live.stable_id, '') = COALESCE(replay.stable_id, '') "
                              "AND COALESCE(live.workspace_key, '') = COALESCE(replay.workspace_key, '') "
                              "AND COALESCE(live.project_key, '') = COALESCE(replay.project_key, '') "
                              "AND COALESCE(live.kind, '') = COALESCE(replay.kind, '') "
                              "AND live.created_ns = replay.created_ns "
                              "AND live.updated_ns = replay.updated_ns "
                              "AND live.pinned = replay.pinned "
                              "AND live.canonical = replay.canonical "
                              "AND live.ttl_until_ns IS replay.ttl_until_ns "
                              "AND live.archived_ns IS replay.archived_ns"
                              ");",
                              &item_field_mismatch);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }
    result = query_single_i64(&db,
                              "SELECT COUNT(*) "
                              "FROM mem_link AS live "
                              "JOIN mem_replay_link AS replay ON replay.id = live.id "
                              "WHERE NOT ("
                              "live.from_item_id = replay.from_item_id "
                              "AND live.to_item_id = replay.to_item_id "
                              "AND COALESCE(live.kind, '') = COALESCE(replay.kind, '') "
                              "AND ("
                              "  (live.weight IS NULL AND replay.weight IS NULL) "
                              "  OR (live.weight IS NOT NULL AND replay.weight IS NOT NULL AND ABS(live.weight - replay.weight) < 0.000001)"
                              ") "
                              "AND live.note IS replay.note"
                              ");",
                              &link_field_mismatch);
    if (result.code != CORE_OK) {
        print_core_error("event-replay-check", result);
        goto cleanup;
    }

    status_ok = (item_missing == 0 &&
                 item_extra == 0 &&
                 link_missing == 0 &&
                 link_extra == 0 &&
                 item_field_mismatch == 0 &&
                 link_field_mismatch == 0) ? 1 : 0;

    if (format == MEM_CLI_OUTPUT_JSON) {
        printf("{\"ok\":%d", status_ok);
        printf(",\"events\":%lld", (long long)event_count);
        printf(",\"applied\":%lld", (long long)applied_count);
        printf(",\"skipped\":%lld", (long long)skipped_count);
        printf(",\"live_item_count\":%lld", (long long)live_item_count);
        printf(",\"replay_item_count\":%lld", (long long)replay_item_count);
        printf(",\"live_link_count\":%lld", (long long)live_link_count);
        printf(",\"replay_link_count\":%lld", (long long)replay_link_count);
        printf(",\"item_missing\":%lld", (long long)item_missing);
        printf(",\"item_extra\":%lld", (long long)item_extra);
        printf(",\"link_missing\":%lld", (long long)link_missing);
        printf(",\"link_extra\":%lld", (long long)link_extra);
        printf(",\"item_field_mismatch\":%lld", (long long)item_field_mismatch);
        printf(",\"link_field_mismatch\":%lld", (long long)link_field_mismatch);
        printf("}\n");
    } else {
        printf("event-replay-check: status=%s events=%lld applied=%lld skipped=%lld "
               "items(live=%lld replay=%lld missing=%lld extra=%lld field_mismatch=%lld) "
               "links(live=%lld replay=%lld missing=%lld extra=%lld field_mismatch=%lld)\n",
               status_ok ? "ok" : "drift",
               (long long)event_count,
               (long long)applied_count,
               (long long)skipped_count,
               (long long)live_item_count,
               (long long)replay_item_count,
               (long long)item_missing,
               (long long)item_extra,
               (long long)item_field_mismatch,
               (long long)live_link_count,
               (long long)replay_link_count,
               (long long)link_missing,
               (long long)link_extra,
               (long long)link_field_mismatch);
    }

    exit_code = status_ok ? 0 : 2;

cleanup:
    (void)core_memdb_stmt_finalize(&remove_link_stmt);
    (void)core_memdb_stmt_finalize(&apply_link_snapshot_stmt);
    (void)core_memdb_stmt_finalize(&ensure_link_stmt);
    (void)core_memdb_stmt_finalize(&set_canonical_stmt);
    (void)core_memdb_stmt_finalize(&set_pinned_stmt);
    (void)core_memdb_stmt_finalize(&apply_item_snapshot_stmt);
    (void)core_memdb_stmt_finalize(&ensure_item_stmt);
    (void)core_memdb_stmt_finalize(&event_stmt);
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_event_backfill(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    int dry_run = has_flag(argc, argv, "--dry-run");
    int tx_started = 0;
    int has_row = 0;
    int exit_code = 1;
    int64_t item_candidate_count = 0;
    int64_t link_candidate_count = 0;
    int64_t node_events_needed = 0;
    int64_t pin_events_needed = 0;
    int64_t canonical_events_needed = 0;
    int64_t edge_events_needed = 0;
    int64_t events_written = 0;
    char detail[256];
    MemCliOutputFormat format = MEM_CLI_OUTPUT_TEXT;
    CoreMemDb db = {0};
    CoreMemStmt item_stmt = {0};
    CoreMemStmt link_stmt = {0};
    CoreResult result;

    if (!db_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_output_format(argc, argv, &format)) {
        return 1;
    }
    if (format == MEM_CLI_OUTPUT_TSV) {
        fprintf(stderr, "event-backfill: --format must be text or json\n");
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    if (!dry_run) {
        result = core_memdb_tx_begin(&db);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        tx_started = 1;
    }

    result = core_memdb_prepare(&db,
                                "WITH item_state AS ("
                                "SELECT i.id, "
                                "COALESCE(i.stable_id, '') AS stable_id, "
                                "i.workspace_key AS workspace_key, "
                                "i.project_key AS project_key, "
                                "i.kind AS kind, "
                                "i.pinned AS pinned, "
                                "i.canonical AS canonical, "
                                "CASE WHEN EXISTS ("
                                "SELECT 1 "
                                "FROM mem_event e "
                                "WHERE e.item_id = i.id "
                                "AND e.event_type IN ('NodeCreated', 'NodeBodyUpdated', 'NodeMerged', 'NodeMetadataPatched') "
                                "AND json_type(e.payload_json, '$.title') IS NOT NULL "
                                "AND json_type(e.payload_json, '$.body') IS NOT NULL "
                                "AND json_type(e.payload_json, '$.fingerprint') IS NOT NULL "
                                "LIMIT 1"
                                ") THEN 1 ELSE 0 END AS has_node_event, "
                                "CASE WHEN EXISTS ("
                                "SELECT 1 "
                                "FROM mem_event e "
                                "WHERE e.item_id = i.id "
                                "AND e.event_type = 'NodePinnedSet' "
                                "LIMIT 1"
                                ") THEN 1 ELSE 0 END AS has_pinned_event, "
                                "CASE WHEN EXISTS ("
                                "SELECT 1 "
                                "FROM mem_event e "
                                "WHERE e.item_id = i.id "
                                "AND e.event_type = 'NodeCanonicalSet' "
                                "LIMIT 1"
                                ") THEN 1 ELSE 0 END AS has_canonical_event "
                                "FROM mem_item i"
                                ") "
                                "SELECT id, "
                                "stable_id, "
                                "workspace_key, "
                                "project_key, "
                                "kind, "
                                "pinned, "
                                "canonical, "
                                "has_node_event, "
                                "has_pinned_event, "
                                "has_canonical_event "
                                "FROM item_state "
                                "WHERE has_node_event = 0 "
                                "OR (pinned <> 0 AND has_pinned_event = 0) "
                                "OR (canonical <> 0 AND has_canonical_event = 0) "
                                "ORDER BY id ASC;",
                                &item_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-backfill", result);
        goto cleanup;
    }

    for (;;) {
        int64_t item_id = 0;
        int64_t pinned_value = 0;
        int64_t canonical_value = 0;
        int64_t has_node_event = 0;
        int64_t has_pinned_event = 0;
        int64_t has_canonical_event = 0;
        CoreStr stable_text = {0};
        CoreStr workspace_text = {0};
        CoreStr project_text = {0};
        CoreStr kind_text = {0};
        int needs_node = 0;
        int needs_pinned = 0;
        int needs_canonical = 0;
        char payload[160];
        char *snapshot_payload = 0;

        result = core_memdb_stmt_step(&item_stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&item_stmt, 0, &item_id);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&item_stmt, 1, &stable_text);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&item_stmt, 2, &workspace_text);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&item_stmt, 3, &project_text);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&item_stmt, 4, &kind_text);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&item_stmt, 5, &pinned_value);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&item_stmt, 6, &canonical_value);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&item_stmt, 7, &has_node_event);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&item_stmt, 8, &has_pinned_event);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&item_stmt, 9, &has_canonical_event);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }

        needs_node = has_node_event == 0 ? 1 : 0;
        needs_pinned = (pinned_value != 0 && has_pinned_event == 0) ? 1 : 0;
        needs_canonical = (canonical_value != 0 && has_canonical_event == 0) ? 1 : 0;

        if (!needs_node && !needs_pinned && !needs_canonical) {
            continue;
        }

        item_candidate_count += 1;

        if (needs_node) {
            node_events_needed += 1;
            if (!dry_run) {
                result = fetch_item_snapshot_json_alloc(&db, item_id, &snapshot_payload);
                if (result.code != CORE_OK) {
                    print_core_error("event-backfill", result);
                    goto cleanup;
                }
                result = append_event_entry(&db,
                                            session_id,
                                            "NodeCreated",
                                            item_id,
                                            0,
                                            core_str_data_or_empty(stable_text),
                                            core_str_data_or_empty(workspace_text),
                                            core_str_data_or_empty(project_text),
                                            core_str_data_or_empty(kind_text),
                                            snapshot_payload);
                core_free(snapshot_payload);
                snapshot_payload = 0;
                if (result.code != CORE_OK) {
                    print_core_error("event-backfill", result);
                    goto cleanup;
                }
                events_written += 1;
            }
        }

        if (needs_pinned) {
            pin_events_needed += 1;
            if (!dry_run) {
                (void)snprintf(payload,
                               sizeof(payload),
                               "{\"field\":\"pinned\",\"value\":%lld,\"source\":\"backfill\"}",
                               (long long)pinned_value);
                result = append_event_entry(&db,
                                            session_id,
                                            "NodePinnedSet",
                                            item_id,
                                            0,
                                            core_str_data_or_empty(stable_text),
                                            core_str_data_or_empty(workspace_text),
                                            core_str_data_or_empty(project_text),
                                            core_str_data_or_empty(kind_text),
                                            payload);
                if (result.code != CORE_OK) {
                    print_core_error("event-backfill", result);
                    goto cleanup;
                }
                events_written += 1;
            }
        }

        if (needs_canonical) {
            canonical_events_needed += 1;
            if (!dry_run) {
                (void)snprintf(payload,
                               sizeof(payload),
                               "{\"field\":\"canonical\",\"value\":%lld,\"source\":\"backfill\"}",
                               (long long)canonical_value);
                result = append_event_entry(&db,
                                            session_id,
                                            "NodeCanonicalSet",
                                            item_id,
                                            0,
                                            core_str_data_or_empty(stable_text),
                                            core_str_data_or_empty(workspace_text),
                                            core_str_data_or_empty(project_text),
                                            core_str_data_or_empty(kind_text),
                                            payload);
                if (result.code != CORE_OK) {
                    print_core_error("event-backfill", result);
                    goto cleanup;
                }
                events_written += 1;
            }
        }

        core_free(snapshot_payload);
    }

    result = core_memdb_stmt_finalize(&item_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-backfill", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&db,
                                "WITH link_state AS ("
                                "SELECT l.id, "
                                "l.from_item_id AS from_item_id, "
                                "l.to_item_id AS to_item_id, "
                                "COALESCE(l.kind, '') AS kind, "
                                "CASE WHEN EXISTS ("
                                "SELECT 1 "
                                "FROM mem_event e "
                                "WHERE e.link_id = l.id "
                                "AND e.event_type IN ('EdgeAdded', 'EdgeUpdated') "
                                "AND (json_type(e.payload_json, '$.from_item_id') IS NOT NULL OR json_type(e.payload_json, '$.from') IS NOT NULL) "
                                "AND (json_type(e.payload_json, '$.to_item_id') IS NOT NULL OR json_type(e.payload_json, '$.to') IS NOT NULL) "
                                "AND json_type(e.payload_json, '$.kind') IS NOT NULL "
                                "LIMIT 1"
                                ") THEN 1 ELSE 0 END AS has_edge_event "
                                "FROM mem_link l"
                                ") "
                                "SELECT id, from_item_id, to_item_id, kind "
                                "FROM link_state "
                                "WHERE has_edge_event = 0 "
                                "ORDER BY id ASC;",
                                &link_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-backfill", result);
        goto cleanup;
    }

    for (;;) {
        int64_t link_id = 0;
        int64_t from_item_id = 0;
        int64_t to_item_id = 0;
        char stable_id[128];
        char workspace_key[128];
        char project_key[128];
        char item_kind[128];
        char *payload = 0;

        result = core_memdb_stmt_step(&link_stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&link_stmt, 0, &link_id);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&link_stmt, 1, &from_item_id);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_i64(&link_stmt, 2, &to_item_id);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        link_candidate_count += 1;
        edge_events_needed += 1;

        if (dry_run) {
            continue;
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
            print_core_error("event-backfill", result);
            goto cleanup;
        }

        result = fetch_link_snapshot_json_alloc(&db, link_id, &payload);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
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
                                    payload);
        core_free(payload);
        payload = 0;
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        events_written += 1;
    }

    result = core_memdb_stmt_finalize(&link_stmt);
    if (result.code != CORE_OK) {
        print_core_error("event-backfill", result);
        goto cleanup;
    }

    if (!dry_run) {
        (void)snprintf(detail,
                       sizeof(detail),
                       "node=%lld pinned=%lld canonical=%lld edge=%lld written=%lld",
                       (long long)node_events_needed,
                       (long long)pin_events_needed,
                       (long long)canonical_events_needed,
                       (long long)edge_events_needed,
                       (long long)events_written);
        result = append_audit_entry(&db,
                                    session_id,
                                    "event-backfill",
                                    "ok",
                                    0,
                                    0,
                                    "",
                                    "",
                                    "maintenance",
                                    detail);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }

        result = core_memdb_tx_commit(&db);
        if (result.code != CORE_OK) {
            print_core_error("event-backfill", result);
            goto cleanup;
        }
        tx_started = 0;
    }

    if (format == MEM_CLI_OUTPUT_JSON) {
        printf("{\"ok\":1,\"dry_run\":%d", dry_run ? 1 : 0);
        printf(",\"item_candidates\":%lld", (long long)item_candidate_count);
        printf(",\"link_candidates\":%lld", (long long)link_candidate_count);
        printf(",\"node_events_needed\":%lld", (long long)node_events_needed);
        printf(",\"pin_events_needed\":%lld", (long long)pin_events_needed);
        printf(",\"canonical_events_needed\":%lld", (long long)canonical_events_needed);
        printf(",\"edge_events_needed\":%lld", (long long)edge_events_needed);
        printf(",\"events_written\":%lld", (long long)events_written);
        printf("}\n");
    } else {
        printf("event-backfill: dry_run=%s item_candidates=%lld link_candidates=%lld "
               "needs(node=%lld pin=%lld canonical=%lld edge=%lld) written=%lld\n",
               dry_run ? "on" : "off",
               (long long)item_candidate_count,
               (long long)link_candidate_count,
               (long long)node_events_needed,
               (long long)pin_events_needed,
               (long long)canonical_events_needed,
               (long long)edge_events_needed,
               (long long)events_written);
    }

    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&link_stmt);
    (void)core_memdb_stmt_finalize(&item_stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    (void)core_memdb_close(&db);
    return exit_code;
}
