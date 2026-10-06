#include "map/terrain_vectors.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Verifies every present tile through the production reader, including replacement and corruption behavior.
int main(int argc,char **argv) {
    assert(argc>=2);TerrainVectors v={0};char error[160]={0};
    assert(terrain_vectors_open(&v,argv[1],error,sizeof(error)));
    assert(v.label_count>0);
    int count=0;sqlite3_stmt *q=NULL;
    assert(sqlite3_prepare_v2(v.db,"select area,lod,col,row from tiles",-1,&q,NULL)==SQLITE_OK);
    while(sqlite3_step(q)==SQLITE_ROW) {
        TerrainVectorSegment *segments=NULL;size_t n=0;
        bool ok=terrain_vectors_tile(&v,sqlite3_column_int(q,0),sqlite3_column_int(q,1),sqlite3_column_int(q,2),sqlite3_column_int(q,3),&segments,&n);
        if(argc==3&&!strcmp(argv[2],"damaged")){assert(!ok);sqlite3_finalize(q);terrain_vectors_close(&v);puts("damaged vector payload rejected");return 0;}
        assert(ok);assert(n*sizeof(*segments)<=TERRAIN_VECTOR_BYTES);free(segments);count++;
    }
    sqlite3_finalize(q);
    TerrainVectorArea a=v.areas[0];
    assert(terrain_vectors_select(&v,a.extent[0]-1,a.extent[1]-1,a.extent[2]+1,a.extent[3]+1)==0);
    sqlite3 *original=v.db;assert(!terrain_vectors_open(&v,"/private/tmp/absent-vector-test",error,sizeof(error)));assert(v.db==original);
    TerrainVectorSegment *empty=NULL;size_t n=0;assert(!terrain_vectors_tile(&v,-1,0,0,0,&empty,&n));
    terrain_vectors_close(&v);printf("verified %d vector tiles and bounded independent labels\n",count);return 0;
}
