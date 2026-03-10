#ifndef MEM_CLI_DB_HELPERS_H
#define MEM_CLI_DB_HELPERS_H

#include <stddef.h>
#include <stdint.h>

#include "core_memdb.h"

int core_str_equals_cstr(CoreStr value, const char *text);
const char *core_str_data_or_empty(CoreStr value);

CoreResult bind_optional_text_or_null(CoreMemStmt *stmt, int index_1based, const char *value);
int append_sql_fragment(char *sql, size_t sql_cap, const char *fragment);
int64_t current_time_ns(void);

CoreResult query_single_i64(CoreMemDb *db, const char *sql, int64_t *out_value);
int open_db_or_fail(const char *db_path, CoreMemDb *db);
CoreResult fetch_changes(CoreMemDb *db, int64_t *out_changes);
CoreResult item_exists_active(CoreMemDb *db, int64_t item_id, int *out_exists);
CoreResult fetch_session_add_write_count(CoreMemDb *db,
                                         const char *session_id,
                                         int64_t *out_count);
CoreResult append_audit_entry(CoreMemDb *db,
                              const char *session_id,
                              const char *action,
                              const char *status,
                              int64_t item_id,
                              const char *stable_id,
                              const char *workspace_key,
                              const char *project_key,
                              const char *item_kind,
                              const char *detail);
CoreResult append_event_entry(CoreMemDb *db,
                              const char *session_id,
                              const char *event_type,
                              int64_t item_id,
                              int64_t link_id,
                              const char *stable_id,
                              const char *workspace_key,
                              const char *project_key,
                              const char *item_kind,
                              const char *payload_json);
CoreResult sync_fts_row(CoreMemDb *db, int64_t item_id, const char *title, const char *body);
CoreResult find_duplicate_item(CoreMemDb *db,
                               const char *fingerprint,
                               int64_t *out_item_id,
                               int *out_found);
CoreResult fetch_item_audit_metadata(CoreMemDb *db,
                                     int64_t item_id,
                                     char *out_stable_id,
                                     size_t stable_id_cap,
                                     char *out_workspace,
                                     size_t workspace_cap,
                                     char *out_project,
                                     size_t project_cap,
                                     char *out_kind,
                                     size_t kind_cap);
CoreResult fetch_link_endpoints(CoreMemDb *db,
                                int64_t link_id,
                                int64_t *out_from_item_id,
                                int64_t *out_to_item_id);
CoreResult fetch_item_snapshot_json_alloc(CoreMemDb *db,
                                          int64_t item_id,
                                          char **out_json);
CoreResult fetch_link_snapshot_json_alloc(CoreMemDb *db,
                                          int64_t link_id,
                                          char **out_json);
CoreResult fetch_next_item_id(CoreMemDb *db, int64_t *out_item_id);
CoreResult fetch_next_link_id(CoreMemDb *db, int64_t *out_link_id);
CoreResult sqlite_master_object_exists(CoreMemDb *db,
                                       const char *type_name,
                                       const char *object_name,
                                       int *out_exists);

#endif
