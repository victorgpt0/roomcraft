#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

#include <vector>

namespace raster {

Point evaluateBezierCubic(Point p0, Point p1, Point p2, Point p3, float t);
void drawBezierCurve(Canvas& canvas, Point p0, Point p1, Point p2, Point p3, Color color, int segments);

Point evaluateCatmullRomSpline(const std::vector<Point>& controlPoints, float t);

} // namespace raster
