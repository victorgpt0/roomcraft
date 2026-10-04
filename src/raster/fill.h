#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"

namespace raster {

void fillPolygonScanline(Canvas& canvas, const Polygon& polygon, Color color);

} // namespace raster
