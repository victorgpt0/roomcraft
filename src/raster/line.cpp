#include "raster/line.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace raster {

namespace {

// ---------------------------------------------------------------------------
// Bresenham walk, shared by the solid, thick and dashed line functions so
// they all use exactly the same centre pixels.
//
// Calls visit(x, y, step) for every pixel from (x0, y0) to (x1, y1)
// inclusive. step counts 0, 1, 2, ... along the major axis.
//
// How it works (explained for a "shallow" line, |dy| <= |dx|, where x is the
// MAJOR axis and y the MINOR axis; a steep line swaps the roles of x and y):
//
// - We move exactly one pixel along the major axis every step. The only
//   question is whether the minor coordinate also moves by one.
// - The ideal line moves minor/major pixels along the minor axis per step.
//   We step the minor axis when the ideal line has passed the MIDPOINT
//   between the two candidate pixels (current minor, current minor + 1).
// - The decision variable is
//       d = 2 * major * (idealMinorAtNextStep - (currentMinor + 0.5))
//   Multiplying by 2 * major removes every fraction, so d is an integer:
//       start:                  d = 2 * minor - major
//       every step:             d += 2 * minor  (ideal line moved minor/major)
//       when we step minor too: d -= 2 * major  (the midpoint moved by 1)
//   d > 0 means the ideal line is past the midpoint, so we step the minor
//   axis. That keeps every pixel within half a pixel of the ideal line.
// - sx and sy (+1 or -1) give the direction, which covers all eight octants
//   with the same code.
// ---------------------------------------------------------------------------
template <typename Visit>
void walkBresenham(int x0, int y0, int x1, int y1, Visit visit) {
    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);
    const int sx = (x1 >= x0) ? 1 : -1;
    const int sy = (y1 >= y0) ? 1 : -1;

    const bool steep = dy > dx; // |slope| > 1: y is the major axis
    const int major = steep ? dy : dx;
    const int minor = steep ? dx : dy;

    int decision = 2 * minor - major;
    int x = x0;
    int y = y0;
    for (int step = 0; step <= major; ++step) {
        visit(x, y, step);

        if (decision > 0) {
            // The ideal line has passed the midpoint: step the minor axis too.
            if (steep) {
                x += sx;
            } else {
                y += sy;
            }
            decision -= 2 * major;
        }
        decision += 2 * minor;

        if (steep) {
            y += sy;
        } else {
            x += sx;
        }
    }
}

// Cohen-Sutherland region codes. Each bit says the point is outside one edge.
// Screen coordinates: TOP means y < ymin (above the window on screen).
//
//          1001 | 1000 | 1010         (TOP + LEFT) | TOP | (TOP + RIGHT)
//          -----+------+-----
//          0001 | 0000 | 0010              LEFT    | in  |  RIGHT
//          -----+------+-----
//          0101 | 0100 | 0110
constexpr int kInside = 0;
constexpr int kLeft = 1;
constexpr int kRight = 2;
constexpr int kBottom = 4;
constexpr int kTop = 8;

int computeOutcode(float x, float y, const Rect& window) {
    int code = kInside;
    if (x < window.xmin) {
        code |= kLeft;
    } else if (x > window.xmax) {
        code |= kRight;
    }
    if (y < window.ymin) {
        code |= kTop;
    } else if (y > window.ymax) {
        code |= kBottom;
    }
    return code;
}

} // namespace

// DDA: the number of steps is the longer of |dx| and |dy|, so one coordinate
// moves exactly 1 pixel per step and the other moves by the slope (at most 1).
// That guarantees no gaps. The float position is rounded to the nearest pixel
// each step. Simple and easy to follow, but it needs floating-point
// arithmetic and rounding, which Bresenham avoids.
void drawLineDDA(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps == 0) {
        canvas.setPixel(x0, y0, color); // start == end: a single pixel
        return;
    }

    const double xIncrement = static_cast<double>(dx) / steps;
    const double yIncrement = static_cast<double>(dy) / steps;

    double x = x0;
    double y = y0;
    for (int i = 0; i <= steps; ++i) {
        canvas.setPixel(roundToPixel(x), roundToPixel(y), color);
        x += xIncrement;
        y += yIncrement;
    }
}

void drawLineBresenham(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
    walkBresenham(x0, y0, x1, y1, [&](int x, int y, int) { canvas.setPixel(x, y, color); });
}

// Thick line: walk the 1-pixel Bresenham centre line and, at every centre
// pixel, draw a short span across the MINOR axis (vertical span for a
// shallow line, horizontal span for a steep one).
//
// A span of N pixels along the minor axis is only N * cos(angle) thick when
// measured perpendicular to the line. So to get the requested perpendicular
// thickness we make the span longer:
//     span = thickness / cos(angle) = thickness * length / major
// where length is the line's true length and major = max(|dx|, |dy|).
void drawThickLine(Canvas& canvas, int x0, int y0, int x1, int y1, int thickness, Color color) {
    if (thickness <= 1) {
        drawLineBresenham(canvas, x0, y0, x1, y1, color);
        return;
    }

    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const bool steep = std::abs(dy) > std::abs(dx);
    const int major = std::max(std::abs(dx), std::abs(dy));

    int span = thickness;
    if (major > 0) {
        const double length = std::sqrt(static_cast<double>(dx) * dx + static_cast<double>(dy) * dy);
        span = roundToPixel(thickness * length / major);
    }

    // Centre the span on the line. For an even span the extra pixel goes to
    // the positive side (below / right).
    const int first = -(span - 1) / 2;
    const int last = first + span - 1;

    walkBresenham(x0, y0, x1, y1, [&](int x, int y, int) {
        for (int k = first; k <= last; ++k) {
            if (steep) {
                canvas.setPixel(x + k, y, color);
            } else {
                canvas.setPixel(x, y + k, color);
            }
        }
    });
}

