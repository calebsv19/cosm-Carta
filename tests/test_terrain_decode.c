#include "app/terrain_decode.h"
#include <SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Controls an in-flight decoder deterministically instead of racing wall-clock IO.
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static bool hold, entered, fail_tile;
static int live_readers;
static pthread_t render_owner;
// Supplies immutable pack metadata shared by the admission and private worker fixture.
static TerrainPack metadata(void) {
    TerrainPack p = {0}; p.style_count=2; p.bytes=4096;
    for(int i=0;i<2;i++) {
        p.styles[i].mode=i;
        p.styles[i].extent[0]=p.styles[i].extent[1]=-1000;
        p.styles[i].extent[2]=p.styles[i].extent[3]=1000;
        for(int l=0;l<5;l++)p.styles[i].levels[l]=(TerrainLevel){256>>l,256>>l,1,1};
    }
    return p;
}
// Requires every SQLite-like open to happen away from the render owner.
bool terrain_pack_open(TerrainPack *p,const char *path,char *error,size_t cap) {
    (void)error;(void)cap;assert(!pthread_equal(render_owner,pthread_self()));
    *p=metadata();p->db=(sqlite3 *)(uintptr_t)(!strcmp(path,"A")?1:2);
    if(!strcmp(path,"mismatch"))p->styles[0].extent[0]-=1;
    pthread_mutex_lock(&gate);live_readers++;pthread_mutex_unlock(&gate);return true;
}
// Tracks private handle ownership so stale changes and cancellation cannot leak readers.
void terrain_pack_close(TerrainPack *p) {
    if(p->db){pthread_mutex_lock(&gate);live_readers--;pthread_mutex_unlock(&gate);}
    memset(p,0,sizeof(*p));
}
// Holds one read across a source change and tags the source in the returned texels.
bool terrain_pack_tile(TerrainPack *p,int style,int lod,int col,int row,unsigned char *rgba) {
    (void)style;(void)lod;(void)col;(void)row;
    assert(!pthread_equal(render_owner,pthread_self()));
    pthread_mutex_lock(&gate);entered=true;pthread_cond_broadcast(&changed);
    while(hold)pthread_cond_wait(&changed,&gate);
    bool fail=fail_tile;pthread_mutex_unlock(&gate);
    memset(rgba,(int)(uintptr_t)p->db,TERRAIN_RGBA_BYTES);return !fail;
}
// Waits with a finite test deadline for one delivered current-generation buffer.
static TerrainDecodeResult result(TerrainDecode *d) {
    TerrainDecodeResult r={0};
    for(int n=0;n<2000;n++){if(terrain_decode_take(d,&r))return r;SDL_Delay(1);}
    assert(!"decoder completion timed out");return r;
}
// Releases an in-flight read while destruction waits for its owning worker to finish.
static void *release(void *unused) {
    (void)unused;SDL_Delay(10);pthread_mutex_lock(&gate);hold=false;
    pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);return NULL;
}
// Proves capacity, stale generation rejection, invalid metadata, terminal failures and clean joining.
int main(void) {
    render_owner=pthread_self();TerrainPack m=metadata();TerrainDecode *d=terrain_decode_create();assert(d);
    terrain_decode_source(d,"A",&m);
    pthread_mutex_lock(&gate);hold=true;entered=false;pthread_mutex_unlock(&gate);
    for(int col=0;col<8;col++)assert(terrain_decode_request(d,(TerrainDecodeKey){0,0,col,0}));
    assert(!terrain_decode_request(d,(TerrainDecodeKey){0,0,8,0}));
    assert(terrain_decode_stats(d).occupied==8);
    pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&changed,&gate);pthread_mutex_unlock(&gate);
    terrain_decode_source(d,"B",&m);
    pthread_mutex_lock(&gate);hold=false;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
    int n=0;while(!terrain_decode_request(d,(TerrainDecodeKey){0,4,0,0})){assert(n++<2000);SDL_Delay(1);}
    TerrainDecodeResult r=result(d);assert(r.ok&&r.rgba[0]==2);free(r.rgba);
    assert(terrain_decode_stats(d).stale==8);
    terrain_decode_source(d,"mismatch",&m);
    assert(terrain_decode_request(d,(TerrainDecodeKey){0,4,0,0}));r=result(d);assert(!r.ok&&!r.rgba);
    pthread_mutex_lock(&gate);fail_tile=true;pthread_mutex_unlock(&gate);
    terrain_decode_source(d,"A",&m);
    assert(terrain_decode_request(d,(TerrainDecodeKey){0,4,0,0}));r=result(d);assert(!r.ok&&!r.rgba);
    pthread_mutex_lock(&gate);fail_tile=false;pthread_mutex_unlock(&gate);
    terrain_decode_source(d,"B",&m);
    assert(terrain_decode_request(d,(TerrainDecodeKey){0,4,0,0}));r=result(d);assert(r.ok&&r.rgba[0]==2);free(r.rgba);
    pthread_mutex_lock(&gate);hold=true;entered=false;pthread_mutex_unlock(&gate);
    assert(terrain_decode_request(d,(TerrainDecodeKey){0,0,0,0}));
    pthread_mutex_lock(&gate);while(!entered)pthread_cond_wait(&changed,&gate);pthread_mutex_unlock(&gate);
    pthread_t helper;assert(!pthread_create(&helper,NULL,release,NULL));terrain_decode_destroy(d);pthread_join(helper,NULL);
    assert(live_readers==0);
    puts("terrain decoder: eight-slot bound, stale in-flight/queued rejection, metadata drift, failure recovery, render-thread isolation and clean shutdown passed");
}
