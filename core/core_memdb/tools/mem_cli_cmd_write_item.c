#include "mem_cli_cmd_write_item.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_memdb.h"
#include "mem_cli_args.h"
#include "mem_cli_db_helpers.h"

static const char *kApplyItemEventSql =
    "INSERT INTO mem_item ("
    "id, stable_id, title, body, fingerprint, workspace_key, project_key, kind, "
    "created_ns, updated_ns, pinned, canonical, ttl_until_ns, archived_ns"
    ") VALUES ("
    "?1, "
    "NULLIF(COALESCE(json_extract(?2, '$.stable_id'), ''), ''), "
    "COALESCE(json_extract(?2, '$.title'), ''), "
    "COALESCE(json_extract(?2, '$.body'), ''), "
    "COALESCE(json_extract(?2, '$.fingerprint'), ''), "
    "COALESCE(json_extract(?2, '$.workspace_key'), ''), "
    "COALESCE(json_extract(?2, '$.project_key'), ''), "
    "COALESCE(json_extract(?2, '$.kind'), ''), "
    "COALESCE(CAST(json_extract(?2, '$.created_ns') AS INTEGER), ?3), "
    "COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), ?4), "
    "0, 0, NULL, NULL"
    ") "
    "ON CONFLICT(id) DO UPDATE SET "
    "stable_id = CASE "
    "  WHEN (mem_item.stable_id IS NULL OR mem_item.stable_id = '') "
    "    THEN COALESCE(NULLIF(json_extract(?2, '$.stable_id'), ''), mem_item.stable_id) "
    "  ELSE mem_item.stable_id "
    "END, "
    "title = COALESCE(json_extract(?2, '$.title'), mem_item.title), "
    "body = COALESCE(json_extract(?2, '$.body'), mem_item.body), "
    "fingerprint = COALESCE(json_extract(?2, '$.fingerprint'), mem_item.fingerprint), "
    "workspace_key = COALESCE(json_extract(?2, '$.workspace_key'), mem_item.workspace_key), "
    "project_key = COALESCE(json_extract(?2, '$.project_key'), mem_item.project_key), "
    "kind = COALESCE(json_extract(?2, '$.kind'), mem_item.kind), "
    "created_ns = COALESCE(CAST(json_extract(?2, '$.created_ns') AS INTEGER), mem_item.created_ns), "
    "updated_ns = COALESCE(CAST(json_extract(?2, '$.updated_ns') AS INTEGER), mem_item.updated_ns), "
    "pinned = COALESCE(CAST(json_extract(?2, '$.pinned') AS INTEGER), mem_item.pinned), "
    "canonical = COALESCE(CAST(json_extract(?2, '$.canonical') AS INTEGER), mem_item.canonical), "
    "ttl_until_ns = CASE "
    "  WHEN json_type(?2, '$.ttl_until_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.ttl_until_ns') AS INTEGER) "
    "  ELSE mem_item.ttl_until_ns "
    "END, "
    "archived_ns = CASE "
    "  WHEN json_type(?2, '$.archived_ns') IS NOT NULL THEN CAST(json_extract(?2, '$.archived_ns') AS INTEGER) "
    "  ELSE mem_item.archived_ns "
    "END "
    "WHERE mem_item.archived_ns IS NULL OR json_type(?2, '$.archived_ns') IS NOT NULL;";

static int parse_tsv_fields(char *line, char **fields, int max_fields) {
    int field_count = 0;
    char *cursor = line;

    if (!line || !fields || max_fields <= 0) {
        return 0;
    }

    fields[field_count++] = cursor;
    while (*cursor != '\0') {
        if (*cursor == '\t') {
            *cursor = '\0';
            if (field_count >= max_fields) {
                return field_count;
            }
            fields[field_count++] = cursor + 1;
        }
        cursor += 1;
    }
    return field_count;
}

static int build_fingerprint(const char *title,
                             const char *body,
                             char *out_fingerprint,
                             size_t out_cap) {
    size_t title_len;
    size_t body_len;
    size_t buffer_len;
    char *buffer;
    uint64_t hash_value;
    int written;

    if (!title || !body || !out_fingerprint || out_cap < 17u) {
        return 0;
    }

    title_len = strlen(title);
    body_len = strlen(body);
    buffer_len = title_len + 1u + body_len;
    buffer = (char *)core_alloc(buffer_len);
    if (!buffer) {
        return 0;
    }

    memcpy(buffer, title, title_len);
    buffer[title_len] = '\n';
    memcpy(buffer + title_len + 1u, body, body_len);

    hash_value = core_hash64_fnv1a(buffer, buffer_len);
    core_free(buffer);

    written = snprintf(out_fingerprint, out_cap, "%016llx", (unsigned long long)hash_value);
    return written > 0 && (size_t)written < out_cap;
}

static CoreResult build_item_event_payload_alloc(CoreMemDb *db,
                                                 const char *title,
                                                 const char *body,
                                                 const char *fingerprint,
                                                 const char *stable_id,
                                                 const char *workspace_key,
                                                 const char *project_key,
                                                 const char *item_kind,
                                                 int has_created_ns,
                                                 int64_t created_ns,
                                                 int has_updated_ns,
                                                 int64_t updated_ns,
                                                 char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr json_text = {0};
    char *buffer = 0;

    if (!db || !title || !body || !fingerprint || !out_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_json = 0;

    result = core_memdb_prepare(db,
                                "SELECT json_object("
                                "'title', ?1, "
                                "'body', ?2, "
                                "'fingerprint', ?3, "
                                "'stable_id', ?4, "
                                "'workspace_key', ?5, "
                                "'project_key', ?6, "
                                "'kind', ?7, "
                                "'created_ns', ?8, "
                                "'updated_ns', ?9"
                                ");",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_text(&stmt, 1, title);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 2, body);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 3, fingerprint);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 4, stable_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 5, workspace_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 6, project_key);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = bind_optional_text_or_null(&stmt, 7, item_kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_created_ns) {
        result = core_memdb_stmt_bind_i64(&stmt, 8, created_ns);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 8);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (has_updated_ns) {
        result = core_memdb_stmt_bind_i64(&stmt, 9, updated_ns);
    } else {
        result = core_memdb_stmt_bind_null(&stmt, 9);
    }
    if (result.code != CORE_OK) {
        goto cleanup;
    }

    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_FORMAT, "item payload query returned no row" };
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

