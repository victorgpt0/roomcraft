#include "raster/curves.h"

namespace raster {

Point evaluateBezierCubic(Point, Point, Point, Point, float) {
    // TODO: implement
    return Point{};
}

void drawBezierCurve(Canvas&, Point, Point, Point, Point, Color, int) {
    // TODO: implement
}

Point evaluateCatmullRomSpline(const std::vector<Point>&, float) {
    // TODO: implement
    return Point{};
}

} // namespace raster
