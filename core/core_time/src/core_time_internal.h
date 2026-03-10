#ifndef CORE_TIME_INTERNAL_H
#define CORE_TIME_INTERNAL_H

#include <stdbool.h>

#include "core_time.h"

bool core_time_platform_now_ns(CoreTimeNs *out_now_ns);

#endif
