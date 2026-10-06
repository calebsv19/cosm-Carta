#include "app/place_maintenance.h"
#include "app/history.h"
#include <fcntl.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Reads maintenance request text without coercion.
static const char *maintenance_text(struct json_object *v, const char *key) {
    struct json_object *x = NULL;
    return v && json_object_object_get_ex(v, key, &x) && json_object_is_type(x, json_type_string)
               ? json_object_get_string(x)
               : "";
}
// Reports explicit local storage refusal.
static struct json_object *maintenance_reply(bool ok, const char *message) {
    struct json_object *r = json_object_new_object();
    json_object_object_add(r, "ok", json_object_new_boolean(ok));
    if (message)
        json_object_object_add(r, ok ? "status" : "error", json_object_new_string(message));
    return r;
}
// Formats exact-document signatures as opaque comparison tokens.
static void maintenance_token(uint64_t signature, char token[32]) {
    snprintf(token, 32, "%016llx", (unsigned long long)signature);
}
// Validates SQLite identity/integrity, foreign keys and bounded canonical payload before use.
static bool maintenance_validate(const char *path, MapForgePinsFile *file, char *error, size_t cap) {
    struct stat st;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    bool ok = false;
    if (lstat(path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > 256 * 1024 * 1024)
        goto done;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
        goto done;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW || strcmp((const char *)sqlite3_column_text(stmt, 0), "ok"))
        goto done;
    sqlite3_finalize(stmt);
    stmt = NULL;
    if (sqlite3_prepare_v2(db, "PRAGMA foreign_key_check", -1, &stmt, NULL) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_DONE)
        goto done;
    sqlite3_finalize(stmt);
    stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT length(CAST(payload AS BLOB)) FROM document WHERE singleton=1", -1,
                           &stmt, NULL) != SQLITE_OK ||
        sqlite3_step(stmt) != SQLITE_ROW || sqlite3_column_int64(stmt, 0) > 16 * 1024 * 1024)
        goto done;
    ok = map_forge_places_load(path, file, error, cap) && !file->write_blocked;
    struct json_object *hiking = NULL, *version = NULL;
    if (ok && json_object_object_get_ex(file->source_json, "hiking", &hiking)) {
        ok = json_object_is_type(hiking, json_type_array) &&
             json_object_object_get_ex(file->source_json, "hiking_version", &version) &&
             json_object_is_type(version, json_type_int) && json_object_get_int(version) == 1;
        for (size_t i = 0; ok && i < json_object_array_length(hiking); i++) {
            struct json_object *record = json_object_array_get_idx(hiking, i);
            const char *hash = maintenance_text(record, "asset_hash");
            if (!*hash)
                continue;
            bool checked = false;
            for (size_t j = 0; j < i; j++)
                if (!strcmp(hash, maintenance_text(json_object_array_get_idx(hiking, j), "asset_hash")))
                    checked = true;
            if (!checked) {
                HistoryTrack track = {0};
                ok = history_asset_load(path, hash, &track, error, cap);
                history_track_free(&track);
            }
        }
    }
