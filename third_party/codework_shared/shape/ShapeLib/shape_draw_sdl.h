#pragma once

#include <SDL2/SDL.h>
#include "ShapeLib/shape_core.h"

bool Shape_DrawToSDL(SDL_Renderer* renderer,
                     const Shape* shape,
                     int screenW, int screenH,
                     float maxError);
