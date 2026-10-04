#pragma once

#include "raster/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace raster {

// Owns a width x height RGB pixel buffer (3 bytes per pixel, row-major,
// origin at top-left) and can export it as a binary PPM (P6) image.
class Canvas {
public:
    Canvas(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    void clear(uint8_t r, uint8_t g, uint8_t b);

    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void setPixel(int x, int y, Color color);

    // Returns {0, 0, 0} for out-of-bounds coordinates.
    Color getPixel(int x, int y) const;

    // Writes a binary PPM (P6) file. Returns false if the file could not be
    // opened for writing.
    bool savePPM(const std::string& path) const;

private:
    int width_;
    int height_;
    std::vector<uint8_t> pixels_; // size = width * height * 3

    size_t index(int x, int y) const;
    bool inBounds(int x, int y) const;
};

} // namespace raster