// Dashed line: the same Bresenham pixels, but each pixel is drawn only if
// its step number falls inside the "dash" part of the repeating pattern.
void drawDashedLine(Canvas& canvas, int x0, int y0, int x1, int y1,
                    int dashLength, int gapLength, Color color) {
    if (dashLength <= 0) {
        return;
    }
    const int period = dashLength + std::max(gapLength, 0);

    walkBresenham(x0, y0, x1, y1, [&](int x, int y, int step) {
        if (step % period < dashLength) {
            canvas.setPixel(x, y, color);
        }
    });
}

void drawPolyline(Canvas& canvas, const std::vector<Point>& points, Color color) {
    if (points.empty()) {
        return;
    }
    if (points.size() == 1) {
        canvas.setPixel(roundToPixel(points[0].x), roundToPixel(points[0].y), color);
        return;
    }
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        drawLineBresenham(canvas,
                          roundToPixel(points[i].x), roundToPixel(points[i].y),
                          roundToPixel(points[i + 1].x), roundToPixel(points[i + 1].y),
                          color);
    }
}

void drawPolygonOutline(Canvas& canvas, const Polygon& polygon, Color color) {
    const std::vector<Point>& v = polygon.vertices;
    drawPolyline(canvas, v, color);

    // Closing edge. With 2 vertices it would just redraw the same segment.
    if (v.size() >= 3) {
        const Point& last = v.back();
        const Point& first = v.front();
        drawLineBresenham(canvas,
                          roundToPixel(last.x), roundToPixel(last.y),
                          roundToPixel(first.x), roundToPixel(first.y),
                          color);
    }
}

// Cohen-Sutherland:
// 1. Give each endpoint a 4-bit region code (see the diagram above).
// 2. Both codes 0          -> both endpoints inside: accept.
//    (code0 & code1) != 0  -> both outside the SAME edge, so the segment
//                             cannot cross the window: reject.
// 3. Otherwise pick an endpoint that is outside, move it to where the segment
//    crosses one of the edges it is outside of, recompute its code and repeat.
//
// The intersection uses the line equation. For example, crossing the top
// edge y = ymin:  x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0).
// No division by zero is possible: if the point is above the window and the
// other is not, then y1 != y0 (if both were above, step 2 rejects first).
bool clipLineCohenSutherland(float& x0, float& y0, float& x1, float& y1, Rect clipWindow) {
    int code0 = computeOutcode(x0, y0, clipWindow);
    int code1 = computeOutcode(x1, y1, clipWindow);

    // In exact arithmetic each endpoint is moved at most twice (once onto a
    // vertical edge, once onto a horizontal one), so 4 moves always finish.
    // The limit is a guard against float rounding near a window corner.
    for (int moves = 0; moves <= 8; ++moves) {
        if ((code0 | code1) == kInside) {
            return true;
        }
        if ((code0 & code1) != 0) {
            return false;
        }

        const int outside = (code0 != kInside) ? code0 : code1;
        float x = 0.0f;
        float y = 0.0f;
        if (outside & kTop) {
            x = x0 + (x1 - x0) * (clipWindow.ymin - y0) / (y1 - y0);
            y = clipWindow.ymin;
        } else if (outside & kBottom) {
            x = x0 + (x1 - x0) * (clipWindow.ymax - y0) / (y1 - y0);
            y = clipWindow.ymax;
        } else if (outside & kRight) {
            y = y0 + (y1 - y0) * (clipWindow.xmax - x0) / (x1 - x0);
            x = clipWindow.xmax;
        } else { // kLeft
            y = y0 + (y1 - y0) * (clipWindow.xmin - x0) / (x1 - x0);
            x = clipWindow.xmin;
        }

        if (outside == code0) {
            x0 = x;
            y0 = y;
            code0 = computeOutcode(x0, y0, clipWindow);
        } else {
            x1 = x;
            y1 = y;
            code1 = computeOutcode(x1, y1, clipWindow);
        }
    }
    return false;
}

void drawLineClipped(Canvas& canvas, Point a, Point b, Rect clipWindow, Color color) {
    if (!clipLineCohenSutherland(a.x, a.y, b.x, b.y, clipWindow)) {
        return;
    }
    drawLineBresenham(canvas, roundToPixel(a.x), roundToPixel(a.y),
                      roundToPixel(b.x), roundToPixel(b.y), color);
}

} // namespace raster
