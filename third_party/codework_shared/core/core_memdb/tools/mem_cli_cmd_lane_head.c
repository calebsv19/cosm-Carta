#include "mem_cli_cmd_lane_head.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_memdb.h"
#include "mem_cli_args.h"
#include "mem_cli_db_helpers.h"

#define LANE_HEAD_MAX_BODY_BYTES 900u
#define LANE_HEAD_MAX_MANAGED_LINKS 64

static const char *kApplyItemEventSql =
    "INSERT INTO mem_item ("
    "id, stable_id, title, body, fingerprint, workspace_key, project_key, kind, "
    "created_ns, updated_ns, pinned, canonical, ttl_until_ns, archived_ns"
    ") VALUES ("
    "?1, NULLIF(COALESCE(json_extract(?2, '$.stable_id'), ''), ''), "
    "COALESCE(json_extract(?2, '$.title'), ''), "
    "COALESCE(json_extract(?2, '$.body'), ''), "
    "COALESCE(json_extract(?2, '$.fingerprint'), ''), "
    "COALESCE(json_extract(?2, '$.workspace_key'), ''), "
    "COALESCE(json_extract(?2, '$.project_key'), ''), "
    "COALESCE(json_extract(?2, '$.kind'), ''), "
    "CAST(json_extract(?2, '$.created_ns') AS INTEGER), "
    "CAST(json_extract(?2, '$.updated_ns') AS INTEGER), "
    "COALESCE(CAST(json_extract(?2, '$.pinned') AS INTEGER), 0), "
    "COALESCE(CAST(json_extract(?2, '$.canonical') AS INTEGER), 0), "
    "CAST(json_extract(?2, '$.ttl_until_ns') AS INTEGER), "
    "CAST(json_extract(?2, '$.archived_ns') AS INTEGER)"
    ") ON CONFLICT(id) DO UPDATE SET "
    "title=excluded.title, body=excluded.body, fingerprint=excluded.fingerprint, "
    "workspace_key=excluded.workspace_key, project_key=excluded.project_key, kind=excluded.kind, "
    "updated_ns=excluded.updated_ns, pinned=excluded.pinned, canonical=excluded.canonical, "
    "ttl_until_ns=excluded.ttl_until_ns, archived_ns=excluded.archived_ns "
    "WHERE mem_item.stable_id=excluded.stable_id AND mem_item.archived_ns IS NULL;";

static const char *kApplyLinkEventSql =
    "INSERT INTO mem_link (id, from_item_id, to_item_id, kind, weight, note) "
    "VALUES (?1, CAST(json_extract(?2, '$.from_item_id') AS INTEGER), "
    "CAST(json_extract(?2, '$.to_item_id') AS INTEGER), "
    "COALESCE(json_extract(?2, '$.kind'), ''), "
    "CAST(json_extract(?2, '$.weight') AS REAL), json_extract(?2, '$.note'));";

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

static CoreResult copy_json_result(CoreMemStmt *stmt, char **out_json) {
    CoreResult result;
    CoreStr json_text = {0};
    int has_row = 0;
    char *buffer;

    if (!stmt || !out_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_json = 0;
    result = core_memdb_stmt_step(stmt, &has_row);
    if (result.code != CORE_OK) {
        return result;
    }
    if (!has_row) {
        return (CoreResult){ CORE_ERR_NOT_FOUND, "json source row not found" };
    }
    result = core_memdb_stmt_column_text(stmt, 0, &json_text);
    if (result.code != CORE_OK) {
        return result;
    }
    buffer = (char *)core_alloc(json_text.len + 1u);
    if (!buffer) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memcpy(buffer, json_text.data, json_text.len);
    buffer[json_text.len] = '\0';
    *out_json = buffer;
    return core_result_ok();
}

static CoreResult find_head(CoreMemDb *db,
                            const char *stable_id,
                            int64_t *out_item_id,
                            int *out_found,
                            char *out_workspace,
                            size_t workspace_cap,
                            char *out_project,
                            size_t project_cap,
                            char *out_kind,
                            size_t kind_cap) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    CoreStr workspace = {0};
    CoreStr project = {0};
    CoreStr kind = {0};

    *out_item_id = 0;
    *out_found = 0;
    out_workspace[0] = '\0';
    out_project[0] = '\0';
    out_kind[0] = '\0';
    result = core_memdb_prepare(db,
                                "SELECT id, workspace_key, project_key, kind FROM mem_item "
                                "WHERE stable_id=?1 AND archived_ns IS NULL LIMIT 1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_text(&stmt, 1, stable_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code != CORE_OK || !has_row) {
        if (result.code == CORE_OK) {
            result = core_result_ok();
        }
        goto cleanup;
    }
    result = core_memdb_stmt_column_i64(&stmt, 0, out_item_id);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 1, &workspace);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 2, &project);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    result = core_memdb_stmt_column_text(&stmt, 3, &kind);
    if (result.code != CORE_OK) {
        goto cleanup;
    }
    if (workspace.len >= workspace_cap || project.len >= project_cap || kind.len >= kind_cap) {
        result = (CoreResult){ CORE_ERR_FORMAT, "lane-head scope metadata exceeds supported length" };
        goto cleanup;
    }
    memcpy(out_workspace, workspace.data, workspace.len);
    out_workspace[workspace.len] = '\0';
    memcpy(out_project, project.data, project.len);
    out_project[project.len] = '\0';
    memcpy(out_kind, kind.data, kind.len);
    out_kind[kind.len] = '\0';
    *out_found = 1;
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