static CoreResult append_text_segment(char **buffer,
                                      size_t *length,
                                      size_t *capacity,
                                      const char *text,
                                      size_t text_len) {
    size_t required;
    size_t next_capacity;
    char *next_buffer;

    if (!buffer || !length || !capacity || (!text && text_len > 0u)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    required = *length + text_len + 1u;
    if (required > *capacity) {
        next_capacity = *capacity ? *capacity : 256u;
        while (next_capacity < required) {
            next_capacity *= 2u;
        }

        next_buffer = (char *)core_realloc(*buffer, next_capacity);
        if (!next_buffer) {
            return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        }

        *buffer = next_buffer;
        *capacity = next_capacity;
    }

    if (text_len > 0u) {
        memcpy(*buffer + *length, text, text_len);
        *length += text_len;
    }
    (*buffer)[*length] = '\0';
    return core_result_ok();
}

static CoreResult append_cstr(char **buffer,
                              size_t *length,
                              size_t *capacity,
                              const char *text) {
    if (!text) {
        text = "";
    }
    return append_text_segment(buffer, length, capacity, text, strlen(text));
}

static CoreResult append_i64_text(char **buffer,
                                  size_t *length,
                                  size_t *capacity,
                                  int64_t value) {
    char text[64];
    int written = snprintf(text, sizeof(text), "%lld", (long long)value);

    if (written <= 0 || (size_t)written >= sizeof(text)) {
        return (CoreResult){ CORE_ERR_FORMAT, "failed to format integer" };
    }

    return append_text_segment(buffer, length, capacity, text, (size_t)written);
}

static size_t build_compact_snippet(const char *text,
                                    size_t text_len,
                                    char *out_text,
                                    size_t out_cap,
                                    size_t max_chars) {
    size_t src_index = 0u;
    size_t out_len = 0u;
    int in_space = 1;
    int truncated = 0;

    if (!out_text || out_cap == 0u) {
        return 0u;
    }
    out_text[0] = '\0';

    if (!text || text_len == 0u) {
        return 0u;
    }

    if (max_chars == 0u || max_chars >= out_cap) {
        max_chars = out_cap - 1u;
    }

    while (src_index < text_len) {
        unsigned char ch = (unsigned char)text[src_index];
        int is_space = 0;

        if (ch == '\0') {
            break;
        }

        if (ch == '\r') {
            src_index += 1u;
            continue;
        }

        if (ch == '\n') {
            if (src_index + 1u < text_len && text[src_index + 1u] == '\n' && out_len >= 48u) {
                truncated = 1;
                break;
            }
            ch = ' ';
        } else if (ch == '\t' || ch == '\f' || ch == '\v') {
            ch = ' ';
        }

        is_space = (ch == ' ');
        if (is_space) {
            if (in_space || out_len == 0u) {
                src_index += 1u;
                continue;
            }
            if (out_len >= max_chars) {
                truncated = 1;
                break;
            }
            out_text[out_len++] = ' ';
            in_space = 1;
            src_index += 1u;
            continue;
        }

        if (ch < 32u) {
            src_index += 1u;
            continue;
        }

        if (out_len >= max_chars) {
            truncated = 1;
            break;
        }
        out_text[out_len++] = (char)ch;
        in_space = 0;
        src_index += 1u;
    }

    while (out_len > 0u && out_text[out_len - 1u] == ' ') {
        out_len -= 1u;
    }

    if (truncated && out_len > 0u) {
        if (out_len + 3u >= out_cap) {
            while (out_len > 0u && out_len + 3u >= out_cap) {
                out_len -= 1u;
            }
        }
        while (out_len > 0u && out_text[out_len - 1u] == ' ') {
            out_len -= 1u;
        }
        if (out_len > 0u && out_len + 3u < out_cap) {
            out_text[out_len++] = '.';
            out_text[out_len++] = '.';
            out_text[out_len++] = '.';
        }
    }

    out_text[out_len] = '\0';
    return out_len;
}

static CoreResult build_item_archive_event_payload_alloc(CoreMemDb *db,
                                                         int64_t item_id,
                                                         int64_t archived_ns,
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
                                "SELECT json_set("
                                "json_object("
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
                                "), "
                                "'$.archived_ns', ?2"
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
    result = core_memdb_stmt_bind_i64(&stmt, 2, archived_ns);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (!has_row) {
        result = (CoreResult){ CORE_ERR_NOT_FOUND, "item archive payload source not found" };
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

int cmd_add(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *title = find_flag_value(argc, argv, "--title");
    const char *body = find_flag_value(argc, argv, "--body");
    const char *stable_id = find_flag_value(argc, argv, "--stable-id");
    const char *workspace_key = find_flag_value(argc, argv, "--workspace");
    const char *project_key = find_flag_value(argc, argv, "--project");
    const char *item_kind = find_flag_value(argc, argv, "--kind");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    const char *session_max_writes_text = find_flag_value(argc, argv, "--session-max-writes");
    const char *event_type = 0;
    const char *event_stable_id = 0;
    const char *event_workspace_key = 0;
    const char *event_project_key = 0;
    const char *event_kind = 0;
    const char *payload_stable_id = 0;
    const char *payload_workspace_key = 0;
    const char *payload_project_key = 0;
    const char *payload_kind = 0;
    CoreMemDb db = {0};
    CoreMemStmt apply_stmt = {0};
    CoreResult result;
    char fingerprint[17];
    int64_t now_ns;
    int64_t item_id = 0;
    int64_t fallback_created_ns = 0;
    int64_t fallback_updated_ns = 0;
    int has_created_ns = 0;
    int has_updated_ns = 1;
    int has_row = 0;
    int duplicate_found = 0;
    int tx_started = 0;
    int exit_code = 1;
    int performed_update = 0;
    int enforce_session_budget = 0;
    int64_t session_max_writes = 0;
    int64_t session_write_count = 0;
    char detail[64];
    char existing_stable_id[128];
    char existing_workspace[128];
    char existing_project[128];
    char existing_kind[128];
    char stable_id_value[128];
    char workspace_value[128];
    char project_value[128];
    char kind_value[128];
    char *event_payload = 0;

    if (!db_path || !title || !body) {
        print_usage(argv[0]);
        return 1;
    }
    if (session_max_writes_text) {
        if (!parse_i64_arg(session_max_writes_text, &session_max_writes) || session_max_writes <= 0 || session_max_writes > 1000000) {
            fprintf(stderr, "add: --session-max-writes must be in range [1, 1000000]\n");
            return 1;
        }
        if (!session_id || session_id[0] == '\0') {
            fprintf(stderr, "add: --session-id is required when --session-max-writes is used\n");
            return 1;
        }
        enforce_session_budget = 1;
    }
    if (!build_fingerprint(title, body, fingerprint, sizeof(fingerprint))) {
        fprintf(stderr, "add: failed to build fingerprint\n");
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }
    if (enforce_session_budget) {
        result = fetch_session_add_write_count(&db, session_id, &session_write_count);
        if (result.code != CORE_OK) {
            print_core_error("add", result);
            goto cleanup;
        }
        if (session_write_count >= session_max_writes) {
            (void)append_audit_entry(&db,
                                     session_id,
                                     "add",
                                     "fail",
                                     0,
                                     stable_id,
                                     workspace_key,
                                     project_key,
                                     item_kind,
                                     "session write budget exceeded");
            fprintf(stderr,
                    "add: session write budget exceeded (session=%s used=%lld max=%lld)\n",
                    session_id,
                    (long long)session_write_count,
                    (long long)session_max_writes);
            goto cleanup;
        }
    }

    result = core_memdb_tx_begin(&db);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    tx_started = 1;
    now_ns = current_time_ns();
    fallback_created_ns = now_ns;
    fallback_updated_ns = now_ns;

    result = find_duplicate_item(&db, fingerprint, &item_id, &duplicate_found);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }

    if (duplicate_found) {
        performed_update = 1;
        result = fetch_item_audit_metadata(&db,
                                           item_id,
                                           existing_stable_id,
                                           sizeof(existing_stable_id),
                                           existing_workspace,
                                           sizeof(existing_workspace),
                                           existing_project,
                                           sizeof(existing_project),
                                           existing_kind,
                                           sizeof(existing_kind));
        if (result.code != CORE_OK) {
            print_core_error("add", result);
            goto cleanup;
        } else {
            event_stable_id = existing_stable_id[0] != '\0' ? existing_stable_id : 0;
            event_workspace_key = existing_workspace;
            event_project_key = existing_project;
            event_kind = existing_kind;

            if (stable_id && stable_id[0] != '\0' && existing_stable_id[0] == '\0') {
                payload_stable_id = stable_id;
                event_stable_id = stable_id;
            }
            if (workspace_key && workspace_key[0] != '\0') {
                payload_workspace_key = workspace_key;
                event_workspace_key = workspace_key;
            }
            if (project_key && project_key[0] != '\0') {
                payload_project_key = project_key;
                event_project_key = project_key;
            }
            if (item_kind && item_kind[0] != '\0') {
                payload_kind = item_kind;
                event_kind = item_kind;
            }
            fallback_created_ns = 0;
            event_type = "NodeBodyUpdated";
        }
    } else {
        result = fetch_next_item_id(&db, &item_id);
        if (result.code != CORE_OK) {
            print_core_error("add", result);
            goto cleanup;
        }
        payload_stable_id = (stable_id && stable_id[0] != '\0') ? stable_id : 0;
        payload_workspace_key = workspace_key;
        payload_project_key = project_key;
        payload_kind = item_kind;
        event_stable_id = payload_stable_id;
        event_workspace_key = workspace_key ? workspace_key : "";
        event_project_key = project_key ? project_key : "";
        event_kind = item_kind ? item_kind : "";
        has_created_ns = 1;
        event_type = "NodeCreated";
    }

    result = build_item_event_payload_alloc(&db,
                                            title,
                                            body,
                                            fingerprint,
                                            payload_stable_id,
                                            payload_workspace_key,
                                            payload_project_key,
                                            payload_kind,
                                            has_created_ns,
                                            now_ns,
                                            has_updated_ns,
                                            now_ns,
                                            &event_payload);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = append_event_entry(&db,
                                session_id,
                                event_type,
                                item_id,
                                0,
                                event_stable_id,
                                event_workspace_key,
                                event_project_key,
                                event_kind,
                                event_payload);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = core_memdb_prepare(&db, kApplyItemEventSql, &apply_stmt);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&apply_stmt, 1, item_id);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&apply_stmt, 2, event_payload);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&apply_stmt, 3, fallback_created_ns);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&apply_stmt, 4, fallback_updated_ns);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&apply_stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    if (has_row) {
        print_core_error("add", (CoreResult){ CORE_ERR_FORMAT, "apply returned unexpected row" });
        goto cleanup;
    }
    result = core_memdb_stmt_finalize(&apply_stmt);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }

    result = sync_fts_row(&db, item_id, title, body);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    result = fetch_item_audit_metadata(&db,
                                       item_id,
                                       stable_id_value,
                                       sizeof(stable_id_value),
                                       workspace_value,
                                       sizeof(workspace_value),
                                       project_value,
                                       sizeof(project_value),
                                       kind_value,
                                       sizeof(kind_value));
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    (void)snprintf(detail,
                   sizeof(detail),
                   "%s",
                   performed_update ? "updated duplicate fingerprint" : "inserted new item");
    result = append_audit_entry(&db,
                                session_id,
                                "add",
                                "ok",
                                item_id,
                                stable_id_value[0] != '\0' ? stable_id_value : 0,
                                workspace_value,
                                project_value,
                                kind_value,
                                detail);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }

    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) {
        print_core_error("add", result);
        goto cleanup;
    }
    tx_started = 0;

    if (performed_update) {
        printf("updated id=%lld\n", (long long)item_id);
    } else if (stable_id && stable_id[0] != '\0') {
        printf("added id=%lld stable_id=%s\n", (long long)item_id, stable_id);
    } else {
        printf("added id=%lld\n", (long long)item_id);
    }
    exit_code = 0;

