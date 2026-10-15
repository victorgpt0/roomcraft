#pragma once

#include "raster/polygon.h"

// Polygon clipping.
//
// RoomCraft use: clipping furniture footprints and the editor grid to the
// room outline, and checking whether a piece of furniture fits inside the
// room (if clipping changes its area, part of it is outside).

namespace raster {

// Sutherland-Hodgman polygon clipping. Returns the part of `subject` that is
// inside `clipWindow`.
//
// - clipWindow must be CONVEX. Its vertices may be listed in either
//   direction (clockwise or counter-clockwise). Points on its edges count
//   as inside.
// - subject may be any simple polygon, convex or concave.
// - Returns an empty polygon if nothing is inside, or if clipWindow has
//   fewer than 3 vertices or zero area.
//
// Known limitation: if a CONCAVE subject is cut into several separate
// pieces, the result is still ONE polygon, with zero-width "bridge" edges
// running along the clip window boundary between the pieces. Its area and
// its scanline fill are still correct, but its outline shows the bridges.
Polygon clipPolygonSutherlandHodgman(const Polygon& subject, const Polygon& clipWindow);

} // namespace raster