static CoreResult scoped_item_exists(CoreMemDb *db,
                                     int64_t item_id,
                                     const char *workspace,
                                     const char *project,
                                     int *out_exists) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    *out_exists = 0;
    result = core_memdb_prepare(db,
                                "SELECT 1 FROM mem_item WHERE id=?1 AND archived_ns IS NULL "
                                "AND workspace_key=?2 AND project_key=?3 LIMIT 1;",
                                &stmt);
    if (result.code != CORE_OK) {
        return result;
    }
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, workspace);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, project);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK) *out_exists = has_row ? 1 : 0;
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult item_matches(CoreMemDb *db,
                               int64_t item_id,
                               const char *stable_id,
                               const char *title,
                               const char *body,
                               const char *fingerprint,
                               const char *workspace,
                               const char *project,
                               int *out_matches) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    *out_matches = 0;
    result = core_memdb_prepare(db,
                                "SELECT 1 FROM mem_item WHERE id=?1 AND stable_id=?2 AND title=?3 "
                                "AND body=?4 AND fingerprint=?5 AND workspace_key=?6 AND project_key=?7 "
                                "AND kind='summary' AND canonical=1 AND archived_ns IS NULL LIMIT 1;",
                                &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, stable_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, title);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 4, body);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 5, fingerprint);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 6, workspace);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 7, project);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK) *out_matches = has_row ? 1 : 0;
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult build_item_payload(CoreMemDb *db,
                                     int exists,
                                     int64_t item_id,
                                     const char *stable_id,
                                     const char *title,
                                     const char *body,
                                     const char *fingerprint,
                                     const char *workspace,
                                     const char *project,
                                     int64_t now_ns,
                                     char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    const char *sql_existing =
        "SELECT json_object('stable_id',?2,'title',?3,'body',?4,'fingerprint',?5,"
        "'workspace_key',?6,'project_key',?7,'kind','summary','created_ns',created_ns,"
        "'updated_ns',?8,'pinned',pinned,'canonical',1,'ttl_until_ns',ttl_until_ns,"
        "'archived_ns',archived_ns) FROM mem_item WHERE id=?1;";
    const char *sql_new =
        "SELECT json_object('stable_id',?2,'title',?3,'body',?4,'fingerprint',?5,"
        "'workspace_key',?6,'project_key',?7,'kind','summary','created_ns',?8,"
        "'updated_ns',?8,'pinned',0,'canonical',1,'ttl_until_ns',NULL,'archived_ns',NULL);";

    result = core_memdb_prepare(db, exists ? sql_existing : sql_new, &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, stable_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, title);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 4, body);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 5, fingerprint);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 6, workspace);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 7, project);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 8, now_ns);
    if (result.code == CORE_OK) result = copy_json_result(&stmt, out_json);
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult apply_item_payload(CoreMemDb *db, int64_t item_id, const char *payload) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    result = core_memdb_prepare(db, kApplyItemEventSql, &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, item_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, payload);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK && has_row) result = (CoreResult){ CORE_ERR_FORMAT, "item apply returned a row" };
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult link_exact_exists(CoreMemDb *db,
                                    int64_t head_id,
                                    int64_t target_id,
                                    const char *kind,
                                    const char *note,
                                    int *out_exists) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    *out_exists = 0;
    result = core_memdb_prepare(db,
                                "SELECT 1 FROM mem_link WHERE from_item_id=?1 AND to_item_id=?2 "
                                "AND kind=?3 AND COALESCE(note,'')=?4 LIMIT 1;",
                                &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, head_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 2, target_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, kind);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 4, note);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK) *out_exists = has_row ? 1 : 0;
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult link_conflict_exists(CoreMemDb *db,
                                       int64_t head_id,
                                       int64_t target_id,
                                       const char *kind,
                                       const char *note,
                                       int *out_exists) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    *out_exists = 0;
    result = core_memdb_prepare(db,
                                "SELECT 1 FROM mem_link WHERE from_item_id=?1 AND to_item_id=?2 "
                                "AND kind=?3 AND COALESCE(note,'')<>?4 LIMIT 1;",
                                &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, head_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 2, target_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, kind);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 4, note);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK) *out_exists = has_row ? 1 : 0;
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult collect_stale_links(CoreMemDb *db,
                                      int64_t head_id,
                                      int64_t desired_target,
                                      const char *kind,
                                      const char *note,
                                      int64_t *out_ids,
                                      int *out_count) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    *out_count = 0;
    result = core_memdb_prepare(db,
                                "SELECT id FROM mem_link WHERE from_item_id=?1 AND kind=?2 "
                                "AND COALESCE(note,'')=?3 AND to_item_id<>?4 ORDER BY id;",
                                &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, head_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, kind);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, note);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 4, desired_target);
    while (result.code == CORE_OK) {
        result = core_memdb_stmt_step(&stmt, &has_row);
        if (result.code != CORE_OK || !has_row) break;
        if (*out_count >= LANE_HEAD_MAX_MANAGED_LINKS) {
            result = (CoreResult){ CORE_ERR_FORMAT, "too many managed lane-head links" };
            break;
        }
        result = core_memdb_stmt_column_i64(&stmt, 0, &out_ids[*out_count]);
        if (result.code == CORE_OK) *out_count += 1;
    }
    if (result.code == CORE_OK && !has_row) result = core_result_ok();
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult find_competing_head(CoreMemDb *db,
                                      const char *workspace,
                                      const char *project,
                                      const char *stable_id,
                                      int64_t anchor_id,
                                      int64_t latest_id,
                                      int64_t *out_head_id,
                                      char *out_stable_id,
                                      size_t out_stable_id_size,
                                      int *out_found) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    *out_found = 0;
    *out_head_id = 0;
    if (out_stable_id_size > 0u) out_stable_id[0] = '\0';
    result = core_memdb_prepare(
        db,
        "SELECT h.id,h.stable_id FROM mem_item h "
        "WHERE h.workspace_key=?1 AND h.project_key=?2 AND h.kind='summary' "
        "AND h.canonical=1 AND h.archived_ns IS NULL AND h.stable_id<>?3 "
        "AND h.stable_id LIKE 'lane-head-' || ?2 || '-%' "
        "AND EXISTS (SELECT 1 FROM mem_link a WHERE a.from_item_id=h.id "
        "AND a.to_item_id=?4 AND a.kind='references' "
        "AND COALESCE(a.note,'') LIKE 'lane-head-v1:anchor:%') "
        "AND EXISTS (SELECT 1 FROM mem_link l WHERE l.from_item_id=h.id "
        "AND l.to_item_id=?5 AND l.kind='summarizes' "
        "AND COALESCE(l.note,'') LIKE 'lane-head-v1:latest:%') "
        "ORDER BY h.id LIMIT 1;",
        &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_text(&stmt, 1, workspace);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, project);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, stable_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 4, anchor_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 5, latest_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK && has_row) {
        CoreStr competing_stable = {0};
        result = core_memdb_stmt_column_i64(&stmt, 0, out_head_id);
        if (result.code == CORE_OK) result = core_memdb_stmt_column_text(&stmt, 1, &competing_stable);
        if (result.code == CORE_OK && competing_stable.len >= out_stable_id_size) {
            result = (CoreResult){ CORE_ERR_FORMAT, "competing lane-head stable id is too long" };
        }
        if (result.code == CORE_OK) {
            memcpy(out_stable_id, competing_stable.data, competing_stable.len);
            out_stable_id[competing_stable.len] = '\0';
        }
        if (result.code == CORE_OK) *out_found = 1;
    }
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult build_link_payload(CoreMemDb *db,
                                     int64_t from_id,
                                     int64_t to_id,
                                     const char *kind,
                                     const char *note,
                                     char **out_json) {
    CoreMemStmt stmt = {0};
    CoreResult result;

    result = core_memdb_prepare(db,
                                "SELECT json_object('from_item_id',?1,'to_item_id',?2,"
                                "'kind',?3,'weight',NULL,'note',?4);",
                                &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, from_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 2, to_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 3, kind);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 4, note);
    if (result.code == CORE_OK) result = copy_json_result(&stmt, out_json);
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult apply_link_payload(CoreMemDb *db, int64_t link_id, const char *payload) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;

    result = core_memdb_prepare(db, kApplyLinkEventSql, &stmt);
    if (result.code != CORE_OK) return result;
    result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_text(&stmt, 2, payload);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK && has_row) result = (CoreResult){ CORE_ERR_FORMAT, "link apply returned a row" };
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult remove_link(CoreMemDb *db,
                              int64_t link_id,
                              int64_t head_id,
                              const char *session_id,
                              const char *stable_id,
                              const char *workspace,
                              const char *project) {
    CoreMemStmt stmt = {0};
    CoreResult result;
    int has_row = 0;
    char *payload = 0;

    result = fetch_link_snapshot_json_alloc(db, link_id, &payload);
    if (result.code == CORE_OK) {
        result = append_event_entry(db, session_id, "EdgeRemoved", head_id, link_id,
                                    stable_id, workspace, project, "summary", payload);
    }
    if (result.code == CORE_OK) result = core_memdb_prepare(db, "DELETE FROM mem_link WHERE id=?1;", &stmt);
    if (result.code == CORE_OK) result = core_memdb_stmt_bind_i64(&stmt, 1, link_id);
    if (result.code == CORE_OK) result = core_memdb_stmt_step(&stmt, &has_row);
    if (result.code == CORE_OK && has_row) result = (CoreResult){ CORE_ERR_FORMAT, "link delete returned a row" };
    core_free(payload);
    {
        CoreResult finalize_result = core_memdb_stmt_finalize(&stmt);
        if (result.code == CORE_OK && finalize_result.code != CORE_OK) result = finalize_result;
    }
    return result;
}