cleanup:
    core_free(event_payload);
    (void)core_memdb_stmt_finalize(&apply_stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_batch_add(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *input_path = find_flag_value(argc, argv, "--input");
    const char *default_workspace = find_flag_value(argc, argv, "--workspace");
    const char *default_project = find_flag_value(argc, argv, "--project");
    const char *default_kind = find_flag_value(argc, argv, "--kind");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    const char *session_max_writes = find_flag_value(argc, argv, "--session-max-writes");
    const char *max_errors_text = find_flag_value(argc, argv, "--max-errors");
    const char *retry_attempts_text = find_flag_value(argc, argv, "--retry-attempts");
    const char *retry_delay_ms_text = find_flag_value(argc, argv, "--retry-delay-ms");
    int continue_on_error = has_flag(argc, argv, "--continue-on-error");
    FILE *input = 0;
    char line[32768];
    int loaded_count = 0;
    int ok_count = 0;
    int failed_count = 0;
    int64_t max_errors = 0;
    int64_t retry_attempts = 0;
    int64_t retry_delay_ms = 25;
    int stop_due_to_max_errors = 0;

    if (!db_path || !input_path) {
        print_usage(argv[0]);
        return 1;
    }
    if (max_errors_text && (!parse_i64_arg(max_errors_text, &max_errors) || max_errors <= 0 || max_errors > 1000000)) {
        fprintf(stderr, "batch-add: --max-errors must be in range [1, 1000000]\n");
        return 1;
    }
    if (retry_attempts_text && (!parse_i64_arg(retry_attempts_text, &retry_attempts) || retry_attempts < 0 || retry_attempts > 64)) {
        fprintf(stderr, "batch-add: --retry-attempts must be in range [0, 64]\n");
        return 1;
    }
    if (retry_delay_ms_text && (!parse_i64_arg(retry_delay_ms_text, &retry_delay_ms) || retry_delay_ms < 0 || retry_delay_ms > 60000)) {
        fprintf(stderr, "batch-add: --retry-delay-ms must be in range [0, 60000]\n");
        return 1;
    }
    if (max_errors > 0 && !continue_on_error) {
        fprintf(stderr, "batch-add: --max-errors requires --continue-on-error\n");
        return 1;
    }

    input = fopen(input_path, "r");
    if (!input) {
        fprintf(stderr, "batch-add: failed to open input: %s\n", input_path);
        return 1;
    }

    while (fgets(line, sizeof(line), input)) {
        char *fields[6] = {0};
        int field_count;
        size_t line_len = strlen(line);
        const char *stable_id = 0;
        const char *title = 0;
        const char *body = 0;
        const char *workspace_key = default_workspace;
        const char *project_key = default_project;
        const char *item_kind = default_kind;
        char *add_argv[32];
        int add_argc = 0;
        int add_rc = 1;
        int64_t attempt = 0;

        while (line_len > 0u && (line[line_len - 1u] == '\n' || line[line_len - 1u] == '\r')) {
            line[line_len - 1u] = '\0';
            line_len -= 1u;
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        field_count = parse_tsv_fields(line, fields, 6);
        if (field_count < 2) {
            fprintf(stderr, "batch-add: invalid row (need at least title/body columns)\n");
            failed_count += 1;
            if (continue_on_error && max_errors > 0 && failed_count >= max_errors) {
                stop_due_to_max_errors = 1;
                break;
            }
            if (!continue_on_error) {
                break;
            }
            continue;
        }

        if (field_count == 2) {
            title = fields[0];
            body = fields[1];
        } else {
            stable_id = fields[0];
            title = fields[1];
            body = fields[2];
            if (field_count >= 4 && fields[3][0] != '\0') {
                workspace_key = fields[3];
            }
            if (field_count >= 5 && fields[4][0] != '\0') {
                project_key = fields[4];
            }
            if (field_count >= 6 && fields[5][0] != '\0') {
                item_kind = fields[5];
            }
        }

        add_argv[add_argc++] = argv[0];
        add_argv[add_argc++] = "add";
        add_argv[add_argc++] = "--db";
        add_argv[add_argc++] = (char *)db_path;
        add_argv[add_argc++] = "--title";
        add_argv[add_argc++] = (char *)title;
        add_argv[add_argc++] = "--body";
        add_argv[add_argc++] = (char *)body;
        if (stable_id && stable_id[0] != '\0') {
            add_argv[add_argc++] = "--stable-id";
            add_argv[add_argc++] = (char *)stable_id;
        }
        if (workspace_key && workspace_key[0] != '\0') {
            add_argv[add_argc++] = "--workspace";
            add_argv[add_argc++] = (char *)workspace_key;
        }
        if (project_key && project_key[0] != '\0') {
            add_argv[add_argc++] = "--project";
            add_argv[add_argc++] = (char *)project_key;
        }
        if (item_kind && item_kind[0] != '\0') {
            add_argv[add_argc++] = "--kind";
            add_argv[add_argc++] = (char *)item_kind;
        }
        if (session_id && session_id[0] != '\0') {
            add_argv[add_argc++] = "--session-id";
            add_argv[add_argc++] = (char *)session_id;
        }
        if (session_max_writes && session_max_writes[0] != '\0') {
            add_argv[add_argc++] = "--session-max-writes";
            add_argv[add_argc++] = (char *)session_max_writes;
        }

        for (;;) {
            add_rc = cmd_add(add_argc, add_argv);
            if (add_rc == 0) {
                break;
            }
            if (attempt >= retry_attempts) {
                break;
            }
            attempt += 1;
            if (retry_delay_ms > 0) {
                sleep_for_ms(retry_delay_ms);
            }
        }
        loaded_count += 1;
        if (add_rc == 0) {
            ok_count += 1;
        } else {
            failed_count += 1;
            if (continue_on_error && max_errors > 0 && failed_count >= max_errors) {
                stop_due_to_max_errors = 1;
                break;
            }
            if (!continue_on_error) {
                break;
            }
        }
    }

    fclose(input);
    printf("batch-add: loaded=%d ok=%d failed=%d\n",
           loaded_count,
           ok_count,
           failed_count);
    if (stop_due_to_max_errors) {
        fprintf(stderr, "batch-add: stopped after reaching --max-errors=%lld\n", (long long)max_errors);
    }
    return failed_count == 0 ? 0 : 1;
}

int cmd_set_bool_field(int argc,
                       char **argv,
                       const char *command_name,
                       const char *field_name) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *id_text = find_flag_value(argc, argv, "--id");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    int set_on = has_flag(argc, argv, "--on");
    int set_off = has_flag(argc, argv, "--off");
    int64_t item_id = 0;
    int64_t changed_rows = 0;
    int64_t now_ns = 0;
    char stable_id[128];
    char workspace_key[128];
    char project_key[128];
    char item_kind[128];
    char detail[96];
    char event_payload[128];
    const char *event_type = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreResult result;
    int tx_started = 0;
    int item_active = 0;
    int has_row = 0;
    int exit_code = 1;
    char sql[256];

    if (!db_path || !id_text || !parse_i64_arg(id_text, &item_id) || set_on == set_off) {
        print_usage(argv[0]);
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    now_ns = current_time_ns();
    result = core_memdb_tx_begin(&db);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    tx_started = 1;

    result = item_exists_active(&db, item_id, &item_active);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    if (!item_active) {
        fprintf(stderr, "%s: id %lld not found\n", command_name, (long long)item_id);
        goto cleanup;
    }
    result = fetch_item_audit_metadata(&db,
                                       item_id,
                                       stable_id,
                                       sizeof(stable_id),
                                       workspace_key,
                                       sizeof(workspace_key),
                                       project_key,
                                       sizeof(project_key),
                                       item_kind,
                                       sizeof(item_kind));
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    event_type = strcmp(field_name, "pinned") == 0 ? "NodePinnedSet" : "NodeCanonicalSet";
    (void)snprintf(event_payload,
                   sizeof(event_payload),
                   "{\"field\":\"%s\",\"value\":%d,\"updated_ns\":%lld}",
                   field_name,
                   set_on ? 1 : 0,
                   (long long)now_ns);
    result = append_event_entry(&db,
                                session_id,
                                event_type,
                                item_id,
                                0,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                event_payload);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    (void)snprintf(sql,
                   sizeof(sql),
                   "UPDATE mem_item "
                   "SET %s = COALESCE(CAST(json_extract(?1, '$.value') AS INTEGER), %s), "
                   "updated_ns = COALESCE(CAST(json_extract(?1, '$.updated_ns') AS INTEGER), updated_ns) "
                   "WHERE id = ?2 AND archived_ns IS NULL;",
                   field_name,
                   field_name);
    result = core_memdb_prepare(&db, sql, &stmt);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&stmt, 1, event_payload);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 2, item_id);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    if (has_row) {
        print_core_error(command_name, (CoreResult){ CORE_ERR_FORMAT, "update returned unexpected row" });
        goto cleanup;
    }
    result = core_memdb_stmt_finalize(&stmt);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    result = fetch_changes(&db, &changed_rows);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    if (changed_rows == 0) {
        fprintf(stderr, "%s: id %lld not found\n", command_name, (long long)item_id);
        goto cleanup;
    }
    (void)snprintf(detail, sizeof(detail), "%s=%s", field_name, set_on ? "1" : "0");
    result = append_audit_entry(&db,
                                session_id,
                                command_name,
                                "ok",
                                item_id,
                                stable_id[0] != '\0' ? stable_id : 0,
                                workspace_key,
                                project_key,
                                item_kind,
                                detail);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) {
        print_core_error(command_name, result);
        goto cleanup;
    }
    tx_started = 0;

    printf("%s id=%lld %s\n",
           command_name,
           (long long)item_id,
           set_on ? "on" : "off");
    exit_code = 0;

