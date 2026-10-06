#include "map/terrain_vectors.h"
#include <json-c/json.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
// Reads only explicitly typed finite JSON numbers.
static bool number(struct json_object *o, double *out) {
    if (!o || (!json_object_is_type(o,json_type_int) && !json_object_is_type(o,json_type_double))) return false;
    *out=json_object_get_double(o);return isfinite(*out);
}
// Reads a bounded exact integer from metadata.
static int integer(struct json_object *o,const char *key,int max) {
    struct json_object *n=NULL;json_object_object_get_ex(o,key,&n);
    if(!n || !json_object_is_type(n,json_type_int))return -1;
    int64_t value=json_object_get_int64(n);return value>=0&&value<=max?(int)value:-1;
}
void terrain_vectors_close(TerrainVectors *v) {
    if(!v)return;
    sqlite3_finalize(v->tile);sqlite3_close(v->db);free(v->labels);memset(v,0,sizeof(*v));
}
// Executes a single integer query while finalizing its transient statement.
static int scalar(sqlite3 *db,const char *sql) {
    sqlite3_stmt *q=NULL;int value=-1;
    if(sqlite3_prepare_v2(db,sql,-1,&q,NULL)==SQLITE_OK && sqlite3_step(q)==SQLITE_ROW)value=sqlite3_column_int(q,0);
    sqlite3_finalize(q);return value;
}
bool terrain_vectors_open(TerrainVectors *v,const char *path,char *error,size_t cap) {
    TerrainVectors next={0};struct stat st;sqlite3_stmt *q=NULL;struct json_object *manifest=NULL;
    const char *reason="Invalid terrain vector sidecar";
    if(stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<4096||st.st_size>64*1024*1024)goto fail;
    if(sqlite3_open_v2(path,&next.db,SQLITE_OPEN_READONLY|SQLITE_OPEN_NOMUTEX,NULL)!=SQLITE_OK)goto fail;
    sqlite3_exec(next.db,"pragma query_only=on;pragma cache_size=-1024;pragma mmap_size=0;",NULL,NULL,NULL);
    if(scalar(next.db,"pragma application_id")!=0x4d465456 || scalar(next.db,"pragma user_version")!=1)goto fail;
    if(sqlite3_prepare_v2(next.db,"select value from metadata where key='manifest'",-1,&q,NULL)!=SQLITE_OK || sqlite3_step(q)!=SQLITE_ROW)goto fail;
    int bytes=sqlite3_column_bytes(q,0);if(bytes<2||bytes>128*1024)goto fail;
    manifest=json_tokener_parse((const char*)sqlite3_column_text(q,0));sqlite3_finalize(q);q=NULL;
    struct json_object *crs=NULL,*areas=NULL;json_object_object_get_ex(manifest,"crs",&crs);json_object_object_get_ex(manifest,"areas",&areas);
    if(integer(manifest,"schema",1)!=1 || !crs || !json_object_is_type(crs,json_type_string)||strcmp(json_object_get_string(crs),"EPSG:3857") || !areas || !json_object_is_type(areas,json_type_array))goto fail;
    next.count=json_object_array_length(areas);if(next.count<1||next.count>5)goto fail;
    int total=0;
    for(int i=0;i<next.count;i++) {
        struct json_object *a=json_object_array_get_idx(areas,i),*extent=NULL,*levels=NULL;
        json_object_object_get_ex(a,"extent",&extent);json_object_object_get_ex(a,"levels",&levels);
        if(!extent||!json_object_is_type(extent,json_type_array)||json_object_array_length(extent)!=4||!levels||!json_object_is_type(levels,json_type_array)||json_object_array_length(levels)!=3)goto fail;
        TerrainVectorArea *dest=&next.areas[i];
        for(int j=0;j<4;j++)if(!number(json_object_array_get_idx(extent,j),&dest->extent[j])||fabs(dest->extent[j])>20037509)goto fail;
        if(dest->extent[2]<=dest->extent[0]||dest->extent[3]<=dest->extent[1])goto fail;
        for(int j=0;j<3;j++) {
            struct json_object *level=json_object_array_get_idx(levels,j);int unit=integer(level,"unit",100000),interval=integer(level,"interval_m",1000),tiles=integer(level,"tiles",8192);
            if(unit!=(j==0?2560:j==1?8192:32768)||interval!=(j==0?20:j==1?100:500)||tiles<0)goto fail;
            dest->unit[j]=unit;dest->interval[j]=interval;total+=tiles;
        }
    }
    if(total>8192 || scalar(next.db,"select count(*) from tiles")!=total || integer(manifest,"tile_count",8192)!=total)goto fail;
    int labels=scalar(next.db,"select count(*) from labels");if(labels<0||labels>TERRAIN_VECTOR_LABELS||integer(manifest,"label_count",TERRAIN_VECTOR_LABELS)!=labels)goto fail;
    next.labels=calloc(labels?labels:1,sizeof(*next.labels));if(!next.labels)goto fail;
    if(sqlite3_prepare_v2(next.db,"select name,kind,x,y from labels order by id",-1,&q,NULL)!=SQLITE_OK)goto fail;
    while(sqlite3_step(q)==SQLITE_ROW) {
        if(next.label_count>=(size_t)labels)goto fail;
        TerrainVectorLabel *l=&next.labels[next.label_count++];
        for(int j=0;j<2;j++)if(sqlite3_column_type(q,j)!=SQLITE_TEXT)goto fail;
        int n=sqlite3_column_bytes(q,0),k=sqlite3_column_bytes(q,1);if(n<1||n>120||k<1||k>50)goto fail;
        memcpy(l->name,sqlite3_column_text(q,0),n);memcpy(l->kind,sqlite3_column_text(q,1),k);
        for(int j=2;j<4;j++)if(sqlite3_column_type(q,j)!=SQLITE_FLOAT&&sqlite3_column_type(q,j)!=SQLITE_INTEGER)goto fail;
        l->x=sqlite3_column_double(q,2);l->y=sqlite3_column_double(q,3);
        if(!isfinite(l->x)||!isfinite(l->y)||fabs(l->x)>20037509||fabs(l->y)>20037509)goto fail;
    }
    sqlite3_finalize(q);q=NULL;
    if(next.label_count!=(size_t)labels||sqlite3_prepare_v2(next.db,"select crc,data from tiles where area=? and lod=? and col=? and row=?",-1,&next.tile,NULL)!=SQLITE_OK)goto fail;
    next.bytes=st.st_size;json_object_put(manifest);terrain_vectors_close(v);*v=next;if(error&&cap)error[0]=0;return true;
fail:
    sqlite3_finalize(q);if(manifest)json_object_put(manifest);terrain_vectors_close(&next);
    if(error&&cap)snprintf(error,cap,"%s",reason);return false;
}
int terrain_vectors_select(const TerrainVectors *v,double xmin,double ymin,double xmax,double ymax) {
    int best=0;double cost=INFINITY;
    for(int i=0;i<v->count;i++) {
        const double *e=v->areas[i].extent;double area=(e[2]-e[0])*(e[3]-e[1]);
        if(e[0]<=xmin&&e[1]<=ymin&&e[2]>=xmax&&e[3]>=ymax&&area<cost){best=i;cost=area;}
    }
    return best;
}
// Converts explicit little-endian IEEE754 words without relying on host byte order.
static float word(const unsigned char *p) {
    uint32_t u=(uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;float f;memcpy(&f,&u,4);return f;
}
bool terrain_vectors_tile(TerrainVectors *v,int area,int lod,int col,int row,TerrainVectorSegment **segments,size_t *count) {
    *segments=NULL;*count=0;
    if(!v->db||area<0||area>=v->count||lod<0||lod>2||col<0||row<0)return false;
    TerrainVectorArea *a=&v->areas[area];double width=a->extent[2]-a->extent[0],height=a->extent[3]-a->extent[1];
    if(col>=(int)ceil(width/a->unit[lod])||row>=(int)ceil(height/a->unit[lod]))return false;
    sqlite3_reset(v->tile);sqlite3_clear_bindings(v->tile);
    sqlite3_bind_int(v->tile,1,area);sqlite3_bind_int(v->tile,2,lod);sqlite3_bind_int(v->tile,3,col);sqlite3_bind_int(v->tile,4,row);
    int rc=sqlite3_step(v->tile);if(rc==SQLITE_DONE)return true;if(rc!=SQLITE_ROW)return false;
    int n=sqlite3_column_bytes(v->tile,1);if(n<1||n>512*1024)return false;
    unsigned char *raw=malloc(TERRAIN_VECTOR_BYTES);if(!raw)return false;uLongf size=TERRAIN_VECTOR_BYTES;
    bool ok=uncompress(raw,&size,sqlite3_column_blob(v->tile,1),n)==Z_OK&&size%20==0&&crc32(0,raw,size)==(uint32_t)sqlite3_column_int64(v->tile,0);
    TerrainVectorSegment *out=ok?calloc(size/20?size/20:1,sizeof(*out)):NULL;if(!out)ok=false;
    for(size_t i=0;ok&&i<size/20;i++) {
        const unsigned char *p=raw+i*20;out[i]=(TerrainVectorSegment){word(p),word(p+4),word(p+8),word(p+12),word(p+16)};
        TerrainVectorSegment s=out[i];
        ok=isfinite(s.x0)&&isfinite(s.y0)&&isfinite(s.x1)&&isfinite(s.y1)&&isfinite(s.elevation)&&s.x0>=0&&s.x1>=0&&s.x0<=width&&s.x1<=width&&s.y0<=0&&s.y1<=0&&s.y0>=-height&&s.y1>=-height&&s.elevation>=20&&s.elevation<=10000;
    }
    free(raw);sqlite3_reset(v->tile);if(!ok){free(out);return false;}*segments=out;*count=size/20;return true;
}
