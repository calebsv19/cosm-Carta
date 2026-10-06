#include "app/terrain_ui.h"
#include "app/terrain.h"
#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_ui_internal.h"
#include "app/app_persist_state.h"
#include "map/mercator.h"
#include "ui/font.h"
#include "ui/modal_controls.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Keeps only presentation state and a stable pending removal ID, never a history snapshot.
static struct { bool open; int offset; char pending[64], status[256];
    KitUiSurface controls; unsigned scope; char identities[8][64]; } ui;
// Shares responsive panel bounds across drawing and native hit testing.
static SDL_FRect panel(const AppState *app) {
    SDL_FRect view = {0,68,app->width,app->height-68};
    float width = fminf(620, view.w - 32), height = fminf(500, view.h - 32);
    return (SDL_FRect){view.x + (view.w-width)*.5f, view.y + (view.h-height)*.5f, width, height};
}
// Places controls within the bounded panel using one shared hit-test geometry.
static SDL_FRect control(SDL_FRect p, int type, int row) {
    if (type == 0) return (SDL_FRect){p.x+p.w-80,p.y+12,68,26};
    if (type == 1) return (SDL_FRect){p.x+12,p.y+p.h-42,90,28};
    if (type == 2) return (SDL_FRect){p.x+108,p.y+p.h-42,90,28};
    if (type == 3) return (SDL_FRect){p.x+p.w-110,p.y+p.h-42,98,28};
    if (type == 4) return (SDL_FRect){p.x+p.w-170,p.y+108+row*62,66,26};
    if (type == 5) return (SDL_FRect){p.x+p.w-98,p.y+108+row*62,86,26};
    if (type == 6) return (SDL_FRect){p.x+12,p.y+p.h-108,120,28};
    return (SDL_FRect){p.x+140,p.y+p.h-108,88,28};
}
static uint64_t action_key(int type,int row) {return type<4 || type>=6 ? (uint64_t)type : 100u+row*2u+(type==5);}
static void button(AppState *app, SDL_FRect box, const char *text, bool active,int type,int row) {
    carta_ui_control_draw_surface(app,&ui.controls,box,text,active,(KitUiSurfaceKey){1,action_key(type,row)},.68f);
}
static void sync(AppState *app) {
    SDL_FRect p=panel(app),clip={0,0,app->width,app->height};
    const TerrainCatalog *c=terrain_area_catalog();
    int rows=(int)fmax(1,floor((p.h-200)/62));
    ui.offset=fmin(ui.offset,fmax(0,(int)c->count-rows));
    char identities[8][64]={{0}};
    for(int i=0;i<rows && i<8 && ui.offset+i<(int)c->count;i++)
        snprintf(identities[i],sizeof(identities[i]),"%s",c->areas[ui.offset+i].id);
    if(memcmp(identities,ui.identities,sizeof(identities))) {
        memcpy(ui.identities,identities,sizeof(identities));kit_ui_surface_set_scope(&ui.controls,++ui.scope);
    }
    carta_modal_controls_begin(&ui.controls,ui.scope?ui.scope:1);
    carta_modal_controls_add(&ui.controls,0,control(p,0,0),clip,true);
    if(*ui.pending) {
        carta_modal_controls_add(&ui.controls,6,control(p,6,0),clip,true);
        carta_modal_controls_add(&ui.controls,7,control(p,7,0),clip,true);
    } else {
        for(int i=1;i<4;i++)carta_modal_controls_add(&ui.controls,i,control(p,i,0),clip,true);
        for(int i=0;i<rows && ui.offset+i<(int)c->count;i++) {
            carta_modal_controls_add(&ui.controls,action_key(4,i),control(p,4,i),clip,true);
            if(c->areas[ui.offset+i].removable)carta_modal_controls_add(&ui.controls,action_key(5,i),control(p,5,i),clip,true);
        }
    }
    carta_modal_controls_end(&ui.controls);
}
// Draws a single clipped panel line using the current theme and font path.
static void line(AppState *app, SDL_FRect p, float y, const char *text, bool muted) {
    MapForgeThemePalette palette=app_ui_theme_palette();
    ui_draw_text_clipped(&app->renderer,p.x+14,y,text,muted?palette.text_muted:palette.text_primary,.70f,p.w-28);
}
SDL_FRect terrain_ui_control_rect(const AppState *app,TerrainUIControl type,int row) {
    return control(panel(app),type,row);
}
void terrain_ui_cancel_transient(void){kit_ui_surface_set_scope(&ui.controls,++ui.scope);}
void terrain_ui_close(void) { memset(&ui,0,sizeof(ui)); }
void terrain_ui_open(void) { ui.open=true; ui.offset=0; ui.pending[0]=ui.status[0]=0; }
bool terrain_ui_active(void) { return ui.open; }
// Fits an area's full extent through the existing map projection without changing selected routes.
static void show(AppState *app, const TerrainArea *area) {
    SDL_FRect view=app_map_viewport_rect(app);
    double width=area->extent[2]-area->extent[0],height=area->extent[3]-area->extent[1];
    double ppm=fmin(fmax(1,view.w-80)/width,fmax(1,view.h-100)/height);
    Camera *c=&app->view_state_bridge.camera;
    c->x=c->x_target=(area->extent[0]+area->extent[2])*.5;
    c->y=c->y_target=(area->extent[1]+area->extent[3])*.5;
    c->zoom=c->zoom_target=fmin(18,fmax(1,log2(ppm*mercator_world_size_meters()/256)));
    c->heading_rad=c->heading_target_rad=0;
    ui.open=false;
    app_save_persisted_view_state(app);
}
bool terrain_ui_event(AppState *app,const SDL_Event *event) {
    if(!ui.open)return false;
    sync(app);uint64_t action;uint32_t modifiers;
    (void)carta_modal_controls_event(&ui.controls,event,&action,&modifiers);
    (void)modifiers;
    if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE && !event->key.repeat) {
        if(*ui.pending)ui.pending[0]=0;else ui.open=false;
        kit_ui_surface_set_scope(&ui.controls,++ui.scope);return true;
    }
    SDL_FRect p=panel(app);const TerrainCatalog *catalog=terrain_area_catalog();
    int rows=(int)fmax(1,floor((p.h-200)/62));
    if(event->type==SDL_MOUSEWHEEL && !*ui.pending) {
        ui.offset=fmin(fmax(0,(int)catalog->count-rows),fmax(0,ui.offset-event->wheel.y));sync(app);return true;
    }
    if(action==UINT64_MAX)return true;
    if(action==0){ui.open=false;ui.pending[0]=0;}
    else if(*ui.pending) {
        if(action==7)ui.pending[0]=0;
        else if(action==6) {
            size_t index=0;while(index<catalog->count && strcmp(catalog->areas[index].id,ui.pending))index++;
            bool ok=terrain_remove_area(app,index,ui.status,sizeof(ui.status));
            if(ok)snprintf(ui.status,sizeof(ui.status),"Background removed · hike records and routes preserved");
            ui.pending[0]=0;ui.offset=0;
        }
    } else if(action==1)ui.offset=fmax(0,ui.offset-rows);
    else if(action==2)ui.offset=fmin(fmax(0,(int)catalog->count-rows),ui.offset+rows);
    else if(action==3) {
        bool ok=terrain_reload(app);snprintf(ui.status,sizeof(ui.status),"%s",ok?"Terrain areas reloaded":"Catalog refused · previous valid areas preserved");ui.offset=0;
    } else if(action>=100) {
        size_t index=ui.offset+(action-100)/2;
        if(index<catalog->count) {
            const TerrainArea *area=&catalog->areas[index];
            if((action-100)%2==0)show(app,area);
            else if(area->removable){snprintf(ui.pending,sizeof(ui.pending),"%s",area->id);ui.status[0]=0;}
        }
    }
    kit_ui_surface_set_scope(&ui.controls,++ui.scope);app->runtime_redraw_requested=true;
    return true;
}
void terrain_ui_draw(AppState *app) {
    if(!ui.open)return;
    sync(app);SDL_FRect p=panel(app);
    MapForgeThemePalette palette=app_ui_theme_palette();
    renderer_set_draw_color(&app->renderer,palette.route_panel_fill.r,palette.route_panel_fill.g,palette.route_panel_fill.b,255);
    renderer_fill_rect(&app->renderer,&p);
    line(app,p,p.y+16,"Downloaded terrain",false);
    button(app,control(p,0,0),"Close",false,0,0);
    const TerrainCatalog *catalog=terrain_area_catalog();
    char text[256];snprintf(text,sizeof(text),"Additional backgrounds: %.1f / 128 MiB · 32 MiB per area",catalog->additional_bytes/1048576.0);
    line(app,p,p.y+50,text,true);
    line(app,p,p.y+74,"USGS 3DEP + GNIS · original hike data is kept separately",true);
    int rows=(int)fmax(1,floor((p.h-200)/62));
    ui.offset=fmin(ui.offset,fmax(0,(int)catalog->count-rows));
    for(int row=0;row<rows && ui.offset+row<(int)catalog->count;row++) {
        const TerrainArea *area=&catalog->areas[ui.offset+row];
        float y=p.y+108+row*62;
        ui_draw_text_clipped(&app->renderer,p.x+14,y+2,area->name,palette.text_primary,.74f,p.w-200);
        snprintf(text,sizeof(text),"%.1f MiB · %.0f m terrain · %s",area->bytes/1048576.0,area->resolution_m,area->removable?"Removable background":"Protected baseline");
        ui_draw_text_clipped(&app->renderer,p.x+14,y+25,text,palette.text_muted,.62f,p.w-28);
        button(app,control(p,4,row),"Show",false,4,row);
        if(area->removable)button(app,control(p,5,row),"Remove",!strcmp(ui.pending,area->id),5,row);
    }
    if(*ui.pending) {
        snprintf(text,sizeof(text),"Remove background %s? Routes and statuses stay saved.",ui.pending);
        line(app,p,p.y+p.h-138,text,false);
        button(app,control(p,6,0),"Confirm remove",true,6,0);
        button(app,control(p,7,0),"Cancel",false,7,0);
    } else line(app,p,p.y+p.h-100,*ui.status?ui.status:"Select a hike to see its background coverage in the route card.",true);
    button(app,control(p,1,0),"Previous",false,1,0);button(app,control(p,2,0),"Next",false,2,0);
    button(app,control(p,3,0),"Reload",false,3,0);
}
