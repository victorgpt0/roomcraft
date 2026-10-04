#include "raster/canvas.h"
#include "raster/curves.h"

#include <string>
#include <vector>

int main() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::Point p0{20.0f, 220.0f};
    raster::Point p1{80.0f, 20.0f};
    raster::Point p2{180.0f, 20.0f};
    raster::Point p3{240.0f, 220.0f};

    raster::drawBezierCurve(canvas, p0, p1, p2, p3, raster::Color{255, 128, 0}, 64);

    raster::Point evaluated = raster::evaluateBezierCubic(p0, p1, p2, p3, 0.5f);
    (void)evaluated;

    std::vector<raster::Point> spline = {p0, p1, p2, p3};
    raster::Point splinePoint = raster::evaluateCatmullRomSpline(spline, 0.5f);
    (void)splinePoint;

    canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_curves.ppm");
    return 0;
}
