#include "raster/canvas.h"
#include "raster/polygon.h"

#include <string>

int main() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::Polygon triangle;
    triangle.vertices = {{50.0f, 50.0f}, {200.0f, 50.0f}, {125.0f, 200.0f}};

    raster::Point pivot{125.0f, 100.0f};
    raster::translate(triangle, 10.0f, 0.0f);
    raster::rotate(triangle, 15.0f, pivot);
    raster::scale(triangle, 1.1f, 1.1f, pivot);

    float area = raster::computeArea(triangle);
    raster::Point centroid = raster::computeCentroid(triangle);
    (void)area;
    (void)centroid;

    canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_polygon.ppm");
    return 0;
}
