#pragma once

#include <stdbool.h>
#include "ShapeLib/shape_core.h"

bool ShapeDocument_SaveToJsonFile(const ShapeDocument* doc,
                                  const char* path);

bool ShapeDocument_LoadFromJsonFile(const char* path,
                                    ShapeDocument* outDoc);
