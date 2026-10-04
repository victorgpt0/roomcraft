#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/ellipse.h"

#include <string>

int main() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::drawCircleMidpoint(canvas, 80, 80, 50, raster::Color{0, 200, 0});
    raster::drawEllipseMidpoint(canvas, 180, 180, 60, 30, raster::Color{200, 0, 200});

    canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_circle_ellipse.ppm");
    return 0;
}
