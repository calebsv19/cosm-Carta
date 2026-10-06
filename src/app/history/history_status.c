#include "app/history_status.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
// Reads a typed metadata field without coercion.
static const char *text(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    return o && json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string)
        ? json_object_get_string(v) : "";
}
// Copies metadata without sharing mutable nodes with the active document.
static struct json_object *copy(struct json_object *o) {
    return json_tokener_parse(json_object_to_json_string_ext(o, JSON_C_TO_STRING_PLAIN));
}
// Creates a typed command result.
static struct json_object *result(bool ok, const char *s) {
    struct json_object *r = json_object_new_object();
    json_object_object_add(r, "ok", json_object_new_boolean(ok));
    json_object_object_add(r, ok ? "status" : "error", json_object_new_string(s));
    return r;
}
// Returns a validated array field.
static struct json_object *items(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    json_object_object_get_ex(o, key, &v);
    return v && json_object_is_type(v, json_type_array) ? v : NULL;
}
// Resolves one stable identity without matching names approximately.
static struct json_object *find(struct json_object *a, const char *id) {
    for (size_t i = 0; a && i < json_object_array_length(a); i++) {
        struct json_object *v = json_object_array_get_idx(a, i);
        if (*id && !strcmp(text(v, "id"), id)) return v;
    }
    return NULL;
}
// Converts explicit legacy visit states without guessing from downloaded routes.
static const char *direct(struct json_object *v) {
    const char *p = text(v, "progress");
    if (*p) return history_progress_valid(p) ? p : "unreviewed";
    const char *s = text(v, "state"), *k = text(v, "kind");
    if (!strcmp(s, "summited")) return "completed";
    if (!strcmp(s, "wishlist")) return "planned";
    if (!strcmp(s, "unvisited")) return "not_done";
    if (!strcmp(s, "visited")) return !strcmp(k, "peak") ? "partial" : "completed";
    return "unreviewed";
}
bool history_progress_valid(const char *v) {
    return v && (!strcmp(v,"completed") || !strcmp(v,"partial") || !strcmp(v,"planned") ||
                 !strcmp(v,"unreviewed") || !strcmp(v,"not_done"));
}
const char *history_progress(struct json_object *root, struct json_object *v) {
    const char *p = direct(v), *kind = text(v, "kind");
    if (*text(v,"progress") || (strcmp(kind,"recorded") && strcmp(kind,"peak"))) return p;
    struct json_object *a = items(root,"hiking");
    for (size_t i=0; a && i<json_object_array_length(a); i++) {
        struct json_object *other=json_object_array_get_idx(a,i), *archived=NULL;
        json_object_object_get_ex(other,"archived",&archived);
        if(json_object_get_boolean(archived)) continue;
        if(!strcmp(kind,"recorded") && !strcmp(text(other,"recording_id"),text(v,"id")) &&
           !strcmp(text(other,"kind"),"recommended")) return direct(other);
        if(!strcmp(kind,"peak") && !strcmp(text(other,"peak_id"),text(v,"id")) &&
           !strcmp(text(other,"kind"),"visit")) {
            if(!strcmp(text(other,"state"),"summited")) return "completed";
            if(!strcmp(text(other,"state"),"visited")) p="partial";
        }
    }
    return p;
}
bool history_peak_tracked(struct json_object *root, struct json_object *v) {
    struct json_object *tracked=NULL;
    if(json_object_object_get_ex(v,"tracked",&tracked)) return json_object_get_boolean(tracked);
    return strcmp(history_progress(root,v),"unreviewed") && strcmp(history_progress(root,v),"not_done");
}
// Hashes the full proposed document for guarded migration identity.
static void hash(const char *s, char out[32]) {
    uint64_t n=UINT64_C(14695981039346656037);
    for(const unsigned char *p=(const unsigned char *)s; *p; p++){n^=*p;n*=UINT64_C(1099511628211);}
    snprintf(out,32,"%016llx",(unsigned long long)n);
}
// Conservatively normalizes legacy records, preserving import claims rather than fabricated attestations.
static void migrate(struct json_object *root) {
    struct json_object *a=items(root,"hiking");
    for(size_t i=0;a&&i<json_object_array_length(a);i++) {
        struct json_object *v=json_object_array_get_idx(a,i);
        if(!*text(v,"progress") && strcmp(text(v,"kind"),"recorded") && strcmp(text(v,"kind"),"peak"))
            json_object_object_add(v,"progress",json_object_new_string(direct(v)));
        if(!strcmp(text(v,"kind"),"peak") && !*text(v,"state"))
            json_object_object_add(v,"tracked",json_object_new_boolean(history_peak_tracked(root,v)));
        if(!strcmp(text(v,"kind"),"recommended") &&
           !strcmp(text(v,"evidence"),"User states recommended route followed")) {
            json_object_object_add(v,"legacy_import_claim",json_object_new_string(text(v,"evidence")));
            json_object_object_add(v,"evidence",json_object_new_string("Downloaded reference; personal completion requires review"));
        }
    }
    json_object_object_add(root,"hiking_status_version",json_object_new_int(1));
}
// Replaces only the specified collection snapshot while preserving user status corrections.
static bool reconcile(struct json_object *root, struct json_object *q, char *error, size_t cap) {
    struct json_object *incoming=items(q,"collections"), *a=items(root,"hiking"), *stored=items(root,"hiking_collections");
    if(!incoming || json_object_array_length(incoming)>64) {snprintf(error,cap,"Bounded collection snapshots required");return false;}
    if(!stored){stored=json_object_new_array();json_object_object_add(root,"hiking_collections",stored);}
    for(size_t i=0;i<json_object_array_length(incoming);i++) {
        struct json_object *c=json_object_array_get_idx(incoming,i), *members=items(c,"members");
        const char *id=text(c,"id"),*name=text(c,"name"),*role=text(c,"role");
        if(!*id || strlen(id)>128 || !*name || strlen(name)>256 || strlen(text(c,"url"))>2048 ||
           (!strcmp(role,"completed")?false:strcmp(role,"planned") && strcmp(role,"reference")) ||
           !members || json_object_array_length(members)>4096) {snprintf(error,cap,"Invalid collection identity, role or membership");return false;}
        for(size_t j=0;j<json_object_array_length(members);j++) {
            struct json_object *member=json_object_array_get_idx(members,j);
            if(!json_object_is_type(member,json_type_string) || !find(a,json_object_get_string(member))) {
                snprintf(error,cap,"Unknown collection member; import its reference metadata first");return false;
            }
        }
        bool replaced=false;
        for(size_t j=0;j<json_object_array_length(stored);j++) if(!strcmp(text(json_object_array_get_idx(stored,j),"id"),id)) {
            json_object_array_put_idx(stored,j,copy(c));replaced=true;break;
        }
        if(!replaced){if(json_object_array_length(stored)>=64){snprintf(error,cap,"Collection capacity reached");return false;}json_object_array_add(stored,copy(c));}
    }
    size_t edges=0;
    for(size_t i=0;i<json_object_array_length(stored);i++) {
        struct json_object *members=items(json_object_array_get_idx(stored,i),"members");
        edges+=members?json_object_array_length(members):0;
    }
    if(edges>8192){snprintf(error,cap,"Collection membership budget exceeded");return false;}
    // Rebuild all memberships, deriving claims only from retained explicit collection snapshots.
    for(size_t i=0;a&&i<json_object_array_length(a);i++) {
        struct json_object *v=json_object_array_get_idx(a,i),*membership=json_object_new_array();
        bool done=false,planned=false;
        for(size_t j=0;j<json_object_array_length(stored);j++) {
            struct json_object *c=json_object_array_get_idx(stored,j),*members=items(c,"members");
            for(size_t k=0;members&&k<json_object_array_length(members);k++) if(!strcmp(json_object_get_string(json_object_array_get_idx(members,k)),text(v,"id"))) {
                json_object_array_add(membership,json_object_new_string(text(c,"id")));
                done|=!strcmp(text(c,"role"),"completed");planned|=!strcmp(text(c,"role"),"planned");break;
            }
        }
        json_object_object_add(v,"collections",membership);
        if(strcmp(text(v,"status_origin"),"user") && strcmp(text(v,"kind"),"recorded") && strcmp(text(v,"kind"),"peak") && strcmp(text(v,"kind"),"visit")) {
            json_object_object_add(v,"progress",json_object_new_string(done?"completed":planned?"planned":"unreviewed"));
            json_object_object_add(v,"status_origin",json_object_new_string("collection"));
            json_object_object_add(v,"status_evidence",json_object_new_string(done?"Member of the inspected AllTrails Completed list":planned?"Member of a user saved goal list":"Reference outside reconciled personal lists"));
        }
    }
    return true;
}
struct json_object *history_collections_command(const char *path, MapForgePinsFile *file, struct json_object *q) {
    const char *op=text(q,"operation");
    bool migration=!strncmp(op,"history.migrate.",16),preview=strstr(op,".preview")!=NULL;
    if(strcmp(op,migration?(preview?"history.migrate.preview":"history.migrate.commit"):(preview?"history.collections.preview":"history.collections.commit"))) return result(false,"Unknown collection operation");
    if(file->write_blocked || !map_forge_places_is_database(path)) return result(false,"Writable personal repository required");
    const char *rid=text(q,"request_id");
    if(!*rid || strlen(rid)>128) return result(false,"Bounded request_id required");
    struct json_object *version=NULL,*raw=NULL;
    if(json_object_object_get_ex(file->source_json,"hiking_status_version",&version) && (!json_object_is_type(version,json_type_int)||json_object_get_int(version)!=1))return result(false,"Unsupported status schema; preserve it");
    if(json_object_object_get_ex(file->source_json,"hiking_collections",&raw) && !json_object_is_type(raw,json_type_array))return result(false,"Unsupported collections; preserve them");
    struct json_object *canonical=copy(q);
    json_object_object_del(canonical,"operation");json_object_object_del(canonical,"candidate_token");json_object_object_del(canonical,"expected_store_token");
    char fingerprint[32];hash(json_object_to_json_string_ext(canonical,JSON_C_TO_STRING_PLAIN),fingerprint);json_object_put(canonical);
    struct json_object *receipts=items(file->source_json,"hiking_collection_receipts"),*prior=find(receipts,rid),*saved=NULL;
    if(!preview && prior){if(strcmp(text(prior,"fingerprint"),fingerprint))return result(false,"Collection request_id reused");json_object_object_get_ex(prior,"reply",&saved);return copy(saved);}
    struct json_object *root=copy(file->source_json);char error[256]={0};
    migrate(root);
    if(!migration && !reconcile(root,q,error,sizeof(error))){json_object_put(root);return result(false,error);}
    char ct[32],st[32];hash(json_object_to_json_string_ext(root,JSON_C_TO_STRING_PLAIN),ct);snprintf(st,32,"%016llx",(unsigned long long)file->source_signature);
    struct json_object *r=result(true,preview?"preview":"saved");
    json_object_object_add(r,"candidate_token",json_object_new_string(ct));json_object_object_add(r,"store_token",json_object_new_string(st));
    json_object_object_add(r,"records",copy(items(root,"hiking")));
    struct json_object *collections=items(root,"hiking_collections");
    json_object_object_add(r,"collections",collections?copy(collections):json_object_new_array());
    if(preview){json_object_put(root);return r;}
    if(strcmp(ct,text(q,"candidate_token")) || strcmp(st,text(q,"expected_store_token"))){json_object_put(root);json_object_put(r);return result(false,"Collection preview changed or stale");}
    receipts=items(root,"hiking_collection_receipts");
    if(!receipts){receipts=json_object_new_array();json_object_object_add(root,"hiking_collection_receipts",receipts);}
    if(json_object_array_length(receipts)>=256){json_object_put(root);json_object_put(r);return result(false,"Collection receipt capacity reached");}
    struct json_object *receipt=json_object_new_object();
    json_object_object_add(receipt,"id",json_object_new_string(rid));json_object_object_add(receipt,"fingerprint",json_object_new_string(fingerprint));
    // Keep receipts compact; callers can read the current catalog independently.
    struct json_object *compact=result(true,"saved");json_object_object_add(compact,"request_id",json_object_new_string(rid));
    json_object_object_add(receipt,"reply",copy(compact));json_object_array_add(receipts,receipt);
    bool ok=map_forge_places_save_asset(path,root,file->source_signature,NULL,NULL,error,sizeof(error));json_object_put(root);json_object_put(r);
    if(ok)ok=map_forge_places_load(path,file,error,sizeof(error));
    if(!ok){json_object_put(compact);return result(false,error);}return compact;
}
