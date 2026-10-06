#include "app/history_layout.h"
#include <math.h>
// Places only visible controls; zero-size rectangles cannot receive pointer input.
HistoryLayout history_layout(SDL_FRect p, bool tools, bool inspect) {
    HistoryLayout l = {0};
    float w = fmaxf(1, (p.w - 32) / 3);
    int primary[] = {0, 1, 2, 10, 3, 12};
    for (int i = 0; i < 6; i++)
        l.actions[primary[i]] = (SDL_FRect){p.x + 12 + i % 3 * (w + 4), p.y + 131 + i / 3 * 32, w, 27};
    l.actions[14] = (SDL_FRect){p.x + 12, p.y + 37, p.w - 24, 27};
    l.actions[16] = (SDL_FRect){p.x + 12, p.y + 99, p.w - 24, 25};
    l.legend = (SDL_FRect){p.x + 12, p.y + 70, p.w - 24, 23};
    if (tools) {
        int extra[] = {4, 6, 5, 9, 11, 15,27,28,26};
        for (int i = 0; i < 9; i++)
            l.actions[extra[i]] = (SDL_FRect){p.x + 12 + i % 3 * (w + 4), p.y + 197 + i / 3 * 30, w, 25};
    }
    float top = tools ? 294 : 200;
    float detail_height = inspect && p.h > 550 ? 320 : p.h > 480 ? 204 : 156;
    l.detail = (SDL_FRect){p.x + 12, p.y + p.h - detail_height - 28, p.w - 24, detail_height};
    l.list = (SDL_FRect){p.x + 12, p.y + top, p.w - 24, fmaxf(0, l.detail.y - p.y - top - 22)};
    l.rows = (int)(l.list.h / 38);
    l.actions[13] = (SDL_FRect){l.detail.x + l.detail.w - 74, l.detail.y + 8, 66, 24};
    if (inspect && p.h > 550) {
        l.actions[7] = (SDL_FRect){l.detail.x + 8, l.detail.y + 142, 55, 24};
        l.actions[8] = (SDL_FRect){l.detail.x + 67, l.detail.y + 142, 55, 24};
    }
    for(int i=0;i<4;i++) l.actions[20+i]=(SDL_FRect){l.detail.x+8+i*(l.detail.w-16)/4,l.detail.y+l.detail.h-56,(l.detail.w-20)/4,24};
    l.actions[24]=(SDL_FRect){l.detail.x+l.detail.w-63,l.detail.y+l.detail.h-28,55,22};
    return l;
}
