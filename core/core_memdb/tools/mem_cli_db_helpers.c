#include "mem_cli_db_helpers.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mem_cli_args.h"

int core_str_equals_cstr(CoreStr value, const char *text) {
    size_t text_len;

    if (!text) {
        return !value.data || value.len == 0u;
    }
    if (!value.data) {
        return text[0] == '\0';
    }
    text_len = strlen(text);
    if (value.len != text_len) {
        return 0;
    }
    return memcmp(value.data, text, text_len) == 0 ? 1 : 0;
}

const char *core_str_data_or_empty(CoreStr value) {
    if (!value.data) {
        return "";
    }
    return value.data;
}

CoreResult bind_optional_text_or_null(CoreMemStmt *stmt, int index_1based, const char *value) {
    if (!value) {
        return core_memdb_stmt_bind_null(stmt, index_1based);
    }
    return core_memdb_stmt_bind_text(stmt, index_1based, value);
}

int append_sql_fragment(char *sql, size_t sql_cap, const char *fragment) {
    size_t used;
    size_t fragment_len;

    if (!sql || sql_cap == 0u || !fragment) {
        return 0;
    }

    used = strlen(sql);
    fragment_len = strlen(fragment);
    if (used + fragment_len + 1u > sql_cap) {
        return 0;
    }

    memcpy(sql + used, fragment, fragment_len);
    sql[used + fragment_len] = '\0';
    return 1;
}

int64_t current_time_ns(void) {
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);
    return ((int64_t)ts.tv_sec * 1000000000LL) + (int64_t)ts.tv_nsec;
}

CoreResult query_single_i64(CoreMemDb *db, const char *sql, int64_t *out_value) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    if (!db || !sql || !out_value) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_value = 0;
    result = core_memdb_prepare(db, sql, &stmt);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "query returned no row" };
        goto cleanup;
    }

    result = core_memdb_stmt_column_i64(&stmt, 0, out_value);

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult fetch_session_add_write_count(CoreMemDb *db,
                                         const char *session_id,
                                         int64_t *out_count) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    if (!db || !session_id || !out_count) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_count = 0;
    result = core_memdb_prepare(db,
                                "SELECT COUNT(*) "
                                "FROM mem_audit "
                                "WHERE session_id = ?1 AND action = 'add' AND status = 'ok';",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_text(&stmt, 1, session_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "session write count returned no row" };
        goto cleanup;
    }
    result = core_memdb_stmt_column_i64(&stmt, 0, out_count);

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult append_audit_entry(CoreMemDb *db,
                              const char *session_id,
                              const char *action,
                              const char *status,
                              int64_t item_id,
                              const char *stable_id,
                              const char *workspace_key,
                              const char *project_key,
                              const char *item_kind,
                              const char *detail) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    if (!db || !action || !status || !detail) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    result = core_memdb_prepare(db,
                                "INSERT INTO mem_audit ("
                                "ts_ns, session_id, action, status, item_id, stable_id, workspace_key, project_key, kind, detail"
                                ") VALUES ("
                                "?1, ?2, ?3, ?4, ?5, ?6, COALESCE(?7, ''), COALESCE(?8, ''), COALESCE(?9, ''), ?10"
                                ");",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_stmt_bind_i64(&stmt, 1, current_time_ns());
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 2, session_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 3, action);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 4, status);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (item_id != 0) {
        result = core_memdb_stmt_bind_i64(&stmt, 5, item_id);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 5);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 6, stable_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 7, workspace_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 8, project_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 9, item_kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 10, detail);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "audit insert returned unexpected row" };
        goto cleanup;
    }
    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

static uint64_t g_event_id_nonce = 0u;

static int build_event_id(char *out_event_id,
                          size_t out_cap,
                          int64_t ts_ns,
                          const char *event_type,
                          int64_t item_id,
                          int64_t link_id) {
    unsigned long long nonce;
    int written;

    if (!out_event_id || out_cap == 0u || !event_type || event_type[0] == '\0') {
        return 0;
    }

    g_event_id_nonce += 1u;
    nonce = (unsigned long long)g_event_id_nonce;
    written = snprintf(out_event_id,
                       out_cap,
                       "%lld:%s:%lld:%lld:%llu",
                       (long long)ts_ns,
                       event_type,
                       (long long)item_id,
                       (long long)link_id,
                       nonce);
    return written > 0 && (size_t)written < out_cap;
}

