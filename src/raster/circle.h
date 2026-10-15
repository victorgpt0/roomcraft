#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

// Circles and circular arcs (midpoint circle algorithm).
//
// Screen coordinates (origin top-left, y down - see types.h). Angles are in
// degrees, measured from the +x axis (pointing right). Because y points
// down, angles increase CLOCKWISE on screen:
//     0 = right, 90 = down, 180 = left, 270 = up.
// This matches rotate() in polygon.h.
//
// RoomCraft use: round tables, chairs and stools (filled circles with an
// outline) and door swings (a quarter-circle arc).

namespace raster {

// Circle outline. radius < 0 draws nothing; radius 0 draws one pixel.
void drawCircleMidpoint(Canvas& canvas, int centerX, int centerY, int radius, Color color);

// Filled disc: exactly the pixels on and inside the drawCircleMidpoint()
// outline. radius < 0 draws nothing.
void fillCircleMidpoint(Canvas& canvas, int centerX, int centerY, int radius, Color color);

// The part of the circle outline from startDegrees to endDegrees, going
// clockwise on screen (increasing angle). Wraps past 360, so 270 -> 90
// draws the right half. A sweep of 360 degrees or more draws the full circle.
void drawCircleArc(Canvas& canvas, int centerX, int centerY, int radius,
                   float startDegrees, float endDegrees, Color color);

} // namespace raster
