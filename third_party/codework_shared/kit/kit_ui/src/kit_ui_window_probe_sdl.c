#include "kit_ui_window_probe_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void kit_ui_window_probe_init_sdl(KitUiWindowProbe *p,const char *program) {
    memset(p,0,sizeof(*p));p->program=program;
    p->directory=getenv("CODEWORK_WINDOW_LIFECYCLE_PROOF");
    if(p->directory && !*p->directory)p->directory=0;
    p->started_ms=p->phase_ms=SDL_GetTicks64();
}
int kit_ui_window_probe_tick_sdl(KitUiWindowProbe *p,SDL_Window *w,uint64_t frames,
    KitUiWindowProbeCapture capture,void *user) {
    static const char *stages[]={"initial","resize","fullscreen","windowed","hidden","shown","minimized","restored"};
    if(!p || !p->directory)return 0;
    if(p->status)return p->status;
    uint64_t now=SDL_GetTicks64();
    if(now-p->started_ms>45000) {
        fprintf(stderr,"WINDOW_LIFECYCLE program=%s stage=%s status=timeout\n",p->program,stages[p->phase]);
        return p->status=-1;
    }
    KitUiWindowState s={0};uint32_t bits;
    if(kit_ui_window_refresh_sdl(&s,w,&bits).code!=CORE_OK)return p->status=-1;
    if(!p->width){p->width=s.logical_width;p->height=s.logical_height;}
    int suspended=p->phase==4 || p->phase==6;
    /* Cocoa minimize is asynchronous: rendering during its animation is valid.
     * Start the suspension interval when the actual native flags settle. */
    if(suspended && !s.presentable && !p->captured) {
        p->captured=1;p->phase_frames=frames;p->phase_ms=now;
    }
    if(now-p->phase_ms<700)return 0;
    if(suspended) {
        if(s.presentable || (p->phase==6 && !(s.flags&SDL_WINDOW_MINIMIZED)))return 0;
        if(frames!=p->phase_frames) {
            fprintf(stderr,"WINDOW_LIFECYCLE program=%s stage=%s status=rendered-while-suspended\n",p->program,stages[p->phase]);
            return p->status=-1;
        }
    } else {
        if(!s.presentable || frames<p->phase_frames+3)return 0;
        if((p->phase==2)!=((s.flags&SDL_WINDOW_FULLSCREEN)!=0))return 0;
        if(!p->captured) {
            char path[1024];int n=snprintf(path,sizeof(path),"%s/%s-%s.bmp",p->directory,p->program,stages[p->phase]);
            if(n<0 || n>=(int)sizeof(path) || !capture || !capture(user,path))return p->status=-1;
            p->captured=1;p->capture_frame=frames;return 0;
        }
        if(frames<=p->capture_frame)return 0;
    }
    int rx,ry;
    if(!kit_ui_window_map_point_sdl(&s,s.drawable_width,s.drawable_height,s.logical_width/2,s.logical_height/2,&rx,&ry))return p->status=-1;
    fprintf(stdout,"WINDOW_LIFECYCLE program=%s stage=%s status=pass logical=%dx%d drawable=%dx%d center=%d,%d frames=%llu flags=0x%x\n",p->program,stages[p->phase],s.logical_width,s.logical_height,s.drawable_width,s.drawable_height,rx,ry,(unsigned long long)frames,s.flags);fflush(stdout);
    ++p->phase;p->captured=0;p->phase_ms=now;p->phase_frames=frames;
    switch(p->phase) {
        case 1:SDL_SetWindowSize(w,getenv("CODEWORK_WINDOW_PROOF_LARGE")?2500:960,720);break;
        case 2:if(SDL_SetWindowFullscreen(w,SDL_WINDOW_FULLSCREEN_DESKTOP)!=0)return p->status=-1;break;
        case 3:if(SDL_SetWindowFullscreen(w,0)!=0)return p->status=-1;break;
        case 4:SDL_HideWindow(w);break;
        case 5:SDL_ShowWindow(w);SDL_RaiseWindow(w);break;
        case 6:SDL_MinimizeWindow(w);break;
        case 7:SDL_RestoreWindow(w);SDL_SetWindowSize(w,p->width,p->height);SDL_RaiseWindow(w);break;
        default:fprintf(stdout,"WINDOW_LIFECYCLE program=%s status=complete frames=%llu\n",p->program,(unsigned long long)frames);fflush(stdout);return p->status=1;
    }
    return 0;
}
