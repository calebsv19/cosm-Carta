#include "app/history.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
// Retrieves bounded source bytes and verifies their content-addressed identity.
bool history_asset_load(const char *path, const char *hash, HistoryTrack *out, char *error, size_t cap) {
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (!hash || strlen(hash) != 64)
        goto done;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto done;
    if (sqlite3_prepare_v2(db, "SELECT original FROM hiking_assets WHERE hash=?1", -1, &stmt, NULL) !=
        SQLITE_OK)
        goto done;
    sqlite3_bind_text(stmt, 1, hash, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW)
        goto done;
    int n = sqlite3_column_bytes(stmt, 0);
    if (n <= 0 || n > 16 * 1024 * 1024)
        goto done;
    ok = history_parse(sqlite3_column_blob(stmt, 0), (size_t)n, out, error, cap);
    if (ok && strcmp(out->hash, hash)) {
        history_track_free(out);
        ok = false;
    }
done:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (!ok && cap)
        snprintf(error, cap, "History source missing, corrupt or refused");
    return ok;
}
