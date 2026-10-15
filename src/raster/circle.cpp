#include "raster/circle.h"

#include <cmath>

namespace raster {

namespace {

// Midpoint circle algorithm for ONE octant.
//
// Calls visit(x, y) for the pixels of the circle x^2 + y^2 = r^2 (centre at
// the origin) in the octant from (0, r) to the 45-degree point, where
// 0 <= x <= y. The other seven octants are mirror images of this one, so
// callers get the whole circle by swapping and negating x and y:
//     (x, y) (y, x) (-x, y) (-y, x) (x, -y) (y, -x) (-x, -y) (-y, -x)
// This is "8-way symmetry": we compute 1/8 of the circle and draw 8 pixels.
//
// In this octant the circle is shallow (slope between 0 and -1), so x goes
// up by exactly 1 each step and the only question is whether y stays or
// goes down by 1.
//
// Decision variable: F(x, y) = x^2 + y^2 - r^2 is < 0 inside the circle,
// 0 on it and > 0 outside. We test the MIDPOINT between the two candidate
// pixels, M = (x + 1, y - 1/2):
//     d = F(M) < 0  -> M is inside, the circle passes above M: keep y
//     d = F(M) >= 0 -> M is on/outside the circle: step down to y - 1
// For a fixed x, F grows with y, so this picks whichever candidate is
// nearer the true circle: every pixel is within half a pixel of it.
//
// Start (x = 0, y = r): d = F(1, r - 1/2) = 5/4 - r. We use 1 - r instead.
// d is only ever compared with 0 and every update is an integer, so
// dropping the 1/4 never changes a decision, and everything stays integer.
// Updates (from expanding F at the next midpoint):
//     keep y:       d += 2x + 1          (with the new x)
//     step y down:  d += 2(x - y) + 1    (with the new x and y)
template <typename Visit>
void walkCircleOctant(int radius, Visit visit) {
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    while (x <= y) {
        visit(x, y);
        ++x;
        if (d < 0) {
            d += 2 * x + 1;
        } else {
            --y;
            d += 2 * (x - y) + 1;
        }
    }
}

// Calls plot(px, py) for all 8 mirror images of (x, y) around the centre.
template <typename Plot>
void plotEight(int cx, int cy, int x, int y, Plot plot) {
    plot(cx + x, cy + y);
    plot(cx + y, cy + x);
    plot(cx - x, cy + y);
    plot(cx - y, cy + x);
    plot(cx + x, cy - y);
    plot(cx + y, cy - x);
    plot(cx - x, cy - y);
    plot(cx - y, cy - x);
}

// Angle of the pixel offset (dx, dy) in degrees, in [0, 360), clockwise on
// screen from +x (because y points down).
double angleOf(int dx, int dy) {
    double degrees = std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * 180.0 / kPi;
    if (degrees < 0.0) {
        degrees += 360.0;
    }
    return degrees;
}

} // namespace

void drawCircleMidpoint(Canvas& canvas, int centerX, int centerY, int radius, Color color) {
    if (radius < 0) {
        return;
    }
    walkCircleOctant(radius, [&](int x, int y) {
        plotEight(centerX, centerY, x, y, [&](int px, int py) { canvas.setPixel(px, py, color); });
    });
}

// Filled disc: every octant point (x, y) gives the two pixel rows cy + y and
// cy - y, which run from cx - x to cx + x, and the two rows cy + x and cy - x,
// which run from cx - y to cx + y. Filling those horizontal spans covers the
// outline and everything inside it. Some rows are filled more than once
// (each time with the same or a wider span), which is harmless.
void fillCircleMidpoint(Canvas& canvas, int centerX, int centerY, int radius, Color color) {
    if (radius < 0) {
        return;
    }
    auto span = [&](int y, int xFrom, int xTo) {
        for (int x = xFrom; x <= xTo; ++x) {
            canvas.setPixel(x, y, color);
        }
    };
    walkCircleOctant(radius, [&](int x, int y) {
        span(centerY + y, centerX - x, centerX + x);
        span(centerY - y, centerX - x, centerX + x);
        span(centerY + x, centerX - y, centerX + y);
        span(centerY - x, centerX - y, centerX + y);
    });
}

// Arc: generate the full circle with the midpoint algorithm, but only plot
// the pixels whose angle (seen from the centre) lies inside the sweep.
// Each candidate's angle is measured relative to startDegrees and wrapped
// into [0, 360), so arcs that cross 0 degrees need no special case.
void drawCircleArc(Canvas& canvas, int centerX, int centerY, int radius,
                   float startDegrees, float endDegrees, Color color) {
    if (radius < 0) {
        return;
    }
    double sweep = static_cast<double>(endDegrees) - startDegrees;
    if (sweep >= 360.0) {
        drawCircleMidpoint(canvas, centerX, centerY, radius, color);
        return;
    }
    while (sweep < 0.0) {
        sweep += 360.0; // e.g. 270 -> 90 becomes a 180-degree sweep
    }
    double start = std::fmod(static_cast<double>(startDegrees), 360.0);
    if (start < 0.0) {
        start += 360.0;
    }

    const double tolerance = 1e-9;
    walkCircleOctant(radius, [&](int x, int y) {
        plotEight(centerX, centerY, x, y, [&](int px, int py) {
            const int dx = px - centerX;
            const int dy = py - centerY;
            if (dx == 0 && dy == 0) {
                canvas.setPixel(px, py, color); // radius 0: the arc is the centre
                return;
            }
            double relative = angleOf(dx, dy) - start;
            if (relative < 0.0) {
                relative += 360.0;
            }
            if (relative <= sweep + tolerance) {
                canvas.setPixel(px, py, color);
            }
        });
    });
}

} // namespace raster
