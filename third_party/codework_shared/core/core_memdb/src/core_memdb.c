/*
 * core_memdb.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_memdb.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"

#define CORE_MEMDB_SCHEMA_VERSION 6u

static CoreResult core_memdb_result(CoreError code, const char *message) {
    CoreResult result = { code, message };
    return result;
}

static CoreError core_memdb_map_sqlite_code(int code) {
    switch (code) {
        case SQLITE_OK:
        case SQLITE_ROW:
        case SQLITE_DONE:
            return CORE_OK;
        case SQLITE_NOMEM:
            return CORE_ERR_OUT_OF_MEMORY;
        case SQLITE_CANTOPEN:
        case SQLITE_IOERR:
        case SQLITE_BUSY:
        case SQLITE_LOCKED:
        case SQLITE_READONLY:
        case SQLITE_PERM:
        case SQLITE_FULL:
            return CORE_ERR_IO;
        case SQLITE_NOTFOUND:
            return CORE_ERR_NOT_FOUND;
        case SQLITE_ERROR:
        case SQLITE_MISUSE:
        case SQLITE_SCHEMA:
        case SQLITE_CONSTRAINT:
        case SQLITE_RANGE:
        default:
            return CORE_ERR_FORMAT;
    }
}

static CoreResult core_memdb_sqlite_db_error(sqlite3 *handle,
                                             int code,
                                             const char *fallback_message) {
    const char *message = fallback_message;

    if (handle) {
        const char *sqlite_message = sqlite3_errmsg(handle);
        if (sqlite_message) {
            message = sqlite_message;
        }
    }

    return core_memdb_result(core_memdb_map_sqlite_code(code), message);
}

static CoreResult core_memdb_sqlite_stmt_error(sqlite3_stmt *handle,
                                               int code,
                                               const char *fallback_message) {
    sqlite3 *db = 0;

    if (handle) {
        db = sqlite3_db_handle(handle);
    }

    return core_memdb_sqlite_db_error(db, code, fallback_message);
}

static CoreResult core_memdb_exec_sqlite(sqlite3 *handle,
                                         const char *sql,
                                         const char *fallback_message) {
    int sqlite_result;

    if (!handle || !sql) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    sqlite_result = sqlite3_exec(handle, sql, 0, 0, 0);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, fallback_message);
    }

    return core_result_ok();
}

static sqlite3 *core_memdb_db_handle(CoreMemDb *db) {
    if (!db) {
        return 0;
    }

    return (sqlite3 *)db->native_db;
}

static sqlite3_stmt *core_memdb_stmt_handle(CoreMemStmt *stmt) {
    if (!stmt) {
        return 0;
    }

    return (sqlite3_stmt *)stmt->native_stmt;
}

static CoreResult core_memdb_require_open_db(CoreMemDb *db, sqlite3 **out_handle) {
    sqlite3 *handle = core_memdb_db_handle(db);

    if (!db || !out_handle) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    if (!handle) {
        *out_handle = 0;
        return core_memdb_result(CORE_ERR_NOT_FOUND, "database not open");
    }

    *out_handle = handle;
    return core_result_ok();
}

static CoreResult core_memdb_require_live_stmt(CoreMemStmt *stmt, sqlite3_stmt **out_handle) {
    sqlite3_stmt *handle = core_memdb_stmt_handle(stmt);

    if (!stmt || !out_handle) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    if (!stmt->db || !handle) {
        *out_handle = 0;
        return core_memdb_result(CORE_ERR_NOT_FOUND, "statement not prepared");
    }

    *out_handle = handle;
    return core_result_ok();
}

static CoreResult core_memdb_begin_tx_sqlite(sqlite3 *handle) {
    return core_memdb_exec_sqlite(handle, "BEGIN IMMEDIATE;", "transaction begin failed");
}

static CoreResult core_memdb_commit_tx_sqlite(sqlite3 *handle) {
    return core_memdb_exec_sqlite(handle, "COMMIT;", "transaction commit failed");
}

static CoreResult core_memdb_rollback_tx_sqlite(sqlite3 *handle) {
    return core_memdb_exec_sqlite(handle, "ROLLBACK;", "transaction rollback failed");
}

static CoreResult core_memdb_has_meta_table(sqlite3 *handle, int *out_exists) {
    sqlite3_stmt *stmt = 0;
    int sqlite_result;
    int finalize_result;
    int count = 0;

    if (!handle || !out_exists) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    *out_exists = 0;
    sqlite_result = sqlite3_prepare_v2(handle,
                                       "SELECT COUNT(*) "
                                       "FROM sqlite_master "
                                       "WHERE type = 'table' AND name = 'mem_meta';",
                                       -1,
                                       &stmt,
                                       0);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to inspect schema");
    }

    sqlite_result = sqlite3_step(stmt);
    if (sqlite_result == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }

    finalize_result = sqlite3_finalize(stmt);
    if (sqlite_result != SQLITE_ROW) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to inspect schema");
    }
    if (finalize_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, finalize_result, "failed to finalize schema inspection");
    }

    *out_exists = count > 0 ? 1 : 0;
    return core_result_ok();
}

static CoreResult core_memdb_table_has_column(sqlite3 *handle,
                                              const char *column_name,
                                              int *out_exists) {
    sqlite3_stmt *stmt = 0;
    int sqlite_result;
    int finalize_result;

    if (!handle || !column_name || !out_exists) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    *out_exists = 0;
    sqlite_result = sqlite3_prepare_v2(handle, "PRAGMA table_info(mem_item);", -1, &stmt, 0);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to inspect table info");
    }

    for (;;) {
        const unsigned char *name_text = 0;
        sqlite_result = sqlite3_step(stmt);
        if (sqlite_result == SQLITE_DONE) {
            break;
        }
        if (sqlite_result != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to read table info");
        }

        name_text = sqlite3_column_text(stmt, 1);
        if (name_text && strcmp((const char *)name_text, column_name) == 0) {
            *out_exists = 1;
            break;
        }
    }

    finalize_result = sqlite3_finalize(stmt);
    if (finalize_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, finalize_result, "failed to finalize table info");
    }

    return core_result_ok();
}

static CoreResult core_memdb_write_schema_version(sqlite3 *handle, uint32_t version) {
    sqlite3_stmt *stmt = 0;
    int sqlite_result;
    int finalize_result;

    if (!handle) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    sqlite_result = sqlite3_prepare_v2(handle,
                                       "INSERT OR REPLACE INTO mem_meta (key, value) "
                                       "VALUES ('schema_version', ?1);",
                                       -1,
                                       &stmt,
                                       0);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to prepare schema version write");
    }

    sqlite_result = sqlite3_bind_int64(stmt, 1, (sqlite3_int64)version);
    if (sqlite_result != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to bind schema version");
    }

    sqlite_result = sqlite3_step(stmt);
    finalize_result = sqlite3_finalize(stmt);
    if (sqlite_result != SQLITE_DONE) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to write schema version");
    }
    if (finalize_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, finalize_result, "failed to finalize schema version write");
    }

    return core_result_ok();
}

static CoreResult core_memdb_read_schema_version(sqlite3 *handle, uint32_t *out_version) {
    sqlite3_stmt *stmt = 0;
    int sqlite_result;
    int finalize_result;

    if (!handle || !out_version) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    *out_version = 0;
    sqlite_result = sqlite3_prepare_v2(handle,
                                       "SELECT value FROM mem_meta WHERE key = 'schema_version';",
                                       -1,
                                       &stmt,
                                       0);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to prepare schema version read");
    }

    sqlite_result = sqlite3_step(stmt);
    if (sqlite_result == SQLITE_ROW) {
        const unsigned char *text_value = sqlite3_column_text(stmt, 0);
        char *end_ptr = 0;
        unsigned long parsed_value;

        if (!text_value) {
            sqlite3_finalize(stmt);
            return core_memdb_result(CORE_ERR_FORMAT, "schema version missing");
        }

        errno = 0;
        parsed_value = strtoul((const char *)text_value, &end_ptr, 10);
        if (errno != 0 || !end_ptr || *end_ptr != '\0' || parsed_value > UINT32_MAX) {
            sqlite3_finalize(stmt);
            return core_memdb_result(CORE_ERR_FORMAT, "schema version invalid");
        }

        *out_version = (uint32_t)parsed_value;
    }

    finalize_result = sqlite3_finalize(stmt);
    if (sqlite_result == SQLITE_DONE) {
        return core_memdb_result(CORE_ERR_NOT_FOUND, "schema version not found");
    }
    if (sqlite_result != SQLITE_ROW) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to read schema version");
    }
    if (finalize_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, finalize_result, "failed to finalize schema version read");
    }

    return core_result_ok();
}

static CoreResult core_memdb_create_base_schema(sqlite3 *handle) {
    static const char *kSchemaSql[] = {
        "CREATE TABLE mem_meta ("
        "key TEXT PRIMARY KEY,"
        "value TEXT NOT NULL"
        ");",
        "CREATE TABLE mem_item ("
        "id INTEGER PRIMARY KEY,"
        "stable_id TEXT UNIQUE,"
        "title TEXT NOT NULL,"
        "body TEXT NOT NULL,"
        "fingerprint TEXT NOT NULL,"
        "workspace_key TEXT NOT NULL DEFAULT '',"
        "project_key TEXT NOT NULL DEFAULT '',"
        "kind TEXT NOT NULL DEFAULT '',"
        "created_ns INTEGER NOT NULL,"
        "updated_ns INTEGER NOT NULL,"
        "pinned INTEGER NOT NULL DEFAULT 0,"
        "canonical INTEGER NOT NULL DEFAULT 0,"
        "ttl_until_ns INTEGER,"
        "archived_ns INTEGER"
        ");",
        "CREATE TABLE mem_tag ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT UNIQUE NOT NULL"
        ");",
        "CREATE TABLE mem_item_tag ("
        "item_id INTEGER NOT NULL,"
        "tag_id INTEGER NOT NULL,"
        "PRIMARY KEY (item_id, tag_id)"
        ");",
        "CREATE TABLE mem_link ("
        "id INTEGER PRIMARY KEY,"
        "from_item_id INTEGER NOT NULL,"
        "to_item_id INTEGER NOT NULL,"
        "kind TEXT NOT NULL,"
        "weight REAL,"
        "note TEXT"
        ");",
        "CREATE TABLE mem_audit ("
        "id INTEGER PRIMARY KEY,"
        "ts_ns INTEGER NOT NULL,"
        "session_id TEXT,"
        "action TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "item_id INTEGER,"
        "stable_id TEXT,"
        "workspace_key TEXT NOT NULL DEFAULT '',"
        "project_key TEXT NOT NULL DEFAULT '',"
        "kind TEXT NOT NULL DEFAULT '',"
        "detail TEXT NOT NULL DEFAULT ''"
        ");",
        "CREATE TABLE mem_event ("
        "id INTEGER PRIMARY KEY,"
        "ts_ns INTEGER NOT NULL,"
        "session_id TEXT,"
        "event_id TEXT NOT NULL,"
        "event_type TEXT NOT NULL,"
        "actor_type TEXT NOT NULL DEFAULT '',"
        "actor_id TEXT NOT NULL DEFAULT '',"
        "item_id INTEGER,"
        "link_id INTEGER,"
        "stable_id TEXT,"
        "workspace_key TEXT NOT NULL DEFAULT '',"
        "project_key TEXT NOT NULL DEFAULT '',"
        "kind TEXT NOT NULL DEFAULT '',"
        "payload_json TEXT NOT NULL DEFAULT '{}'"
        ");",
        "CREATE INDEX mem_item_fingerprint_idx ON mem_item(fingerprint);",
        "CREATE INDEX mem_item_scope_idx ON mem_item(workspace_key, project_key, kind, updated_ns DESC, id ASC);",
        "CREATE INDEX mem_audit_session_idx ON mem_audit(session_id, ts_ns, id);",
        "CREATE UNIQUE INDEX mem_event_event_id_idx ON mem_event(event_id);",
        "CREATE INDEX mem_event_ts_idx ON mem_event(ts_ns, id);",
        "CREATE INDEX mem_event_type_idx ON mem_event(event_type, ts_ns, id);",
        "CREATE INDEX mem_event_session_idx ON mem_event(session_id, ts_ns, id);",
        "CREATE INDEX mem_link_from_idx ON mem_link(from_item_id, id);",
        "CREATE INDEX mem_link_to_idx ON mem_link(to_item_id, id);",
        "CREATE INDEX mem_link_kind_idx ON mem_link(kind, id);",
        "CREATE UNIQUE INDEX mem_link_unique_kind_edge ON mem_link(from_item_id, to_item_id, kind);",
        "CREATE VIRTUAL TABLE mem_item_fts USING fts5(title, body);"
    };
    size_t i;
    CoreResult result;

    if (!handle) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    for (i = 0; i < sizeof(kSchemaSql) / sizeof(kSchemaSql[0]); ++i) {
        result = core_memdb_exec_sqlite(handle, kSchemaSql[i], "schema creation failed");
        if (result.code != CORE_OK) {
            return result;
        }
    }

    return core_memdb_write_schema_version(handle, CORE_MEMDB_SCHEMA_VERSION);
}

static CoreResult core_memdb_apply_builtin_migrations(sqlite3 *handle,
                                                      uint32_t from_version,
                                                      uint32_t target_version) {
    uint32_t version = from_version;
    CoreResult result;

    if (!handle) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    while (version < target_version) {
        if (version == 1u) {
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_item_fingerprint_idx "
                                            "ON mem_item(fingerprint);",
                                            "failed to apply v2 migration");
            if (result.code != CORE_OK) {
                return result;
            }
            version = 2u;
            continue;
        }
        if (version == 2u) {
            int column_exists = 0;

            result = core_memdb_table_has_column(handle, "workspace_key", &column_exists);
            if (result.code != CORE_OK) {
                return result;
            }
            if (!column_exists) {
                result = core_memdb_exec_sqlite(handle,
                                                "ALTER TABLE mem_item ADD COLUMN workspace_key TEXT NOT NULL DEFAULT '';",
                                                "failed to apply v3 migration (workspace_key)");
                if (result.code != CORE_OK) {
                    return result;
                }
            }

            result = core_memdb_table_has_column(handle, "project_key", &column_exists);
            if (result.code != CORE_OK) {
                return result;
            }
            if (!column_exists) {
                result = core_memdb_exec_sqlite(handle,
                                                "ALTER TABLE mem_item ADD COLUMN project_key TEXT NOT NULL DEFAULT '';",
                                                "failed to apply v3 migration (project_key)");
                if (result.code != CORE_OK) {
                    return result;
                }
            }

            result = core_memdb_table_has_column(handle, "kind", &column_exists);
            if (result.code != CORE_OK) {
                return result;
            }
            if (!column_exists) {
                result = core_memdb_exec_sqlite(handle,
                                                "ALTER TABLE mem_item ADD COLUMN kind TEXT NOT NULL DEFAULT '';",
                                                "failed to apply v3 migration (kind)");
                if (result.code != CORE_OK) {
                    return result;
                }
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_item_fingerprint_idx ON mem_item(fingerprint);",
                                            "failed to apply v3 migration (fingerprint index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_item_scope_idx ON mem_item(workspace_key, project_key, kind, updated_ns DESC, id ASC);",
                                            "failed to apply v3 migration (scope index)");
            if (result.code != CORE_OK) {
                return result;
            }
            version = 3u;
            continue;
        }
        if (version == 3u) {
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE TABLE IF NOT EXISTS mem_audit ("
                                            "id INTEGER PRIMARY KEY,"
                                            "ts_ns INTEGER NOT NULL,"
                                            "session_id TEXT,"
                                            "action TEXT NOT NULL,"
                                            "status TEXT NOT NULL,"
                                            "item_id INTEGER,"
                                            "stable_id TEXT,"
                                            "workspace_key TEXT NOT NULL DEFAULT '',"
                                            "project_key TEXT NOT NULL DEFAULT '',"
                                            "kind TEXT NOT NULL DEFAULT '',"
                                            "detail TEXT NOT NULL DEFAULT ''"
                                            ");",
                                            "failed to apply v4 migration (audit table)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_audit_session_idx ON mem_audit(session_id, ts_ns, id);",
                                            "failed to apply v4 migration (audit index)");
            if (result.code != CORE_OK) {
                return result;
            }
            version = 4u;
            continue;
        }
        if (version == 4u) {
            result = core_memdb_exec_sqlite(handle,
                                            "DELETE FROM mem_link "
                                            "WHERE from_item_id = to_item_id;",
                                            "failed to apply v5 migration (remove self-loops)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "DELETE FROM mem_link "
                                            "WHERE id NOT IN ("
                                            "SELECT MIN(id) "
                                            "FROM mem_link "
                                            "GROUP BY from_item_id, to_item_id, kind"
                                            ");",
                                            "failed to apply v5 migration (dedupe links)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_link_from_idx ON mem_link(from_item_id, id);",
                                            "failed to apply v5 migration (from index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_link_to_idx ON mem_link(to_item_id, id);",
                                            "failed to apply v5 migration (to index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_link_kind_idx ON mem_link(kind, id);",
                                            "failed to apply v5 migration (kind index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE UNIQUE INDEX IF NOT EXISTS mem_link_unique_kind_edge "
                                            "ON mem_link(from_item_id, to_item_id, kind);",
                                            "failed to apply v5 migration (unique edge index)");
            if (result.code != CORE_OK) {
                return result;
            }
            version = 5u;
            continue;
        }
        if (version == 5u) {
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE TABLE IF NOT EXISTS mem_event ("
                                            "id INTEGER PRIMARY KEY,"
                                            "ts_ns INTEGER NOT NULL,"
                                            "session_id TEXT,"
                                            "event_id TEXT NOT NULL,"
                                            "event_type TEXT NOT NULL,"
                                            "actor_type TEXT NOT NULL DEFAULT '',"
                                            "actor_id TEXT NOT NULL DEFAULT '',"
                                            "item_id INTEGER,"
                                            "link_id INTEGER,"
                                            "stable_id TEXT,"
                                            "workspace_key TEXT NOT NULL DEFAULT '',"
                                            "project_key TEXT NOT NULL DEFAULT '',"
                                            "kind TEXT NOT NULL DEFAULT '',"
                                            "payload_json TEXT NOT NULL DEFAULT '{}'"
                                            ");",
                                            "failed to apply v6 migration (event table)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE UNIQUE INDEX IF NOT EXISTS mem_event_event_id_idx ON mem_event(event_id);",
                                            "failed to apply v6 migration (event id index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_event_ts_idx ON mem_event(ts_ns, id);",
                                            "failed to apply v6 migration (event ts index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_event_type_idx ON mem_event(event_type, ts_ns, id);",
                                            "failed to apply v6 migration (event type index)");
            if (result.code != CORE_OK) {
                return result;
            }
            result = core_memdb_exec_sqlite(handle,
                                            "CREATE INDEX IF NOT EXISTS mem_event_session_idx ON mem_event(session_id, ts_ns, id);",
                                            "failed to apply v6 migration (event session index)");
            if (result.code != CORE_OK) {
                return result;
            }
            version = 6u;
            continue;
        }

        return core_memdb_result(CORE_ERR_NOT_FOUND, "unsupported schema version");
    }

    return core_result_ok();
}

static void core_memdb_clear_db(CoreMemDb *db) {
    if (!db) {
        return;
    }

    db->native_db = 0;
}

static void core_memdb_clear_stmt(CoreMemStmt *stmt) {
    if (!stmt) {
        return;
    }

    stmt->native_stmt = 0;
    stmt->db = 0;
}

CoreResult core_memdb_open(const char *path, CoreMemDb *out_db) {
    sqlite3 *handle = 0;
    int sqlite_result;
    CoreResult result;

    if (!path || !out_db) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    core_memdb_clear_db(out_db);
    sqlite_result = sqlite3_open_v2(path,
                                    &handle,
                                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                    0);
    if (sqlite_result != SQLITE_OK) {
        result = core_memdb_sqlite_db_error(handle, sqlite_result, "failed to open database");
        if (handle) {
            sqlite3_close(handle);
        }
        return result;
    }

    sqlite3_extended_result_codes(handle, 1);
    out_db->native_db = handle;

    result = core_memdb_run_migrations(out_db, CORE_MEMDB_SCHEMA_VERSION, 0, 0);
    if (result.code != CORE_OK) {
        sqlite3_close(handle);
        core_memdb_clear_db(out_db);
        return result;
    }

    return core_result_ok();
}

CoreResult core_memdb_close(CoreMemDb *db) {
    sqlite3 *handle = 0;
    int sqlite_result;

    if (!db) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    handle = core_memdb_db_handle(db);
    if (!handle) {
        core_memdb_clear_db(db);
        return core_result_ok();
    }

    sqlite_result = sqlite3_close(handle);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(handle, sqlite_result, "failed to close database");
    }

    core_memdb_clear_db(db);
    return core_result_ok();
}

CoreResult core_memdb_exec(CoreMemDb *db, const char *sql) {
    sqlite3 *handle = 0;
    CoreResult result;

    if (!db || !sql) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_open_db(db, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    return core_memdb_exec_sqlite(handle, sql, "exec failed");
}

CoreResult core_memdb_prepare(CoreMemDb *db, const char *sql, CoreMemStmt *out_stmt) {
    sqlite3 *handle = 0;
    sqlite3_stmt *stmt_handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!db || !sql || !out_stmt) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_open_db(db, &handle);
    if (result.code != CORE_OK) {
        core_memdb_clear_stmt(out_stmt);
        return result;
    }

    core_memdb_clear_stmt(out_stmt);
    sqlite_result = sqlite3_prepare_v2(handle, sql, -1, &stmt_handle, 0);
    if (sqlite_result != SQLITE_OK) {
        if (stmt_handle) {
            sqlite3_finalize(stmt_handle);
        }
        return core_memdb_sqlite_db_error(handle, sqlite_result, "prepare failed");
    }

    out_stmt->native_stmt = stmt_handle;
    out_stmt->db = db;
    return core_result_ok();
}

CoreResult core_memdb_stmt_step(CoreMemStmt *stmt, int *out_has_row) {
    sqlite3_stmt *handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!stmt || !out_has_row) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        *out_has_row = 0;
        return result;
    }

    sqlite_result = sqlite3_step(handle);
    if (sqlite_result == SQLITE_ROW) {
        *out_has_row = 1;
        return core_result_ok();
    }
    if (sqlite_result == SQLITE_DONE) {
        *out_has_row = 0;
        return core_result_ok();
    }

    *out_has_row = 0;
    return core_memdb_sqlite_stmt_error(handle, sqlite_result, "statement step failed");
}

CoreResult core_memdb_stmt_reset(CoreMemStmt *stmt) {
    sqlite3_stmt *handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!stmt) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    sqlite_result = sqlite3_reset(handle);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_stmt_error(handle, sqlite_result, "statement reset failed");
    }

    return core_result_ok();
}

CoreResult core_memdb_stmt_finalize(CoreMemStmt *stmt) {
    sqlite3_stmt *handle = 0;
    sqlite3 *db = 0;
    int sqlite_result = SQLITE_OK;

    if (!stmt) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    handle = core_memdb_stmt_handle(stmt);
    if (handle) {
        db = sqlite3_db_handle(handle);
        sqlite_result = sqlite3_finalize(handle);
    }

    core_memdb_clear_stmt(stmt);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_db_error(db, sqlite_result, "statement finalize failed");
    }

    return core_result_ok();
}

CoreResult core_memdb_stmt_bind_i64(CoreMemStmt *stmt, int index_1based, int64_t value) {
    sqlite3_stmt *handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!stmt || index_1based <= 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    sqlite_result = sqlite3_bind_int64(handle, index_1based, (sqlite3_int64)value);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_stmt_error(handle, sqlite_result, "bind i64 failed");
    }

    return core_result_ok();
}

CoreResult core_memdb_stmt_bind_text(CoreMemStmt *stmt, int index_1based, const char *value) {
    sqlite3_stmt *handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!stmt || !value || index_1based <= 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    sqlite_result = sqlite3_bind_text(handle, index_1based, value, -1, SQLITE_TRANSIENT);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_stmt_error(handle, sqlite_result, "bind text failed");
    }

    return core_result_ok();
}

CoreResult core_memdb_stmt_bind_f64(CoreMemStmt *stmt, int index_1based, double value) {
    sqlite3_stmt *handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!stmt || index_1based <= 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    sqlite_result = sqlite3_bind_double(handle, index_1based, value);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_stmt_error(handle, sqlite_result, "bind f64 failed");
    }

    return core_result_ok();
}

CoreResult core_memdb_stmt_bind_null(CoreMemStmt *stmt, int index_1based) {
    sqlite3_stmt *handle = 0;
    CoreResult result;
    int sqlite_result;

    if (!stmt || index_1based <= 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    sqlite_result = sqlite3_bind_null(handle, index_1based);
    if (sqlite_result != SQLITE_OK) {
        return core_memdb_sqlite_stmt_error(handle, sqlite_result, "bind null failed");
    }

    return core_result_ok();
}

CoreResult core_memdb_stmt_column_i64(CoreMemStmt *stmt, int column_0based, int64_t *out_value) {
    sqlite3_stmt *handle = 0;
    CoreResult result;

    if (!stmt || !out_value || column_0based < 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        *out_value = 0;
        return result;
    }

    *out_value = (int64_t)sqlite3_column_int64(handle, column_0based);
    return core_result_ok();
}

CoreResult core_memdb_stmt_column_text(CoreMemStmt *stmt, int column_0based, CoreStr *out_text) {
    sqlite3_stmt *handle = 0;
    const unsigned char *text_data = 0;
    CoreResult result;

    if (!stmt || !out_text || column_0based < 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_live_stmt(stmt, &handle);
    if (result.code != CORE_OK) {
        out_text->data = 0;
        out_text->len = 0;
        return result;
    }

    text_data = sqlite3_column_text(handle, column_0based);
    if (!text_data) {
        out_text->data = 0;
        out_text->len = 0;
        return core_result_ok();
    }

    out_text->data = (const char *)text_data;
    out_text->len = (size_t)sqlite3_column_bytes(handle, column_0based);
    return core_result_ok();
}

CoreResult core_memdb_tx_begin(CoreMemDb *db) {
    sqlite3 *handle = 0;
    CoreResult result;

    if (!db) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_open_db(db, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    return core_memdb_begin_tx_sqlite(handle);
}

CoreResult core_memdb_tx_commit(CoreMemDb *db) {
    sqlite3 *handle = 0;
    CoreResult result;

    if (!db) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_open_db(db, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    return core_memdb_commit_tx_sqlite(handle);
}

CoreResult core_memdb_tx_rollback(CoreMemDb *db) {
    sqlite3 *handle = 0;
    CoreResult result;

    if (!db) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    result = core_memdb_require_open_db(db, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    return core_memdb_rollback_tx_sqlite(handle);
}

CoreResult core_memdb_run_migrations(CoreMemDb *db,
                                     uint32_t target_version,
                                     CoreMemDbMigrationFn migration_fn,
                                     void *user_ctx) {
    sqlite3 *handle = 0;
    CoreResult result;
    uint32_t current_version = 0;
    int has_meta = 0;

    if (!db) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }
    if (target_version == 0) {
        return core_memdb_result(CORE_ERR_INVALID_ARG, "invalid argument");
    }

    result = core_memdb_require_open_db(db, &handle);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_has_meta_table(handle, &has_meta);
    if (result.code != CORE_OK) {
        return result;
    }

    if (!has_meta) {
        result = core_memdb_begin_tx_sqlite(handle);
        if (result.code != CORE_OK) {
            return result;
        }

        result = core_memdb_create_base_schema(handle);
        if (result.code != CORE_OK) {
            core_memdb_rollback_tx_sqlite(handle);
            return result;
        }

        result = core_memdb_commit_tx_sqlite(handle);
        if (result.code != CORE_OK) {
            core_memdb_rollback_tx_sqlite(handle);
            return result;
        }
    }

    result = core_memdb_read_schema_version(handle, &current_version);
    if (result.code != CORE_OK) {
        return result;
    }

    if (current_version == target_version) {
        return core_result_ok();
    }
    if (current_version > target_version) {
        return core_memdb_result(CORE_ERR_FORMAT, "database schema version is newer than supported");
    }
    result = core_memdb_begin_tx_sqlite(handle);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_memdb_apply_builtin_migrations(handle, current_version, target_version);
    if (result.code != CORE_OK) {
        if (migration_fn) {
            result = migration_fn(db, current_version, target_version, user_ctx);
        }
        if (result.code != CORE_OK) {
            core_memdb_rollback_tx_sqlite(handle);
            return result;
        }
    }

    result = core_memdb_write_schema_version(handle, target_version);
    if (result.code != CORE_OK) {
        core_memdb_rollback_tx_sqlite(handle);
        return result;
    }

    result = core_memdb_commit_tx_sqlite(handle);
    if (result.code != CORE_OK) {
        core_memdb_rollback_tx_sqlite(handle);
        return result;
    }

    return core_result_ok();
}