static CoreResult add_link(CoreMemDb *db,
                           int64_t head_id,
                           int64_t target_id,
                           const char *kind,
                           const char *note,
                           const char *session_id,
                           const char *stable_id,
                           const char *workspace,
                           const char *project) {
    CoreResult result;
    int64_t link_id = 0;
    char *payload = 0;

    result = fetch_next_link_id(db, &link_id);
    if (result.code == CORE_OK) result = build_link_payload(db, head_id, target_id, kind, note, &payload);
    if (result.code == CORE_OK) {
        result = append_event_entry(db, session_id, "EdgeAdded", head_id, link_id,
                                    stable_id, workspace, project, "summary", payload);
    }
    if (result.code == CORE_OK) result = apply_link_payload(db, link_id, payload);
    core_free(payload);
    return result;
}

static int body_has_ordered_nonempty_fields(const char *body) {
    static const char *labels[] = { "Outcome:", "Evidence:", "Remaining boundary:", "Next:" };
    const char *positions[4];
    size_t i;

    if (!body || strlen(body) > LANE_HEAD_MAX_BODY_BYTES) return 0;
    positions[0] = strstr(body, labels[0]);
    if (!positions[0]) return 0;
    for (i = 1u; i < 4u; ++i) {
        positions[i] = strstr(positions[i - 1u] + strlen(labels[i - 1u]), labels[i]);
        if (!positions[i]) return 0;
    }
    for (i = 0u; i < 4u; ++i) {
        const char *value = positions[i] + strlen(labels[i]);
        const char *end = i + 1u < 4u ? positions[i + 1u] : body + strlen(body);
        while (value < end && (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r')) value += 1;
        if (value >= end) return 0;
    }
    return 1;
}

static int lane_key_is_valid(const char *value) {
    const unsigned char *cursor = (const unsigned char *)value;
    if (!value || value[0] == '\0') return 0;
    while (*cursor != '\0') {
        if (!((*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= 'a' && *cursor <= 'z') ||
              (*cursor >= '0' && *cursor <= '9') || *cursor == '.' || *cursor == '_' || *cursor == '-')) {
            return 0;
        }
        cursor += 1;
    }
    return 1;
}

int cmd_lane_head_upsert(int argc, char **argv) {
    const char *db_path = find_flag_value(argc, argv, "--db");
    const char *workspace = find_flag_value(argc, argv, "--workspace");
    const char *project = find_flag_value(argc, argv, "--project");
    const char *lane = find_flag_value(argc, argv, "--lane");
    const char *stable_id = find_flag_value(argc, argv, "--stable-id");
    const char *title = find_flag_value(argc, argv, "--title");
    const char *body = find_flag_value(argc, argv, "--body");
    const char *anchor_text = find_flag_value(argc, argv, "--anchor-id");
    const char *latest_text = find_flag_value(argc, argv, "--latest-id");
    const char *session_id = find_flag_value(argc, argv, "--session-id");
    const char *session_max_text = find_flag_value(argc, argv, "--session-max-writes");
    CoreMemDb db = {0};
    CoreResult result;
    int64_t anchor_id = 0;
    int64_t latest_id = 0;
    int64_t head_id = 0;
    int head_found = 0;
    int target_exists = 0;
    int item_exact = 0;
    int anchor_exact = 0;
    int latest_exact = 0;
    int anchor_conflict = 0;
    int latest_conflict = 0;
    int competing_head_found = 0;
    int enforce_budget = 0;
    int64_t session_max = 0;
    int64_t session_count = 0;
    int64_t competing_head_id = 0;
    int64_t stale_anchor_ids[LANE_HEAD_MAX_MANAGED_LINKS];
    int64_t stale_latest_ids[LANE_HEAD_MAX_MANAGED_LINKS];
    int stale_anchor_count = 0;
    int stale_latest_count = 0;
    int tx_started = 0;
    int exit_code = 1;
    int created = 0;
    int i;
    int64_t now_ns;
    char fingerprint[17];
    char expected_anchor_note[256];
    char expected_latest_note[256];
    char expected_stable_id[384];
    char existing_workspace[128];
    char existing_project[128];
    char existing_kind[128];
    char competing_stable_id[384];
    char detail[256];
    char *item_payload = 0;

    if (!db_path || !workspace || !project || !lane || !stable_id || !title || !body ||
        !anchor_text || !latest_text || !parse_i64_arg(anchor_text, &anchor_id) ||
        !parse_i64_arg(latest_text, &latest_id) || anchor_id <= 0 || latest_id <= 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (strcmp(workspace, "codework") != 0) {
        fprintf(stderr, "lane-head-upsert: Lane Head V1 supports only workspace=codework\n");
        return 1;
    }
    if (!lane_key_is_valid(project) || !lane_key_is_valid(lane)) {
        fprintf(stderr, "lane-head-upsert: project and lane keys may contain only letters, digits, dot, underscore, and hyphen\n");
        return 1;
    }
    {
        int stable_written = snprintf(expected_stable_id, sizeof(expected_stable_id),
                                      "lane-head-%s-%s", project, lane);
        if (stable_written <= 0 || (size_t)stable_written >= sizeof(expected_stable_id) ||
            strcmp(stable_id, expected_stable_id) != 0) {
            fprintf(stderr, "lane-head-upsert: --stable-id must equal lane-head-<project>-<lane>\n");
            return 1;
        }
    }
    if (anchor_id == latest_id) {
        fprintf(stderr, "lane-head-upsert: anchor and latest receipt must differ\n");
        return 1;
    }
    if (!body_has_ordered_nonempty_fields(body)) {
        fprintf(stderr, "lane-head-upsert: body requires ordered non-empty Outcome, Evidence, Remaining boundary, and Next fields and at most %u bytes\n",
                (unsigned int)LANE_HEAD_MAX_BODY_BYTES);
        return 1;
    }
    if (!parse_session_budget_arg("lane-head-upsert", session_id, session_max_text,
                                  &session_max, &enforce_budget)) {
        return 1;
    }
    {
        int anchor_written = snprintf(expected_anchor_note, sizeof(expected_anchor_note),
                                      "lane-head-v1:anchor:%s", lane);
        int latest_written = snprintf(expected_latest_note, sizeof(expected_latest_note),
                                      "lane-head-v1:latest:%s", lane);
        if (anchor_written <= 0 || latest_written <= 0 ||
            (size_t)anchor_written >= sizeof(expected_anchor_note) ||
            (size_t)latest_written >= sizeof(expected_latest_note)) {
            fprintf(stderr, "lane-head-upsert: lane key is too long\n");
            return 1;
        }
    }
    if (!build_fingerprint(title, body, fingerprint, sizeof(fingerprint))) {
        fprintf(stderr, "lane-head-upsert: failed to build fingerprint\n");
        return 1;
    }
    if (!open_db_or_fail(db_path, &db)) return 1;
    result = core_memdb_exec(&db, "BEGIN IMMEDIATE;");
    if (result.code != CORE_OK) {
        print_core_error("lane-head-upsert", result);
        goto cleanup;
    }
    tx_started = 1;
    result = find_head(&db, stable_id, &head_id, &head_found,
                       existing_workspace, sizeof(existing_workspace),
                       existing_project, sizeof(existing_project),
                       existing_kind, sizeof(existing_kind));
    if (result.code != CORE_OK) goto core_error;
    if (head_found && (strcmp(existing_workspace, workspace) != 0 ||
                       strcmp(existing_project, project) != 0 ||
                       strcmp(existing_kind, "summary") != 0)) {
        fprintf(stderr, "lane-head-upsert: stable identity belongs to scope workspace=%s project=%s kind=%s\n",
                existing_workspace, existing_project, existing_kind);
        goto cleanup;
    }
    result = scoped_item_exists(&db, anchor_id, workspace, project, &target_exists);
    if (result.code != CORE_OK) goto core_error;
    if (!target_exists) {
        fprintf(stderr, "lane-head-upsert: anchor item %lld is outside workspace=%s project=%s\n",
                (long long)anchor_id, workspace, project);
        goto cleanup;
    }
    result = scoped_item_exists(&db, latest_id, workspace, project, &target_exists);
    if (result.code != CORE_OK) goto core_error;
    if (!target_exists) {
        fprintf(stderr, "lane-head-upsert: latest item %lld is outside workspace=%s project=%s\n",
                (long long)latest_id, workspace, project);
        goto cleanup;
    }
    result = find_competing_head(&db, workspace, project, stable_id, anchor_id, latest_id,
                                 &competing_head_id, competing_stable_id,
                                 sizeof(competing_stable_id), &competing_head_found);
    if (result.code != CORE_OK) goto core_error;
    if (competing_head_found) {
        fprintf(stderr,
                "lane-head-upsert: anchor/latest pair is already owned by id=%lld stable_id=%s; use that established lane key or choose distinct receipts\n",
                (long long)competing_head_id, competing_stable_id);
        goto cleanup;
    }
    if (head_found && (head_id == anchor_id || head_id == latest_id)) {
        fprintf(stderr, "lane-head-upsert: managed link would be a self-loop\n");
        goto cleanup;
    }
    if (head_found) {
        result = item_matches(&db, head_id, stable_id, title, body, fingerprint, workspace, project, &item_exact);
        if (result.code != CORE_OK) goto core_error;
        result = link_exact_exists(&db, head_id, anchor_id, "references", expected_anchor_note, &anchor_exact);
        if (result.code != CORE_OK) goto core_error;
        result = link_exact_exists(&db, head_id, latest_id, "summarizes", expected_latest_note, &latest_exact);
        if (result.code != CORE_OK) goto core_error;
        result = link_conflict_exists(&db, head_id, anchor_id, "references", expected_anchor_note, &anchor_conflict);
        if (result.code != CORE_OK) goto core_error;
        result = link_conflict_exists(&db, head_id, latest_id, "summarizes", expected_latest_note, &latest_conflict);
        if (result.code != CORE_OK) goto core_error;
        result = collect_stale_links(&db, head_id, anchor_id, "references", expected_anchor_note,
                                     stale_anchor_ids, &stale_anchor_count);
        if (result.code != CORE_OK) goto core_error;
        result = collect_stale_links(&db, head_id, latest_id, "summarizes", expected_latest_note,
                                     stale_latest_ids, &stale_latest_count);
        if (result.code != CORE_OK) goto core_error;
        if (anchor_conflict || latest_conflict) {
            fprintf(stderr, "lane-head-upsert: desired managed edge conflicts with an unrelated edge; no changes applied\n");
            goto cleanup;
        }
        if (item_exact && anchor_exact && latest_exact && stale_anchor_count == 0 && stale_latest_count == 0) {
            result = core_memdb_tx_commit(&db);
            if (result.code != CORE_OK) goto core_error;
            tx_started = 0;
            printf("unchanged id=%lld stable_id=%s anchor_id=%lld latest_id=%lld\n",
                   (long long)head_id, stable_id, (long long)anchor_id, (long long)latest_id);
            exit_code = 0;
            goto cleanup;
        }
    }
    if (enforce_budget) {
        result = fetch_session_mutation_write_count(&db, session_id, &session_count);
        if (result.code != CORE_OK) goto core_error;
        if (session_count >= session_max) {
            (void)report_session_budget_exceeded(&db, "lane-head-upsert", session_id,
                                                 session_count, session_max, head_id,
                                                 stable_id, workspace, project, "summary");
            result = core_memdb_tx_commit(&db);
            if (result.code != CORE_OK) goto core_error;
            tx_started = 0;
            fprintf(stderr, "lane-head-upsert: session write budget exceeded (session=%s used=%lld max=%lld)\n",
                    session_id, (long long)session_count, (long long)session_max);
            goto cleanup;
        }
    }
    if (!head_found) {
        result = fetch_next_item_id(&db, &head_id);
        if (result.code != CORE_OK) goto core_error;
        created = 1;
    }
    now_ns = current_time_ns();
    if (!item_exact) {
        result = build_item_payload(&db, head_found, head_id, stable_id, title, body,
                                    fingerprint, workspace, project, now_ns, &item_payload);
        if (result.code != CORE_OK) goto core_error;
        result = append_event_entry(&db, session_id, created ? "NodeCreated" : "NodeBodyUpdated",
                                    head_id, 0, stable_id, workspace, project, "summary", item_payload);
        if (result.code != CORE_OK) goto core_error;
        result = apply_item_payload(&db, head_id, item_payload);
        if (result.code != CORE_OK) goto core_error;
        result = sync_fts_row(&db, head_id, title, body);
        if (result.code != CORE_OK) goto core_error;
    }
    for (i = 0; i < stale_anchor_count; ++i) {
        result = remove_link(&db, stale_anchor_ids[i], head_id, session_id, stable_id, workspace, project);
        if (result.code != CORE_OK) goto core_error;
    }
    for (i = 0; i < stale_latest_count; ++i) {
        result = remove_link(&db, stale_latest_ids[i], head_id, session_id, stable_id, workspace, project);
        if (result.code != CORE_OK) goto core_error;
    }
    if (!anchor_exact) {
        result = add_link(&db, head_id, anchor_id, "references", expected_anchor_note,
                          session_id, stable_id, workspace, project);
        if (result.code != CORE_OK) goto core_error;
    }
    if (!latest_exact) {
        result = add_link(&db, head_id, latest_id, "summarizes", expected_latest_note,
                          session_id, stable_id, workspace, project);
        if (result.code != CORE_OK) goto core_error;
    }
    (void)snprintf(detail, sizeof(detail), "%s atomically anchor=%lld latest=%lld",
                   created ? "created" : "updated", (long long)anchor_id, (long long)latest_id);
    result = append_audit_entry(&db, session_id, "lane-head-upsert", "ok", head_id,
                                stable_id, workspace, project, "summary", detail);
    if (result.code != CORE_OK) goto core_error;
    result = core_memdb_tx_commit(&db);
    if (result.code != CORE_OK) goto core_error;
    tx_started = 0;
    printf("%s id=%lld stable_id=%s anchor_id=%lld latest_id=%lld\n",
           created ? "created" : "updated", (long long)head_id, stable_id,
           (long long)anchor_id, (long long)latest_id);
    exit_code = 0;
    goto cleanup;

core_error:
    print_core_error("lane-head-upsert", result);
cleanup:
    core_free(item_payload);
    if (tx_started) (void)core_memdb_tx_rollback(&db);
    (void)core_memdb_close(&db);
    return exit_code;
}
