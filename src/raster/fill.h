#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"

// Polygon filling.
//
// RoomCraft use: solid furniture footprints and floor areas (scanline fill),
// and "paint bucket" colouring of a closed region drawn on the floor plan
// (boundary fill).

namespace raster {

// Scanline polygon fill with the even-odd rule.
//
// Which pixels are filled: pixel (x, y) is filled when its CENTRE (x, y) is
// inside the polygon, using a half-open rule: the left and top edges are
// inside, the right and bottom edges are outside. So a rectangle from
// (10, 10) to (40, 30) fills exactly 30 x 20 pixels (x 10..39, y 10..29),
// and two polygons that share an edge neither overlap nor leave a gap.
//
// Works for convex, concave and self-intersecting polygons (for the last,
// regions covered an even number of times stay empty).
void fillPolygonScanline(Canvas& canvas, const Polygon& polygon, Color color);

// Boundary fill (4-connected), starting at the seed pixel (x, y).
// Paints every pixel reachable from the seed by up/down/left/right steps
// without crossing a pixel of boundaryColor. Uses an explicit stack instead
// of recursion, so large areas cannot overflow the call stack.
//
// The region must be closed: if the boundary has a gap, the fill leaks out
// through it (at worst it fills the whole canvas).
// Does nothing if the seed is outside the canvas or already has
// boundaryColor or fillColor. Returns the number of pixels painted.
int boundaryFill(Canvas& canvas, int x, int y, Color fillColor, Color boundaryColor);

} // namespace raster