cleanup:
    (void)core_memdb_stmt_finalize(&stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    (void)core_memdb_close(&db);
    return exit_code;
}

int cmd_rollup(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *before_text = find_flag_value(argc, argv, "--before");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    const char *workspace_filter = find_flag_value(argc, argv, "--workspace");
    const char *project_filter = find_flag_value(argc, argv, "--project");
    const char *kind_filter = find_flag_value(argc, argv, "--kind");
    const char *limit_text = find_flag_value(argc, argv, "--limit");
    int64_t cutoff_ns = 0;
    int64_t rollup_limit = 0;
    int64_t now_ns = 0;
    int64_t rollup_id = 0;
    int64_t rolled_count = 0;
    int64_t archive_count = 0;
    int64_t hidden_summary_items = 0;
    int has_row = 0;
    int tx_started = 0;
    int exit_code = 1;
    int summary_visible_items = 0;
    const size_t summary_char_budget = 2400u;
    char title[128];
    char fingerprint[17];
    char detail[128];
    char stable_id[128];
    char workspace_key[128];
    char project_key[128];
    char item_kind[128];
    int64_t *rolled_item_ids = 0;
    size_t rolled_item_count = 0u;
    size_t rolled_item_cap = 0u;
    char *event_payload = 0;
    char *summary_body = 0;
    char *summary_paragraph = 0;
    char *coverage_lines = 0;
    size_t summary_len = 0u;
    size_t summary_cap = 0u;
    size_t summary_para_len = 0u;
    size_t summary_para_cap = 0u;
    size_t coverage_len = 0u;
    size_t coverage_cap = 0u;
    char query_sql[1024];
    char sql_fragment[96];
    int bind_index = 0;
    CoreMemDb db = {0};
    CoreMemStmt stmt = {0};
    CoreMemStmt apply_stmt = {0};
    CoreResult result;

    if (!db_path || !before_text || !parse_i64_arg(before_text, &cutoff_ns)) {
        print_usage(argv[0]);
        return 1;
    }
    if (limit_text && (!parse_i64_arg(limit_text, &rollup_limit) || rollup_limit <= 0)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) {
        return 1;
    }

    result = core_memdb_tx_begin(&db);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    tx_started = 1;

    result = append_cstr(&summary_body, &summary_len, &summary_cap, "Rollup cutoff_ns: ");
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = append_i64_text(&summary_body, &summary_len, &summary_cap, cutoff_ns);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n\n");
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    if (workspace_filter && workspace_filter[0] != '\0') {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "scope workspace: ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, workspace_filter);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    }
    if (project_filter && project_filter[0] != '\0') {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "scope project: ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, project_filter);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    }
    if (kind_filter && kind_filter[0] != '\0') {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "scope kind: ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, kind_filter);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    } else {
        result = append_cstr(&summary_body,
                             &summary_len,
                             &summary_cap,
                             "scope kind excluded: rollup (default)\n");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    }
    if (rollup_limit > 0) {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "rollup limit: ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_i64_text(&summary_body, &summary_len, &summary_cap, rollup_limit);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    }
    result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n");
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    query_sql[0] = '\0';
    if (!append_sql_fragment(query_sql,
                             sizeof(query_sql),
                             "SELECT id, title, body "
                             "FROM mem_item "
                             "WHERE updated_ns < ?1 "
                             "AND pinned = 0 "
                             "AND canonical = 0 "
                             "AND archived_ns IS NULL")) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup query buffer overflow" });
        goto cleanup;
    }
    if ((!kind_filter || kind_filter[0] == '\0') &&
        !append_sql_fragment(query_sql, sizeof(query_sql), " AND kind <> 'rollup'")) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup default kind exclusion overflow" });
        goto cleanup;
    }

    bind_index = 2;
    if (workspace_filter && workspace_filter[0] != '\0') {
        if (snprintf(sql_fragment, sizeof(sql_fragment), " AND workspace_key = ?%d", bind_index) <= 0 ||
            !append_sql_fragment(query_sql, sizeof(query_sql), sql_fragment)) {
            print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup workspace filter overflow" });
            goto cleanup;
        }
        bind_index += 1;
    }
    if (project_filter && project_filter[0] != '\0') {
        if (snprintf(sql_fragment, sizeof(sql_fragment), " AND project_key = ?%d", bind_index) <= 0 ||
            !append_sql_fragment(query_sql, sizeof(query_sql), sql_fragment)) {
            print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup project filter overflow" });
            goto cleanup;
        }
        bind_index += 1;
    }
    if (kind_filter && kind_filter[0] != '\0') {
        if (snprintf(sql_fragment, sizeof(sql_fragment), " AND kind = ?%d", bind_index) <= 0 ||
            !append_sql_fragment(query_sql, sizeof(query_sql), sql_fragment)) {
            print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup kind filter overflow" });
            goto cleanup;
        }
        bind_index += 1;
    }

    if (!append_sql_fragment(query_sql, sizeof(query_sql), " ORDER BY updated_ns ASC, id ASC")) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup query overflow" });
        goto cleanup;
    }
    if (rollup_limit > 0) {
        if (snprintf(sql_fragment, sizeof(sql_fragment), " LIMIT ?%d", bind_index) <= 0 ||
            !append_sql_fragment(query_sql, sizeof(query_sql), sql_fragment)) {
            print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup limit overflow" });
            goto cleanup;
        }
    }
    if (!append_sql_fragment(query_sql, sizeof(query_sql), ";")) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup query overflow" });
        goto cleanup;
    }

    result = core_memdb_prepare(&db, query_sql, &stmt);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, cutoff_ns);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    bind_index = 2;
    if (workspace_filter && workspace_filter[0] != '\0') {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, workspace_filter);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    if (project_filter && project_filter[0] != '\0') {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, project_filter);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    if (kind_filter && kind_filter[0] != '\0') {
        result = core_memdb_stmt_bind_text(&stmt, bind_index, kind_filter);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        bind_index += 1;
    }
    if (rollup_limit > 0) {
        result = core_memdb_stmt_bind_i64(&stmt, bind_index, rollup_limit);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    }

    for (;;) {
        int64_t item_id = 0;
        CoreStr item_title = {0};
        CoreStr item_body = {0};
        char compact_title[112];
        char compact_body[320];
        char compact_body_coverage[168];
        char clause[640];
        int clause_written = 0;
        size_t clause_len = 0u;

        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        if (!has_row) {
            break;
        }

        result = core_memdb_stmt_column_i64(&stmt, 0, &item_id);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 1, &item_title);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = core_memdb_stmt_column_text(&stmt, 2, &item_body);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }

        build_compact_snippet(item_title.data,
                              item_title.len,
                              compact_title,
                              sizeof(compact_title),
                              96u);
        if (compact_title[0] == '\0') {
            (void)snprintf(compact_title, sizeof(compact_title), "(untitled)");
        }
        build_compact_snippet(item_body.data,
                              item_body.len,
                              compact_body,
                              sizeof(compact_body),
                              240u);
        if (compact_body[0] == '\0') {
            (void)snprintf(compact_body, sizeof(compact_body), "No body text.");
        }
        build_compact_snippet(item_body.data,
                              item_body.len,
                              compact_body_coverage,
                              sizeof(compact_body_coverage),
                              120u);
        if (compact_body_coverage[0] == '\0') {
            (void)snprintf(compact_body_coverage, sizeof(compact_body_coverage), "No body text.");
        }

        clause_written = snprintf(clause,
                                  sizeof(clause),
                                  "[%lld] %s: %s",
                                  (long long)item_id,
                                  compact_title,
                                  compact_body);
        if (clause_written <= 0 || (size_t)clause_written >= sizeof(clause)) {
            print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "failed to format rollup clause" });
            goto cleanup;
        }
        clause_len = (size_t)clause_written;
        if (summary_para_len + (summary_visible_items > 0 ? 2u : 0u) + clause_len <= summary_char_budget) {
            if (summary_visible_items > 0) {
                result = append_cstr(&summary_paragraph, &summary_para_len, &summary_para_cap, "; ");
                if (result.code != CORE_OK) {
                    print_core_error("rollup", result);
                    goto cleanup;
                }
            }
            result = append_text_segment(&summary_paragraph,
                                         &summary_para_len,
                                         &summary_para_cap,
                                         clause,
                                         clause_len);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            summary_visible_items += 1;
        } else {
            hidden_summary_items += 1;
        }

        result = append_cstr(&coverage_lines, &coverage_len, &coverage_cap, "- item ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_i64_text(&coverage_lines, &coverage_len, &coverage_cap, item_id);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&coverage_lines, &coverage_len, &coverage_cap, " | ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&coverage_lines, &coverage_len, &coverage_cap, compact_title);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&coverage_lines, &coverage_len, &coverage_cap, " | ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&coverage_lines, &coverage_len, &coverage_cap, compact_body_coverage);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&coverage_lines, &coverage_len, &coverage_cap, "\n");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }

        if (rolled_item_count == rolled_item_cap) {
            size_t next_cap = rolled_item_cap == 0u ? 32u : rolled_item_cap * 2u;
            int64_t *next_ids = (int64_t *)core_realloc(rolled_item_ids, next_cap * sizeof(int64_t));
            if (!next_ids) {
                print_core_error("rollup", (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" });
                goto cleanup;
            }
            rolled_item_ids = next_ids;
            rolled_item_cap = next_cap;
        }
        rolled_item_ids[rolled_item_count++] = item_id;

        rolled_count += 1;
    }

    result = core_memdb_stmt_finalize(&stmt);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    if (rolled_count == 0) {
        (void)core_memdb_tx_rollback(&db);
        tx_started = 0;
        printf("rollup: no eligible rows before %lld\n", (long long)cutoff_ns);
        exit_code = 0;
        goto cleanup;
    }

    result = append_cstr(&summary_body, &summary_len, &summary_cap, "merged items: ");
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = append_i64_text(&summary_body, &summary_len, &summary_cap, rolled_count);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n\nRollup summary:\n");
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    if (summary_paragraph && summary_para_len > 0u) {
        result = append_text_segment(&summary_body,
                                     &summary_len,
                                     &summary_cap,
                                     summary_paragraph,
                                     summary_para_len);
    } else {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "No synthesized summary available.");
    }
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    if (hidden_summary_items > 0) {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "; plus ");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_i64_text(&summary_body, &summary_len, &summary_cap, hidden_summary_items);
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
        result = append_cstr(&summary_body,
                             &summary_len,
                             &summary_cap,
                             " additional items (see coverage list).");
        if (result.code != CORE_OK) {
            print_core_error("rollup", result);
            goto cleanup;
        }
    }
    result = append_cstr(&summary_body, &summary_len, &summary_cap, "\n\nCoverage list:\n");
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    if (coverage_lines && coverage_len > 0u) {
        result = append_text_segment(&summary_body,
                                     &summary_len,
                                     &summary_cap,
                                     coverage_lines,
                                     coverage_len);
    } else {
        result = append_cstr(&summary_body, &summary_len, &summary_cap, "- none\n");
    }
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    if (snprintf(title,
                 sizeof(title),
                 "Rollup before %lld (%lld items)",
                 (long long)cutoff_ns,
                 (long long)rolled_count) <= 0) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "failed to format rollup title" });
        goto cleanup;
    }
    if (!build_fingerprint(title, summary_body, fingerprint, sizeof(fingerprint))) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "failed to build rollup fingerprint" });
        goto cleanup;
    }

    now_ns = current_time_ns();
    result = fetch_next_item_id(&db, &rollup_id);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = build_item_event_payload_alloc(&db,
                                            title,
                                            summary_body,
                                            fingerprint,
                                            0,
                                            (workspace_filter && workspace_filter[0] != '\0') ? workspace_filter : "",
                                            (project_filter && project_filter[0] != '\0') ? project_filter : "",
                                            (kind_filter && kind_filter[0] != '\0') ? kind_filter : "rollup",
                                            1,
                                            now_ns,
                                            1,
                                            now_ns,
                                            &event_payload);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = append_event_entry(&db,
                                session_id,
                                "NodeMerged",
                                rollup_id,
                                0,
                                0,
                                (workspace_filter && workspace_filter[0] != '\0') ? workspace_filter : "",
                                (project_filter && project_filter[0] != '\0') ? project_filter : "",
                                (kind_filter && kind_filter[0] != '\0') ? kind_filter : "rollup",
                                event_payload);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    result = core_memdb_prepare(&db, kApplyItemEventSql, &apply_stmt);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&apply_stmt, 1, rollup_id);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_text(&apply_stmt, 2, event_payload);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&apply_stmt, 3, now_ns);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = core_memdb_stmt_bind_i64(&apply_stmt, 4, now_ns);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    result = core_memdb_stmt_step(&apply_stmt, &has_row);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    if (has_row) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup apply returned unexpected row" });
        goto cleanup;
    }
    result = core_memdb_stmt_reset(&apply_stmt);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    core_free(event_payload);
    event_payload = 0;

    result = sync_fts_row(&db, rollup_id, title, summary_body);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    {
        size_t i;
        for (i = 0u; i < rolled_item_count; ++i) {
            int64_t archived_item_id = rolled_item_ids[i];
            int64_t changed_rows = 0;

            result = fetch_item_audit_metadata(&db,
                                               archived_item_id,
                                               stable_id,
                                               sizeof(stable_id),
                                               workspace_key,
                                               sizeof(workspace_key),
                                               project_key,
                                               sizeof(project_key),
                                               item_kind,
                                               sizeof(item_kind));
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }

            result = build_item_archive_event_payload_alloc(&db, archived_item_id, now_ns, &event_payload);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            result = append_event_entry(&db,
                                        session_id,
                                        "NodeMetadataPatched",
                                        archived_item_id,
                                        0,
                                        stable_id[0] != '\0' ? stable_id : 0,
                                        workspace_key,
                                        project_key,
                                        item_kind,
                                        event_payload);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }

            result = core_memdb_stmt_bind_i64(&apply_stmt, 1, archived_item_id);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_text(&apply_stmt, 2, event_payload);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_i64(&apply_stmt, 3, 0);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            result = core_memdb_stmt_bind_i64(&apply_stmt, 4, 0);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            result = core_memdb_stmt_step(&apply_stmt, &has_row);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            if (has_row) {
                print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup archive apply returned unexpected row" });
                goto cleanup;
            }
            result = core_memdb_stmt_reset(&apply_stmt);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            result = fetch_changes(&db, &changed_rows);
            if (result.code != CORE_OK) {
                print_core_error("rollup", result);
                goto cleanup;
            }
            if (changed_rows != 1) {
                print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup archive apply changed unexpected row count" });
                goto cleanup;
            }
            archive_count += 1;
            core_free(event_payload);
            event_payload = 0;
        }
    }

    if (archive_count != rolled_count) {
        print_core_error("rollup", (CoreResult){ CORE_ERR_FORMAT, "rollup archive count mismatch" });
        goto cleanup;
    }
    result = fetch_item_audit_metadata(&db,
                                       rollup_id,
                                       stable_id,
                                       sizeof(stable_id),
                                       workspace_key,
                                       sizeof(workspace_key),
                                       project_key,
                                       sizeof(project_key),
                                       item_kind,
                                       sizeof(item_kind));
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    (void)snprintf(detail,
                   sizeof(detail),
                   "rolled=%lld archived=%lld cutoff_ns=%lld",
                   (long long)rolled_count,
                   (long long)archive_count,
                   (long long)cutoff_ns);
    result = append_audit_entry(&db,
                                session_id,
                                "rollup",
                                "ok",
                                rollup_id,
                                0,
                                "",
                                "",
                                "rollup",
                                detail);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }

    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) {
        print_core_error("rollup", result);
        goto cleanup;
    }
    tx_started = 0;

    printf("rolled up %lld rows into id=%lld\n",
           (long long)rolled_count,
           (long long)rollup_id);
    exit_code = 0;

cleanup:
    core_free(event_payload);
    core_free(rolled_item_ids);
    core_free(summary_paragraph);
    core_free(coverage_lines);
    (void)core_memdb_stmt_finalize(&apply_stmt);
    (void)core_memdb_stmt_finalize(&stmt);
    if (tx_started) {
        (void)core_memdb_tx_rollback(&db);
    }
    core_free(summary_body);
    (void)core_memdb_close(&db);
    return exit_code;
}
