#pragma once

#include <cstdint>

namespace raster {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct Rect {
    float xmin = 0.0f;
    float ymin = 0.0f;
    float xmax = 0.0f;
    float ymax = 0.0f;
};

} // namespace raster