CoreResult append_event_entry(CoreMemDb *db,
                              const char *session_id,
                              const char *event_type,
                              int64_t item_id,
                              int64_t link_id,
                              const char *stable_id,
                              const char *workspace_key,
                              const char *project_key,
                              const char *item_kind,
                              const char *payload_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int64_t ts_ns = current_time_ns();
    const char *actor_id = "mem_cli";
    char event_id[192];

    if (!db || !event_type || event_type[0] == '\0') {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    if (!payload_json || payload_json[0] == '\0') {
        payload_json = "{}";
    }
    if (session_id && session_id[0] != '\0') {
        actor_id = session_id;
    }
    if (!build_event_id(event_id, sizeof(event_id), ts_ns, event_type, item_id, link_id)) {
        return (CoreResult){ CORE_ERR_FORMAT, "failed to build event_id" };
    }

    result = core_memdb_prepare(db,
                                "INSERT INTO mem_event ("
                                "ts_ns, session_id, event_id, event_type, actor_type, actor_id, item_id, link_id, stable_id, workspace_key, project_key, kind, payload_json"
                                ") VALUES ("
                                "?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, COALESCE(?10, ''), COALESCE(?11, ''), COALESCE(?12, ''), COALESCE(?13, '{}')"
                                ");",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_stmt_bind_i64(&stmt, 1, ts_ns);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 2, session_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 3, event_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 4, event_type);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 5, "cli");
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 6, actor_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (item_id > 0) {
        result = core_memdb_stmt_bind_i64(&stmt, 7, item_id);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 7);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (link_id > 0) {
        result = core_memdb_stmt_bind_i64(&stmt, 8, link_id);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 8);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 9, stable_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 10, workspace_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 11, project_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 12, item_kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 13, payload_json);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "event insert returned unexpected row" };
        goto cleanup;
    }
    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult sync_fts_row(CoreMemDb *db, int64_t item_id, const char *title, const char *body) {
    CoreMemStmt delete_stmt = {0};
    CoreMemStmt insert_stmt = {0};
    CoreResult result;
    int has_row = 0;

    result = core_memdb_prepare(db, "DELETE FROM mem_item_fts WHERE rowid = ?1;", &delete_stmt);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&delete_stmt, 1, item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&delete_stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    result = core_memdb_stmt_finalize(&delete_stmt);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    result = core_memdb_prepare(db,
                                "INSERT INTO mem_item_fts (rowid, title, body) VALUES (?1, ?2, ?3);",
                                &insert_stmt);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&insert_stmt, 1, item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&insert_stmt, 2, title);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&insert_stmt, 3, body);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&insert_stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "fts write returned unexpected row" };
        goto cleanup;
    }

    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&insert_stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&delete_stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult find_duplicate_item(CoreMemDb *db,
                               const char *fingerprint,
                               int64_t *out_item_id,
                               int *out_found) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    if (!db || !fingerprint || !out_item_id || !out_found) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_item_id = 0;
    *out_found = 0;

    result = core_memdb_prepare(db,
                                "SELECT id "
                                "FROM mem_item "
                                "WHERE fingerprint = ?1 AND archived_ns IS NULL "
                                "ORDER BY canonical DESC, updated_ns DESC, id ASC "
                                "LIMIT 1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_stmt_bind_text(&stmt, 1, fingerprint);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = core_result_ok();
        goto cleanup;
    }

    result = core_memdb_stmt_column_i64(&stmt, 0, out_item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    *out_found = 1;

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

static void copy_core_str_to_cbuf(CoreStr value, char *out, size_t out_cap) {
    size_t copy_len;

    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (!value.data || value.len == 0u) {
        return;
    }

    copy_len = value.len;
    if (copy_len >= out_cap) {
        copy_len = out_cap - 1u;
    }
    memcpy(out, value.data, copy_len);
    out[copy_len] = '\0';
}

CoreResult fetch_item_audit_metadata(CoreMemDb *db,
                                     int64_t item_id,
                                     char *out_stable_id,
                                     size_t stable_id_cap,
                                     char *out_workspace,
                                     size_t workspace_cap,
                                     char *out_project,
                                     size_t project_cap,
                                     char *out_kind,
                                     size_t kind_cap) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr stable_id = {0};
    CoreStr workspace_key = {0};
    CoreStr project_key = {0};
    CoreStr item_kind = {0};

    if (!db || item_id <= 0 || !out_stable_id || !out_workspace || !out_project || !out_kind) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    out_stable_id[0] = '\0';
    out_workspace[0] = '\0';
    out_project[0] = '\0';
    out_kind[0] = '\0';

    result = core_memdb_prepare(db,
                                "SELECT COALESCE(stable_id, ''), workspace_key, project_key, kind "
                                "FROM mem_item WHERE id = ?1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_NOT_FOUND, "item metadata not found" };
        goto cleanup;
    }

    result = core_memdb_stmt_column_text(&stmt, 0, &stable_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 1, &workspace_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 2, &project_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 3, &item_kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    copy_core_str_to_cbuf(stable_id, out_stable_id, stable_id_cap);
    copy_core_str_to_cbuf(workspace_key, out_workspace, workspace_cap);
    copy_core_str_to_cbuf(project_key, out_project, project_cap);
    copy_core_str_to_cbuf(item_kind, out_kind, kind_cap);
    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult fetch_link_endpoints(CoreMemDb *db,
                                int64_t link_id,
                                int64_t *out_from_item_id,
                                int64_t *out_to_item_id) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    if (!db || link_id <= 0 || !out_from_item_id || !out_to_item_id) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_from_item_id = 0;
    *out_to_item_id = 0;
    result = core_memdb_prepare(db,
                                "SELECT from_item_id, to_item_id "
                                "FROM mem_link WHERE id = ?1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_NOT_FOUND, "link not found" };
        goto cleanup;
    }
    result = core_memdb_stmt_column_i64(&stmt, 0, out_from_item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_i64(&stmt, 1, out_to_item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult fetch_item_snapshot_json_alloc(CoreMemDb *db,
                                          int64_t item_id,
                                          char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr json_text = {0};
    char *buffer = 0;

    if (!db || item_id <= 0 || !out_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_json = 0;

    result = core_memdb_prepare(db,
                                "SELECT json_object("
                                "'title', title, "
                                "'body', body, "
                                "'fingerprint', fingerprint, "
                                "'stable_id', COALESCE(stable_id, ''), "
                                "'workspace_key', workspace_key, "
                                "'project_key', project_key, "
                                "'kind', kind, "
                                "'created_ns', created_ns, "
                                "'updated_ns', updated_ns, "
                                "'pinned', pinned, "
                                "'canonical', canonical, "
                                "'ttl_until_ns', ttl_until_ns, "
                                "'archived_ns', archived_ns"
                                ") "
                                "FROM mem_item "
                                "WHERE id = ?1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_NOT_FOUND, "item snapshot not found" };
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

CoreResult fetch_link_snapshot_json_alloc(CoreMemDb *db,
                                          int64_t link_id,
                                          char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr json_text = {0};
    char *buffer = 0;

    if (!db || link_id <= 0 || !out_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_json = 0;

    result = core_memdb_prepare(db,
                                "SELECT json_object("
                                "'from_item_id', from_item_id, "
                                "'to_item_id', to_item_id, "
                                "'kind', kind, "
                                "'weight', weight, "
                                "'note', note"
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
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_NOT_FOUND, "link snapshot not found" };
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

int open_db_or_fail(const char *db_path, CoreMemDb *db) {
    CoreResult result;

    result = core_memdb_open(db_path, db);
    if (result.code != CORE_OK) {
        print_core_error("open", result);
        return 0;
    }

    return 1;
}

CoreResult fetch_changes(CoreMemDb *db, int64_t *out_changes) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    if (!db || !out_changes) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_changes = 0;

    result = core_memdb_prepare(db, "SELECT changes();", &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "missing changes row" };
        goto cleanup;
    }
    result = core_memdb_stmt_column_i64(&stmt, 0, out_changes);

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult item_exists_active(CoreMemDb *db, int64_t item_id, int *out_exists) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int64_t count = 0;

    if (!db || !out_exists) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_exists = 0;
    result = core_memdb_prepare(db,
                                "SELECT COUNT(*) "
                                "FROM mem_item "
                                "WHERE id = ?1 AND archived_ns IS NULL;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "item existence query returned no row" };
        goto cleanup;
    }

    result = core_memdb_stmt_column_i64(&stmt, 0, &count);
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    *out_exists = count > 0 ? 1 : 0;
    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}

CoreResult fetch_next_item_id(CoreMemDb *db, int64_t *out_item_id) {
    return query_single_i64(db, "SELECT COALESCE(MAX(id), 0) + 1 FROM mem_item;", out_item_id);
}

CoreResult fetch_next_link_id(CoreMemDb *db, int64_t *out_link_id) {
    return query_single_i64(db, "SELECT COALESCE(MAX(id), 0) + 1 FROM mem_link;", out_link_id);
}

CoreResult sqlite_master_object_exists(CoreMemDb *db,
                                       const char *type_name,
                                       const char *object_name,
                                       int *out_exists) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    int64_t count = 0;

    if (!db || !type_name || !object_name || !out_exists) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    *out_exists = 0;
    result = core_memdb_prepare(db,
                                "SELECT COUNT(*) "
                                "FROM sqlite_master "
                                "WHERE type = ?1 AND name = ?2;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_text(&stmt, 1, type_name);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 2, object_name);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "sqlite_master object query returned no row" };
        goto cleanup;
    }
    result = core_memdb_stmt_column_i64(&stmt, 0, &count);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    *out_exists = count > 0 ? 1 : 0;
    result = core_result_ok();

cleanup:
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) {
            result = finalize_result;
        }
    }
    return result;
}
