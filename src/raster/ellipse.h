#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

// Axis-aligned ellipses (midpoint ellipse algorithm).
//
// Screen coordinates (origin top-left, y down - see types.h). radiusX is the
// half-width, radiusY the half-height.
//
// RoomCraft use: oval rugs, pillows/cushions and oval tables.

namespace raster {

// Ellipse outline. A negative radius draws nothing. If one radius is 0 the
// ellipse is flat and is drawn as a straight line.
void drawEllipseMidpoint(Canvas& canvas, int centerX, int centerY, int radiusX, int radiusY, Color color);

// Filled ellipse: exactly the pixels on and inside the drawEllipseMidpoint()
// outline.
void fillEllipseMidpoint(Canvas& canvas, int centerX, int centerY, int radiusX, int radiusY, Color color);

} // namespace raster
