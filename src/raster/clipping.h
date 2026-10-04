#pragma once

#include "raster/polygon.h"

namespace raster {

// Clips the subject polygon against clipWindow (assumed convex) using the
// Sutherland-Hodgman algorithm.
Polygon clipPolygonSutherlandHodgman(const Polygon& subject, const Polygon& clipWindow);

} // namespace raster
