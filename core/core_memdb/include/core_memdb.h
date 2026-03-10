#ifndef CORE_MEMDB_H
#define CORE_MEMDB_H

#include <stdint.h>

#include "core_base.h"

typedef struct CoreMemDb {
    void *native_db;
} CoreMemDb;

typedef struct CoreMemStmt {
    void *native_stmt;
    CoreMemDb *db;
} CoreMemStmt;

typedef CoreResult (*CoreMemDbMigrationFn)(CoreMemDb *db,
                                           uint32_t from_version,
                                           uint32_t to_version,
                                           void *user_ctx);

CoreResult core_memdb_open(const char *path, CoreMemDb *out_db);
CoreResult core_memdb_close(CoreMemDb *db);

CoreResult core_memdb_exec(CoreMemDb *db, const char *sql);
CoreResult core_memdb_prepare(CoreMemDb *db, const char *sql, CoreMemStmt *out_stmt);
CoreResult core_memdb_stmt_step(CoreMemStmt *stmt, int *out_has_row);
CoreResult core_memdb_stmt_reset(CoreMemStmt *stmt);
CoreResult core_memdb_stmt_finalize(CoreMemStmt *stmt);

/*
 * Bind indices are 1-based, matching SQLite.
 */
CoreResult core_memdb_stmt_bind_i64(CoreMemStmt *stmt, int index_1based, int64_t value);
CoreResult core_memdb_stmt_bind_text(CoreMemStmt *stmt, int index_1based, const char *value);
CoreResult core_memdb_stmt_bind_f64(CoreMemStmt *stmt, int index_1based, double value);
CoreResult core_memdb_stmt_bind_null(CoreMemStmt *stmt, int index_1based);

/*
 * Column indices are 0-based.
 * Text columns are returned as borrowed views that remain valid until the next
 * step/reset/finalize on the same statement.
 */
CoreResult core_memdb_stmt_column_i64(CoreMemStmt *stmt, int column_0based, int64_t *out_value);
CoreResult core_memdb_stmt_column_text(CoreMemStmt *stmt, int column_0based, CoreStr *out_text);

CoreResult core_memdb_tx_begin(CoreMemDb *db);
CoreResult core_memdb_tx_commit(CoreMemDb *db);
CoreResult core_memdb_tx_rollback(CoreMemDb *db);

CoreResult core_memdb_run_migrations(CoreMemDb *db,
                                     uint32_t target_version,
                                     CoreMemDbMigrationFn migration_fn,
                                     void *user_ctx);

#endif
