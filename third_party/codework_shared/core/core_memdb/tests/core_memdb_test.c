#include <assert.h>
#include <stdio.h>

#include "core_memdb.h"
#include "sqlite3.h"

static const char *kTestDbPath = "build/core_memdb_step4_test.sqlite";

static void remove_test_db(void) {
    (void)remove(kTestDbPath);
}

static void create_legacy_v2_db(const char *path) {
    sqlite3 *db = 0;
    int rc;

    assert(path);
    (void)remove(path);

    rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    assert(rc == SQLITE_OK);
    assert(db != 0);

    rc = sqlite3_exec(db,
                      "CREATE TABLE mem_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);"
                      "CREATE TABLE mem_item ("
                      "id INTEGER PRIMARY KEY,"
                      "stable_id TEXT UNIQUE,"
                      "title TEXT NOT NULL,"
                      "body TEXT NOT NULL,"
                      "fingerprint TEXT NOT NULL,"
                      "created_ns INTEGER NOT NULL,"
                      "updated_ns INTEGER NOT NULL,"
                      "pinned INTEGER NOT NULL DEFAULT 0,"
                      "canonical INTEGER NOT NULL DEFAULT 0,"
                      "ttl_until_ns INTEGER,"
                      "archived_ns INTEGER"
                      ");"
                      "CREATE TABLE mem_tag (id INTEGER PRIMARY KEY, name TEXT UNIQUE NOT NULL);"
                      "CREATE TABLE mem_item_tag (item_id INTEGER NOT NULL, tag_id INTEGER NOT NULL, PRIMARY KEY (item_id, tag_id));"
                      "CREATE TABLE mem_link ("
                      "id INTEGER PRIMARY KEY,"
                      "from_item_id INTEGER NOT NULL,"
                      "to_item_id INTEGER NOT NULL,"
                      "kind TEXT NOT NULL,"
                      "weight REAL,"
                      "note TEXT"
                      ");"
                      "CREATE INDEX mem_item_fingerprint_idx ON mem_item(fingerprint);"
                      "CREATE VIRTUAL TABLE mem_item_fts USING fts5(title, body);"
                      "INSERT INTO mem_meta (key, value) VALUES ('schema_version', '2');",
                      0,
                      0,
                      0);
    assert(rc == SQLITE_OK);

    rc = sqlite3_close(db);
    assert(rc == SQLITE_OK);
}

