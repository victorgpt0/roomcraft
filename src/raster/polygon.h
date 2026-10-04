#pragma once

#include "raster/types.h"

#include <vector>

namespace raster {

struct Polygon {
    std::vector<Point> vertices;
};

void translate(Polygon& polygon, float dx, float dy);
void rotate(Polygon& polygon, float angleDegrees, Point pivot);
void scale(Polygon& polygon, float sx, float sy, Point pivot);

float computeArea(const Polygon& polygon);
Point computeCentroid(const Polygon& polygon);

} // namespace raster