done:
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (!ok && !error[0])
        snprintf(error, cap, "Backup identity, size or integrity refused");
    return ok;
}
// Creates a consistent SQLite snapshot without overwriting existing files or copying map packs.
static bool maintenance_backup(const char *source, const char *destination, char *error, size_t cap) {
    if (destination[0] != '/' || strlen(destination) >= MAPFORGE_PIN_PATH_CAPACITY ||
        !map_forge_places_is_database(destination)) {
        snprintf(error, cap, "An absolute .sqlite destination is required");
        return false;
    }
    int fd = open(destination, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (fd < 0) {
        snprintf(error, cap, "Destination exists or cannot be created");
        return false;
    }
    close(fd);
    sqlite3 *a = NULL, *b = NULL;
    sqlite3_backup *backup = NULL;
    bool ok = false;
    if (sqlite3_open_v2(source, &a, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK ||
        sqlite3_open_v2(destination, &b, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
        goto done;
    backup = sqlite3_backup_init(b, "main", a, "main");
    if (!backup)
        goto done;
    int result = sqlite3_backup_step(backup, -1);
    int finish = sqlite3_backup_finish(backup);
    backup = NULL;
    ok = result == SQLITE_DONE && finish == SQLITE_OK;
done:
    if (backup)
        sqlite3_backup_finish(backup);
    sqlite3_close(a);
    sqlite3_close(b);
    if (ok) {
        MapForgePinsFile check;
        map_forge_pins_file_init(&check);
        ok = maintenance_validate(destination, &check, error, cap);
        map_forge_pins_file_free(&check);
    }
    if (!ok) {
        unlink(destination);
        if (!error[0])
            snprintf(error, cap, "Snapshot failed; source preserved");
    }
    return ok;
}
// Adds human-readable record counts to an export or restore preview.
static void maintenance_counts(struct json_object *reply, const MapForgePinsFile *file) {
    json_object_object_add(reply, "places", json_object_new_int64((int64_t)file->pin_count));
    const char *keys[] = {"pets", "services", "route_plans", "hiking"};
    for (size_t i = 0; i < 4; ++i) {
        struct json_object *v = NULL;
        json_object_object_get_ex(file->source_json, keys[i], &v);
        json_object_object_add(reply, keys[i],
                               json_object_new_int64(v && json_object_is_type(v, json_type_array)
                                                         ? (int64_t)json_object_array_length(v)
                                                         : 0));
    }
    struct json_object *h = NULL;
    json_object_object_get_ex(file->source_json, "hiking", &h);
    int64_t peaks = 0, visits = 0;
    for (size_t i = 0; h && json_object_is_type(h, json_type_array) && i < json_object_array_length(h); i++) {
        const char *kind = maintenance_text(json_object_array_get_idx(h, i), "kind");
        peaks += !strcmp(kind, "peak");
        visits += !strcmp(kind, "visit");
    }
    json_object_object_add(reply, "peaks", json_object_new_int64(peaks));
    json_object_object_add(reply, "visits", json_object_new_int64(visits));
}
// Handles private backups/exports and explicit compare-and-swap restore with an undo snapshot.
struct json_object *map_forge_place_maintenance_command(const char *path, MapForgePinsFile *file,
                                                        struct json_object *request) {
    const char *op = maintenance_text(request, "operation");
    char error[256] = {0}, token[32];
    maintenance_token(file->source_signature, token);
    if (!strcmp(op, "repository.backup") || !strcmp(op, "repository.export")) {
        const char *dest = maintenance_text(request, "destination");
        bool backup = !strcmp(op, "repository.backup");
        if (backup) {
            if (!maintenance_backup(path, dest, error, sizeof(error)))
                return maintenance_reply(false, error);
        } else {
            if (dest[0] != '/' || strlen(dest) >= MAPFORGE_PIN_PATH_CAPACITY)
                return maintenance_reply(false, "Absolute export destination required");
            int fd = open(dest, O_CREAT | O_EXCL | O_WRONLY, 0600);
            if (fd < 0)
                return maintenance_reply(false, "Export destination exists or cannot be created");
            const char *text = json_object_to_json_string_ext(file->source_json, JSON_C_TO_STRING_PRETTY |
                                                                                     JSON_C_TO_STRING_SPACED);
            size_t n = strlen(text), offset = 0;
            bool ok = true;
            while (offset < n) {
                ssize_t count = write(fd, text + offset, n - offset);
                if (count <= 0) {
                    ok = false;
                    break;
                }
                offset += (size_t)count;
            }
            if (fsync(fd))
                ok = false;
            if (close(fd))
                ok = false;
            if (!ok) {
                unlink(dest);
                return maintenance_reply(false, "Export failed; source preserved");
            }
        }
        struct json_object *r = maintenance_reply(true, backup ? "backup_verified" : "exported");
        json_object_object_add(r, "destination", json_object_new_string(dest));
        json_object_object_add(r, "store_token", json_object_new_string(token));
        json_object_object_add(r, "assets_included", json_object_new_boolean(backup));
        maintenance_counts(r, file);
        return r;
    }
    bool preview = !strcmp(op, "repository.restore.preview");
    if (!preview && strcmp(op, "repository.restore"))
        return maintenance_reply(false, "Unknown repository operation");
    if (!preview && file->write_blocked)
        return maintenance_reply(false,
                                 "Current personal records are read-only; preserve them before recovery");
    const char *source = maintenance_text(request, "source");
    if (source[0] != '/' || !map_forge_places_is_database(source) || !strcmp(source, path))
        return maintenance_reply(false, "Select a separate absolute .sqlite backup");
    MapForgePinsFile snapshot;
    map_forge_pins_file_init(&snapshot);
    if (!maintenance_validate(source, &snapshot, error, sizeof(error))) {
        map_forge_pins_file_free(&snapshot);
        return maintenance_reply(false, error);
    }
    char source_token[32];
    maintenance_token(snapshot.source_signature, source_token);
    struct json_object *reply = maintenance_reply(true, preview ? "restore_preview" : "restored");
    json_object_object_add(reply, "source", json_object_new_string(source));
    json_object_object_add(reply, "source_token", json_object_new_string(source_token));
    json_object_object_add(reply, "store_token", json_object_new_string(token));
    maintenance_counts(reply, &snapshot);
    if (preview) {
        map_forge_pins_file_free(&snapshot);
        return reply;
    }
    const char *rid = maintenance_text(request, "request_id");
    struct json_object *last = NULL;
    json_object_object_get_ex(file->source_json, "last_restore_receipt", &last);
    if (last && !strcmp(maintenance_text(last, "request_id"), rid) && rid[0]) {
        bool replay = !strcmp(maintenance_text(last, "source"), source) &&
                      !strcmp(maintenance_text(last, "source_token"), source_token) &&
                      !strcmp(maintenance_text(last, "expected_store_token"),
                              maintenance_text(request, "expected_store_token"));
        json_object_put(reply);
        map_forge_pins_file_free(&snapshot);
        if (!replay)
            return maintenance_reply(false, "Restore request_id reused with different input");
        return json_tokener_parse(json_object_to_json_string_ext(last, JSON_C_TO_STRING_PLAIN));
    }
    if (!rid[0] || strlen(rid) > 128 || strcmp(token, maintenance_text(request, "expected_store_token")) ||
        strcmp(source_token, maintenance_text(request, "source_token"))) {
        json_object_put(reply);
        map_forge_pins_file_free(&snapshot);
        return maintenance_reply(false, "Restore preview changed or is stale");
    }
    char undo[MAPFORGE_PIN_PATH_CAPACITY];
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    int n = snprintf(undo, sizeof(undo), "%s.before-restore-%lld-%ld.sqlite", path, (long long)now.tv_sec,
                     now.tv_nsec);
    if (n <= 0 || (size_t)n >= sizeof(undo) || !maintenance_backup(path, undo, error, sizeof(error))) {
        json_object_put(reply);
        map_forge_pins_file_free(&snapshot);
        return maintenance_reply(false, error[0] ? error : "Recovery path too long");
    }
    json_object_object_add(reply, "request_id", json_object_new_string(rid));
    json_object_object_add(reply, "expected_store_token", json_object_new_string(token));
    json_object_object_add(reply, "recovery_backup", json_object_new_string(undo));
    json_object_object_add(snapshot.source_json, "last_restore_receipt",
                           json_tokener_parse(json_object_to_json_string_ext(reply, JSON_C_TO_STRING_PLAIN)));
    bool ok = map_forge_places_save_asset(path, snapshot.source_json, file->source_signature, NULL, source,
                                          error, sizeof(error));
    map_forge_pins_file_free(&snapshot);
    if (!ok) {
        json_object_put(reply);
        return maintenance_reply(false, error);
    }
    if (!map_forge_pins_load(path, file, error, sizeof(error))) {
        json_object_put(reply);
        return maintenance_reply(false, "Restored; reload required");
    }
    return reply;
}
