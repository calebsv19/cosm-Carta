#include "map/terrain_catalog.h"
#include "map/terrain_pack.h"
#include "map/terrain_vectors.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Exercises real catalog admission/removal and complete production tile decoding when requested.
int main(int argc,char **argv) {
    assert(argc==4);
    TerrainCatalog *catalog=calloc(1,sizeof(*catalog)),*before=calloc(1,sizeof(*before));
    assert(catalog&&before);char error[160]={0};
    if(!strcmp(argv[1],"reject")) {
        assert(terrain_catalog_load(catalog,argv[2],argv[3],true,error,sizeof(error)));
        *before=*catalog;
        assert(!terrain_catalog_load(catalog,argv[2],argv[3],false,error,sizeof(error)));
        assert(!memcmp(before,catalog,sizeof(*catalog)));
    } else {
        assert(terrain_catalog_load(catalog,argv[2],argv[3],false,error,sizeof(error)));
        assert(catalog->count>=1);
        assert(terrain_catalog_select(catalog,10000000,10000000,10000010,10000010)==-1);
        if(!strcmp(argv[1],"base"))assert(catalog->count==1&&!catalog->additional_bytes);
        else {
            assert(catalog->count==2);
            TerrainArea *area=&catalog->areas[1];
            double x=(area->extent[0]+area->extent[2])*.5,y=(area->extent[1]+area->extent[3])*.5;
            assert(terrain_catalog_select(catalog,x-1,y-1,x+1,y+1)==1);
            assert(!terrain_catalog_remove(catalog,0,error,sizeof(error)));
            if(!strcmp(argv[1],"remove")) {
                char path[4096];snprintf(path,sizeof(path),"%s",area->raster_path);
                assert(terrain_catalog_remove(catalog,1,error,sizeof(error)));
                assert(catalog->count==1&&catalog->additional_bytes<4096&&access(path,F_OK)!=0);
            } else if(!strcmp(argv[1],"stale")) {
                char path[8192];snprintf(path,sizeof(path),"%s/catalog.json",argv[3]);
                FILE *f=fopen(path,"a");assert(f);fputs("\n",f);fclose(f);
                assert(!terrain_catalog_remove(catalog,1,error,sizeof(error)));
                assert(access(area->raster_path,F_OK)==0);
            } else if(!strcmp(argv[1],"decode")) {
                TerrainPack raster={0};TerrainVectors vectors={0};
                assert(terrain_pack_open(&raster,area->raster_path,error,sizeof(error)));
                assert(terrain_vectors_open(&vectors,area->vector_path,error,sizeof(error)));
                unsigned char *rgba=malloc(TERRAIN_RGBA_BYTES);assert(rgba);
                int tiles=0;
                for(int s=0;s<raster.style_count;s++)for(int lod=0;lod<5;lod++)
                    for(int row=0;row<raster.styles[s].levels[lod].rows;row++)
                        for(int col=0;col<raster.styles[s].levels[lod].cols;col++) {
                            assert(terrain_pack_tile(&raster,s,lod,col,row,rgba));tiles++;
                        }
                sqlite3_stmt *query=NULL;assert(sqlite3_prepare_v2(vectors.db,"select area,lod,col,row from tiles",-1,&query,NULL)==SQLITE_OK);
                int contours=0;
                while(sqlite3_step(query)==SQLITE_ROW) {
                    TerrainVectorSegment *segments=NULL;size_t count=0;
                    assert(terrain_vectors_tile(&vectors,sqlite3_column_int(query,0),sqlite3_column_int(query,1),sqlite3_column_int(query,2),sqlite3_column_int(query,3),&segments,&count));
                    free(segments);contours++;
                }
                sqlite3_finalize(query);free(rgba);
                printf("decoded %d raster tiles, %d contour tiles and %zu official labels\n",tiles,contours,vectors.label_count);
                assert(vectors.label_count>0);terrain_pack_close(&raster);terrain_vectors_close(&vectors);
            }
        }
    }
    free(before);free(catalog);puts("terrain catalog policy passed");return 0;
}
