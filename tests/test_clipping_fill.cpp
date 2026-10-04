#include "raster/canvas.h"
#include "raster/clipping.h"
#include "raster/fill.h"

#include <string>

int main() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::Polygon subject;
    subject.vertices = {{30.0f, 30.0f}, {220.0f, 30.0f}, {220.0f, 220.0f}, {30.0f, 220.0f}};

    raster::Polygon clipWindow;
    clipWindow.vertices = {{64.0f, 64.0f}, {192.0f, 64.0f}, {192.0f, 192.0f}, {64.0f, 192.0f}};

    raster::Polygon clipped = raster::clipPolygonSutherlandHodgman(subject, clipWindow);
    raster::fillPolygonScanline(canvas, clipped, raster::Color{0, 128, 255});

    canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_clipping_fill.ppm");
    return 0;
}
