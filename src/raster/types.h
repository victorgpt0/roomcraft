#pragma once

#include <cmath>
#include <cstdint>

// Coordinate convention used by every module in raster/
// -----------------------------------------------------
// We use SCREEN coordinates, the same as the pixel buffer in Canvas:
//
//   (0,0) ----------> +x
//     |
//     |
//     v
//    +y
//
// - The origin is the top-left corner of the canvas.
// - x grows to the right, y grows DOWNWARD.
// - Integer coordinates are pixel CENTRES: pixel (3, 5) is the pixel whose
//   centre is at x = 3.0, y = 5.0. Float coordinates are rounded to the
//   nearest pixel with roundToPixel() below.
//
// Because y points down, "above" on screen means a SMALLER y. A positive
// rotation angle therefore turns clockwise on screen (see polygon.h).

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

// Axis-aligned rectangle. On screen, ymin is the TOP edge and ymax the
// BOTTOM edge. Code that takes a Rect assumes xmin <= xmax and ymin <= ymax.
struct Rect {
    float xmin = 0.0f;
    float ymin = 0.0f;
    float xmax = 0.0f;
    float ymax = 0.0f;
};

constexpr double kPi = 3.14159265358979323846;

// Rounds a coordinate to the nearest pixel centre, with halves rounding up
// (2.5 -> 3, -2.5 -> -2). Using floor(v + 0.5) instead of a cast keeps the
// rounding the same for negative coordinates (a cast would truncate toward 0).
inline int roundToPixel(double v) {
    return static_cast<int>(std::floor(v + 0.5));
}

} // namespace raster
