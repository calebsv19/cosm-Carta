#define _DARWIN_C_SOURCE
#include "app/app_persist_state.h"
#include "app/workspace_authoring/map_forge_workspace_authoring_host.h"
#include "ui/font.h"
#include "ui/font_bridge.h"
#include "ui/shared_theme_font_adapter.h"
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int opens, fail_after=-1, fail_rename;
/* Refuse after a selected partial font-bank boundary. */
TTF_Font *TTF_OpenFont(const char *path,int size) {
    TTF_Font *(*real_open)(const char *,int);void *symbol=dlsym(RTLD_NEXT,"TTF_OpenFont");
    memcpy(&real_open,&symbol,sizeof(real_open));assert(real_open);
    if(fail_after>=0 && opens++>=fail_after)return NULL;
    if(fail_after<0)++opens;
    return real_open(path,size);
}
/* Exercise the real atomic JSON owner at its final filesystem replacement. */
int rename(const char *from,const char *to) {
    int (*real_rename)(const char *,const char *);void *symbol=dlsym(RTLD_NEXT,"rename");
    memcpy(&real_rename,&symbol,sizeof(real_rename));assert(real_rename);
    if(fail_rename){errno=EIO;return -1;}return real_rename(from,to);
}
/* Forward authoring acceptance to the actual app persistence owner. */
static bool accept(void *user) {return app_save_persisted_view_state(user);}
/* Compare exact live handles at every retained layout scale. */
static void same(TTF_Font **saved) {
    const float scales[]={1,.95f,1.5f};
    for(int i=0;i<3;++i){MapForgeResolvedFont font;assert(map_forge_font_bridge_acquire(scales[i],&font));assert(font.font==saved[i]);}
}
/* Verify saved style, camera and unrelated JSON state after accepted Apply. */
static void stored(const char *path,const char *font,int zoom) {
    struct json_object *root=json_object_from_file(path),*view,*value;assert(root);
    assert(json_object_object_get_ex(root,"manual",&value)&&json_object_get_int(value)==42);
    assert(json_object_object_get_ex(root,"map_view",&view));
    assert(json_object_object_get_ex(view,"font_preset",&value)&&!strcmp(json_object_get_string(value),font));
    assert(json_object_object_get_ex(view,"text_zoom_step",&value)&&json_object_get_int(value)==zoom);
    assert(json_object_object_get_ex(view,"zoom",&value)&&json_object_get_double(value)==14);
    json_object_put(root);
}
/* Run the fault/settings corpus separately from real-window pixel qualification. */
int main(int argc,char **argv) {
    assert(argc==3 && TTF_Init()==0);
    SDL_setenv("MAPFORGE_RUNTIME_CONFIG_PATH",argv[2],1);
    AppState *app=calloc(1,sizeof(*app));assert(app);app->view_state_bridge.camera.zoom_target=14;
    char path[384];int size;assert(mapforge_shared_font_resolve_ui_regular(path,sizeof(path),&size));
    if(!strcmp(argv[1],"--reopen")) {
        assert(ui_font_set(path,size));app_load_persisted_view_state(app);
        char preset[64];assert(mapforge_shared_font_current_preset(preset,sizeof(preset)));
        assert(!strcmp(preset,"daw_default")&&mapforge_shared_font_zoom_step()==3);
        assert(app->view_state_bridge.camera.zoom_target==14);stored(argv[2],"daw_default",3);
        ui_font_shutdown(NULL);free(app);TTF_Quit();puts("separate-process style/camera/manual-key reopen PASS");return 0;
    }
    opens=0;fail_after=0;assert(!ui_font_set(path,size));fail_after=-1;
    assert(ui_font_set(path,size));
    const float scales[]={1,.95f,1.5f};TTF_Font *saved[3];
    for(int i=0;i<3;++i){MapForgeResolvedFont f;assert(map_forge_font_bridge_acquire(scales[i],&f));saved[i]=f.font;}
    for(int operation=0;operation<3;++operation) {
        opens=0;fail_after=1;
        bool ok=operation==0?ui_font_set("/missing/font.ttf",13):
            operation==1?mapforge_shared_font_set_zoom_step(3):mapforge_shared_font_set_preset("daw_default");
        assert(!ok);fail_after=-1;same(saved);assert(mapforge_shared_font_zoom_step()==0);
        char preset[64];assert(mapforge_shared_font_current_preset(preset,sizeof(preset))&&!strcmp(preset,"ide"));
    }
    FILE *file=fopen(argv[2],"w");assert(file);fputs("{\"manual\":42}",file);fclose(file);
    assert(app_save_persisted_view_state(app));stored(argv[2],"ide",0);
    MapForgeWorkspaceAuthoringHostState host;map_forge_workspace_authoring_host_reset(&host);
    host.accept=accept;host.accept_user=app;
    assert(map_forge_workspace_authoring_host_enter(&host).code==CORE_OK);
    assert(map_forge_workspace_authoring_host_cycle_overlay(&host).code==CORE_OK);
    opens=0;fail_after=0;
    assert(map_forge_workspace_authoring_host_apply_font_theme_button(&host,KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC));
    assert(host.font_theme_pending_changes==0 && mapforge_shared_font_zoom_step()==0);
    fail_after=-1;same(saved);
    assert(mapforge_shared_font_apply("daw_default",3));
    fail_rename=1;assert(map_forge_workspace_authoring_host_apply(&host).code!=CORE_OK);
    assert(host.active&&host.baseline_valid);stored(argv[2],"ide",0);fail_rename=0;
    opens=0;fail_after=0;assert(map_forge_workspace_authoring_host_cancel(&host).code==CORE_OK);
    assert(opens==0);fail_after=-1;same(saved);assert(mapforge_shared_font_zoom_step()==0);
    assert(map_forge_workspace_authoring_host_enter(&host).code==CORE_OK);
    assert(mapforge_shared_font_apply("daw_default",3));
    assert(map_forge_workspace_authoring_host_apply(&host).code==CORE_OK&&!host.active);
    stored(argv[2],"daw_default",3);
    for(int preset=0;preset<CORE_FONT_PRESET_COUNT;++preset)for(int zoom=-4;zoom<=5;++zoom)
        assert(mapforge_shared_font_apply(core_font_preset_name((CoreFontPresetId)preset),zoom));
    SDL_setenv("MAPFORGE_USE_SHARED_FONT","0",1);
    assert(mapforge_shared_font_apply("ide",5));
    MapForgeResolvedFont legacy;assert(map_forge_font_bridge_acquire(1,&legacy)&&legacy.logical_point_size==10);
    SDL_setenv("MAPFORGE_USE_SHARED_FONT","1",1);
    assert(map_forge_workspace_authoring_host_enter(&host).code==CORE_OK);
    assert(mapforge_shared_font_apply("ide",0));
    ui_font_shutdown(NULL);free(app);TTF_Quit();
    puts("startup/partial failure; exact Cancel; failed atomic Apply; presets/zoom; pending preview shutdown PASS");return 0;
}