static int query_count(CoreMemDb *db, const char *sql) {
    CoreMemStmt stmt;
    CoreResult result;
    int has_row = 0;
    int64_t value = 0;

    result = core_memdb_prepare(db, sql, &stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_step(&stmt, &has_row);
    assert(result.code == CORE_OK);
    assert(has_row == 1);

    result = core_memdb_stmt_column_i64(&stmt, 0, &value);
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_finalize(&stmt);
    assert(result.code == CORE_OK);

    return (int)value;
}

static void assert_schema_version(CoreMemDb *db, const char *expected_text) {
    CoreMemStmt stmt;
    CoreResult result;
    int has_row = 0;
    CoreStr text_value = {0};

    result = core_memdb_prepare(db,
                                "SELECT value FROM mem_meta WHERE key = 'schema_version';",
                                &stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_step(&stmt, &has_row);
    assert(result.code == CORE_OK);
    assert(has_row == 1);

    result = core_memdb_stmt_column_text(&stmt, 0, &text_value);
    assert(result.code == CORE_OK);
    assert(core_str_eq(text_value, core_str_from_cstr(expected_text)));

    result = core_memdb_stmt_finalize(&stmt);
    assert(result.code == CORE_OK);
}

static void assert_mem_item_scope_columns(CoreMemDb *db) {
    assert(query_count(db,
                       "SELECT COUNT(*) FROM pragma_table_info('mem_item') "
                       "WHERE name='workspace_key';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM pragma_table_info('mem_item') "
                       "WHERE name='project_key';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM pragma_table_info('mem_item') "
                       "WHERE name='kind';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_item_scope_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_audit';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_audit_session_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_event';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_event_id_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_ts_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_type_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_session_idx';") == 1);
}

static void assert_mem_link_indexes(CoreMemDb *db) {
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_link_from_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_link_to_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_link_kind_idx';") == 1);
    assert(query_count(db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_link_unique_kind_edge';") == 1);
}

static void test_invalid_args(void) {
    CoreMemDb db;
    CoreMemStmt stmt;
    CoreResult result;
    int has_row = -1;
    int64_t i64_value = -1;
    CoreStr text_value;

    result = core_memdb_open(0, &db);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_open("memory.db", 0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_close(0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_exec(0, "select 1;");
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_prepare(0, "select 1;", &stmt);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_step(0, &has_row);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_reset(0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_finalize(0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_i64(0, 1, 42);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_i64(&stmt, 0, 42);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_text(0, 1, "value");
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_text(&stmt, 0, "value");
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_f64(0, 1, 1.0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_f64(&stmt, 0, 1.0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_null(0, 1);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_bind_null(&stmt, 0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_column_i64(0, 0, &i64_value);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_column_i64(&stmt, -1, &i64_value);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_column_text(0, 0, &text_value);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_stmt_column_text(&stmt, -1, &text_value);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_tx_begin(0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_tx_commit(0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_tx_rollback(0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_run_migrations(0, 1u, 0, 0);
    assert(result.code == CORE_ERR_INVALID_ARG);

    result = core_memdb_run_migrations(&db, 0, 0, 0);
    assert(result.code == CORE_ERR_INVALID_ARG);
}

static void test_schema_bootstrap_and_versioning(void) {
    CoreMemDb db;
    CoreMemStmt insert_stmt;
    CoreMemStmt count_stmt;
    CoreResult result;
    int has_row = -1;
    int64_t i64_value = -1;

    remove_test_db();

    result = core_memdb_open(kTestDbPath, &db);
    assert(result.code == CORE_OK);
    assert(db.native_db != 0);

    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_meta';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_item';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_tag';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_item_tag';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_link';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_item_fts';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_item_fingerprint_idx';") == 1);
    assert_schema_version(&db, "6");
    assert_mem_item_scope_columns(&db);
    assert_mem_link_indexes(&db);

    result = core_memdb_exec(&db,
                             "CREATE TABLE notes ("
                             "id INTEGER PRIMARY KEY,"
                             "title TEXT NOT NULL"
                             ");");
    assert(result.code == CORE_OK);

    result = core_memdb_prepare(&db, "INSERT INTO notes (title) VALUES (?1);", &insert_stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_bind_text(&insert_stmt, 1, "alpha");
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_step(&insert_stmt, &has_row);
    assert(result.code == CORE_OK);
    assert(has_row == 0);

    result = core_memdb_stmt_finalize(&insert_stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_tx_begin(&db);
    assert(result.code == CORE_OK);

    result = core_memdb_exec(&db, "INSERT INTO notes (title) VALUES ('rolled-back');");
    assert(result.code == CORE_OK);

    result = core_memdb_tx_rollback(&db);
    assert(result.code == CORE_OK);

    result = core_memdb_prepare(&db,
                                "SELECT COUNT(*) FROM notes WHERE title = ?1;",
                                &count_stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_bind_text(&count_stmt, 1, "rolled-back");
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_step(&count_stmt, &has_row);
    assert(result.code == CORE_OK);
    assert(has_row == 1);

    result = core_memdb_stmt_column_i64(&count_stmt, 0, &i64_value);
    assert(result.code == CORE_OK);
    assert(i64_value == 0);

    result = core_memdb_stmt_finalize(&count_stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_tx_begin(&db);
    assert(result.code == CORE_OK);

    result = core_memdb_exec(&db, "INSERT INTO notes (title) VALUES ('committed');");
    assert(result.code == CORE_OK);

    result = core_memdb_tx_commit(&db);
    assert(result.code == CORE_OK);

    result = core_memdb_prepare(&db,
                                "SELECT COUNT(*) FROM notes WHERE title = ?1;",
                                &count_stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_bind_text(&count_stmt, 1, "committed");
    assert(result.code == CORE_OK);

    result = core_memdb_stmt_step(&count_stmt, &has_row);
    assert(result.code == CORE_OK);
    assert(has_row == 1);

    result = core_memdb_stmt_column_i64(&count_stmt, 0, &i64_value);
    assert(result.code == CORE_OK);
    assert(i64_value == 1);

    result = core_memdb_stmt_finalize(&count_stmt);
    assert(result.code == CORE_OK);

    result = core_memdb_close(&db);
    assert(result.code == CORE_OK);
    assert(db.native_db == 0);

    result = core_memdb_open(kTestDbPath, &db);
    assert(result.code == CORE_OK);
    assert(db.native_db != 0);
    assert(query_count(&db, "SELECT COUNT(*) FROM mem_item_fts;") == 0);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_item_fingerprint_idx';") == 1);
    assert_schema_version(&db, "6");
    assert_mem_item_scope_columns(&db);
    assert_mem_link_indexes(&db);

    result = core_memdb_run_migrations(&db, 6u, 0, 0);
    assert(result.code == CORE_OK);

    result = core_memdb_exec(&db, "UPDATE mem_meta SET value = '2' WHERE key = 'schema_version';");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_item_fingerprint_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_item_scope_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_audit_session_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_event_event_id_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_event_ts_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_event_type_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_event_session_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_link_from_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_link_to_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_link_kind_idx;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP INDEX IF EXISTS mem_link_unique_kind_edge;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP TABLE IF EXISTS mem_audit;");
    assert(result.code == CORE_OK);
    result = core_memdb_exec(&db, "DROP TABLE IF EXISTS mem_event;");
    assert(result.code == CORE_OK);

    result = core_memdb_close(&db);
    assert(result.code == CORE_OK);

    result = core_memdb_open(kTestDbPath, &db);
    assert(result.code == CORE_OK);
    assert(db.native_db != 0);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_item_fingerprint_idx';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_item_scope_idx';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_audit';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_audit_session_idx';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='table' AND name='mem_event';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_event_id_idx';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_ts_idx';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_type_idx';") == 1);
    assert(query_count(&db,
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type='index' AND name='mem_event_session_idx';") == 1);
    assert_mem_link_indexes(&db);
    assert_schema_version(&db, "6");
    assert_mem_item_scope_columns(&db);

    result = core_memdb_exec(&db, "UPDATE mem_meta SET value = '99' WHERE key = 'schema_version';");
    assert(result.code == CORE_OK);

    result = core_memdb_close(&db);
    assert(result.code == CORE_OK);

    result = core_memdb_open(kTestDbPath, &db);
    assert(result.code == CORE_ERR_FORMAT);
    assert(db.native_db == 0);

    remove_test_db();
}

static void test_v2_to_v6_upgrade(void) {
    CoreMemDb db;
    CoreResult result;

    create_legacy_v2_db(kTestDbPath);

    result = core_memdb_open(kTestDbPath, &db);
    assert(result.code == CORE_OK);
    assert(db.native_db != 0);
    assert_schema_version(&db, "6");
    assert_mem_item_scope_columns(&db);
    assert_mem_link_indexes(&db);

    result = core_memdb_close(&db);
    assert(result.code == CORE_OK);

    remove_test_db();
}

int main(void) {
    test_invalid_args();
    test_schema_bootstrap_and_versioning();
    test_v2_to_v6_upgrade();
    return 0;
}
