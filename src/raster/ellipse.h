#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

namespace raster {

void drawEllipseMidpoint(Canvas& canvas, int centerX, int centerY, int radiusX, int radiusY, Color color);

} // namespace raster
