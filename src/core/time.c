#include "core/time.h"

#include <SDL.h>

// Returns current time in seconds from a monotonic clock.
double time_now_seconds(void) {
    static double frequency = 0.0;
    if (frequency == 0.0) {
        frequency = (double)SDL_GetPerformanceFrequency();
    }

    return (double)SDL_GetPerformanceCounter() / frequency;
}
