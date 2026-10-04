#include "raster/canvas.h"
#include "raster/line.h"

#include <string>

int main() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::Color red{255, 0, 0};
    raster::drawLineDDA(canvas, 20, 20, 230, 100, red);
    raster::drawLineBresenham(canvas, 20, 150, 230, 230, red);

    int x0 = 0, y0 = 0, x1 = 255, y1 = 255;
    raster::Rect clipWindow{50.0f, 50.0f, 200.0f, 200.0f};
    raster::clipLineCohenSutherland(x0, y0, x1, y1, clipWindow);

    canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_line.ppm");
    return 0;
}
