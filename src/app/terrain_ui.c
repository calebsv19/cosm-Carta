#include "app/terrain_ui.h"
#include "app/terrain.h"
#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_ui_internal.h"
#include "app/app_persist_state.h"
#include "map/mercator.h"
#include "ui/font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Keeps only presentation state and a stable pending removal ID, never a history snapshot.
static struct { bool open; int offset; char pending[64], status[256]; } ui;
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
// Tests only visible positive-size native controls.
static bool hit(SDL_FRect box, int x, int y) {
    return box.w>0 && box.h>0 && x>=box.x && y>=box.y && x<box.x+box.w && y<box.y+box.h;
}
// Uses the app's existing theme and renderer expression for a concise action cell.
static void button(AppState *app, SDL_FRect box, const char *text, bool active) {
    MapForgeThemePalette p = app_ui_theme_palette();
    SDL_Color color = active ? p.button_active_primary : p.button_fill;
    renderer_set_draw_color(&app->renderer,color.r,color.g,color.b,color.a);
    renderer_fill_rect(&app->renderer,&box);
    bool light=(299u*color.r+587u*color.g+114u*color.b)/1000u>160;
    ui_draw_text_clipped(&app->renderer,box.x+6,box.y+5,text,light?(SDL_Color){24,43,47,255}:p.text_primary,.68f,box.w-12);
}
// Draws a single clipped panel line using the current theme and font path.
static void line(AppState *app, SDL_FRect p, float y, const char *text, bool muted) {
    MapForgeThemePalette palette=app_ui_theme_palette();
    ui_draw_text_clipped(&app->renderer,p.x+14,y,text,muted?palette.text_muted:palette.text_primary,.70f,p.w-28);
}
SDL_FRect terrain_ui_control_rect(const AppState *app,TerrainUIControl type,int row) {
    return control(panel(app),type,row);
}
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
    if(event->type==SDL_KEYDOWN && event->key.keysym.sym==SDLK_ESCAPE) {
        if(*ui.pending)ui.pending[0]=0;else ui.open=false;
        return true;
    }
    SDL_FRect p=panel(app);
    const TerrainCatalog *catalog=terrain_area_catalog();
    int rows=(int)fmax(1,floor((p.h-200)/62));
    if(event->type==SDL_MOUSEWHEEL && !*ui.pending) {
        ui.offset=fmin(fmax(0,(int)catalog->count-rows),fmax(0,ui.offset-event->wheel.y));
        return true;
    }
    if(event->type!=SDL_MOUSEBUTTONDOWN || event->button.button!=SDL_BUTTON_LEFT)return true;
    int x=event->button.x,y=event->button.y;
    if(hit(control(p,0,0),x,y)){ui.open=false;ui.pending[0]=0;return true;}
    if(*ui.pending) {
        if(hit(control(p,7,0),x,y))ui.pending[0]=0;
        else if(hit(control(p,6,0),x,y)) {
            size_t index=0;while(index<catalog->count && strcmp(catalog->areas[index].id,ui.pending))index++;
            bool ok=terrain_remove_area(app,index,ui.status,sizeof(ui.status));
            if(ok)snprintf(ui.status,sizeof(ui.status),"Background removed · hike records and routes preserved");
            ui.pending[0]=0;ui.offset=0;
        }
        return true;
    }
    if(hit(control(p,1,0),x,y))ui.offset=fmax(0,ui.offset-rows);
    else if(hit(control(p,2,0),x,y))ui.offset=fmin(fmax(0,(int)catalog->count-rows),ui.offset+rows);
    else if(hit(control(p,3,0),x,y)) {
        bool ok=terrain_reload(app);
        snprintf(ui.status,sizeof(ui.status),"%s",ok?"Terrain areas reloaded":"Catalog refused · previous valid areas preserved");
        ui.offset=0;
    } else for(int row=0;row<rows && ui.offset+row<(int)catalog->count;row++) {
        const TerrainArea *area=&catalog->areas[ui.offset+row];
        if(hit(control(p,4,row),x,y)){show(app,area);break;}
        if(area->removable && hit(control(p,5,row),x,y)) {
            snprintf(ui.pending,sizeof(ui.pending),"%s",area->id);ui.status[0]=0;break;
        }
    }
    return true;
}
void terrain_ui_draw(AppState *app) {
    if(!ui.open)return;
    SDL_FRect p=panel(app);
    MapForgeThemePalette palette=app_ui_theme_palette();
    renderer_set_draw_color(&app->renderer,palette.route_panel_fill.r,palette.route_panel_fill.g,palette.route_panel_fill.b,255);
    renderer_fill_rect(&app->renderer,&p);
    line(app,p,p.y+16,"Downloaded terrain",false);
    button(app,control(p,0,0),"Close",false);
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
        button(app,control(p,4,row),"Show",false);
        if(area->removable)button(app,control(p,5,row),"Remove",!strcmp(ui.pending,area->id));
    }
    if(*ui.pending) {
        snprintf(text,sizeof(text),"Remove background %s? Routes and statuses stay saved.",ui.pending);
        line(app,p,p.y+p.h-138,text,false);
        button(app,control(p,6,0),"Confirm remove",true);
        button(app,control(p,7,0),"Cancel",false);
    } else line(app,p,p.y+p.h-100,*ui.status?ui.status:"Select a hike to see its background coverage in the route card.",true);
    button(app,control(p,1,0),"Previous",false);button(app,control(p,2,0),"Next",false);
    button(app,control(p,3,0),"Reload",false);
}
