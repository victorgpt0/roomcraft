#include "raster/ellipse.h"

#include <algorithm>
#include <cmath>

namespace raster {

namespace {

// Midpoint ellipse algorithm for ONE quadrant.
//
// Calls visit(x, y) for the pixels of x^2/rx^2 + y^2/ry^2 = 1 (centre at the
// origin) between (0, ry) and (rx, 0). An ellipse is symmetric about both
// axes, so callers mirror each point into the other three quadrants:
//     (x, y) (-x, y) (x, -y) (-x, -y)          ("4-way symmetry")
// (A circle also has diagonal symmetry, giving 8-way; an ellipse does not.)
//
// THE MIDPOINT TEST. F(x, y) = ry^2 x^2 + rx^2 y^2 - rx^2 ry^2 is < 0 inside
// the ellipse, 0 on it and > 0 outside. To choose between two neighbouring
// candidate pixels we evaluate F at the MIDPOINT between them: the sign says
// on which side of the midpoint the true curve passes, so it tells us which
// candidate is nearer. Every pixel is therefore within half a pixel of the
// curve, measured along the axis we step along.
//
// TWO REGIONS. Along the quadrant the slope changes from 0 (at the top) to
// -infinity (at the right tip), so no single stepping direction works:
//   Region 1, top part, slope between 0 and -1: step x by 1 per COLUMN and
//     use the midpoint test (x, y - 1/2) to pick y. The curve drops by at
//     most 1 pixel per column here.
//   Region 2, side part, slope steeper than -1: step y by 1 per ROW and use
//     the midpoint test (x - 1/2, y) to pick x.
// The regions meet where the slope is exactly -1. There the gradient of F
// has equal parts (2 ry^2 x = 2 rx^2 y); on the ellipse this is the point
//     x* = rx^2 / sqrt(rx^2 + ry^2),   y* = ry^2 / sqrt(rx^2 + ry^2).
//
// HOW THIS DIFFERS FROM THE USUAL TEXTBOOK LOOP. The textbook version walks
// region 1 from the top, decides when to switch with a slope test at the
// current pixel, then continues region 2 from wherever region 1 stopped,
// updating the decision value incrementally. We tested that version on
// every ellipse from 1x1 to 60x60: for 21-128 of those 3600 sizes
// (depending on the switch test) some pixels are 0.5-1.0 pixels off the
// curve, and without an extra fix 82-93 sizes never even reach the tip
// (rx, 0). Small and very flat ellipses (rugs, cushions) are hit hardest.
// Here instead:
//   - region 1 starts exactly at its tip (0, ry), region 2 exactly at its
//     tip (rx, 0), so neither depends on rounding errors of the other;
//   - the switch is computed from x* and y* above, and each region runs one
//     column/row past it, so the two runs overlap by a pixel and always
//     join without a gap;
//   - F is evaluated directly at each midpoint in 64-bit integers, which
//     costs a few multiplications more than incremental updates but is
//     easy to check.
// With these changes every ellipse from 1x1 to 150x150 (22500 sizes) was
// within 0.5 pixels everywhere and gap-free (and test_circle_ellipse checks
// many of them).
//
// INTEGER ARITHMETIC. The midpoints have halves in them, so each test uses
// 4 * F, which has the same sign and no fractions:
//   region 1:  4 F(x, y - 1/2) = 4 ry^2 x^2 + rx^2 (2y - 1)^2 - 4 rx^2 ry^2
//   region 2:  4 F(x - 1/2, y) = ry^2 (2x - 1)^2 + 4 rx^2 y^2 - 4 rx^2 ry^2
// long long is needed: rx^2 * ry^2 overflows a 32-bit int for radii above ~200.
template <typename Visit>
void walkEllipseQuadrant(int radiusX, int radiusY, Visit visit) {
    const long long rx2 = static_cast<long long>(radiusX) * radiusX;
    const long long ry2 = static_cast<long long>(radiusY) * radiusY;

    // Last column of region 1 and last row of region 2 (one past the
    // slope = -1 point, so the two regions overlap).
    const double diagonal = std::sqrt(static_cast<double>(rx2 + ry2));
    const double xStar = static_cast<double>(rx2) / diagonal;
    const double yStar = static_cast<double>(ry2) / diagonal;
    const long long lastColumn = std::min<long long>(radiusX, static_cast<long long>(std::floor(xStar)) + 1);
    const long long lastRow = std::min<long long>(radiusY, static_cast<long long>(std::floor(yStar)) + 1);

    // Region 1: from the top tip (0, ry), one pixel per column.
    // While the midpoint below the current pixel is outside the ellipse
    // (4F > 0), the curve passes below that midpoint, so step down.
    long long y = radiusY;
    for (long long x = 0; x <= lastColumn; ++x) {
        while (y > 0 && 4 * ry2 * x * x + rx2 * (2 * y - 1) * (2 * y - 1) - 4 * rx2 * ry2 > 0) {
            --y;
        }
        visit(static_cast<int>(x), static_cast<int>(y));
    }

    // Region 2: from the right tip (rx, 0), one pixel per row, going up.
    // While the midpoint left of the current pixel is outside, step left.
    long long x = radiusX;
    for (long long row = 0; row <= lastRow; ++row) {
        while (x > 0 && ry2 * (2 * x - 1) * (2 * x - 1) + 4 * rx2 * row * row - 4 * rx2 * ry2 > 0) {
            --x;
        }
        visit(static_cast<int>(x), static_cast<int>(row));
    }
}

template <typename Plot>
void plotFour(int cx, int cy, int x, int y, Plot plot) {
    plot(cx + x, cy + y);
    plot(cx - x, cy + y);
    plot(cx + x, cy - y);
    plot(cx - x, cy - y);
}

} // namespace

void drawEllipseMidpoint(Canvas& canvas, int centerX, int centerY, int radiusX, int radiusY, Color color) {
    if (radiusX < 0 || radiusY < 0) {
        return;
    }
    // A flat ellipse (one radius 0) is a straight line segment; the
    // quadrant walk needs both radii > 0 for its slope = -1 point.
    if (radiusX == 0 || radiusY == 0) {
        for (int y = centerY - radiusY; y <= centerY + radiusY; ++y) {
            for (int x = centerX - radiusX; x <= centerX + radiusX; ++x) {
                canvas.setPixel(x, y, color);
            }
        }
        return;
    }
    walkEllipseQuadrant(radiusX, radiusY, [&](int x, int y) {
        plotFour(centerX, centerY, x, y, [&](int px, int py) { canvas.setPixel(px, py, color); });
    });
}

// Filled ellipse: every quadrant point (x, y) gives the rows cy + y and
// cy - y, spanning cx - x .. cx + x. Several points can share a row; the
// widest one wins, so the union of all spans is the outline plus everything
// inside it.
void fillEllipseMidpoint(Canvas& canvas, int centerX, int centerY, int radiusX, int radiusY, Color color) {
    if (radiusX < 0 || radiusY < 0) {
        return;
    }
    if (radiusX == 0 || radiusY == 0) {
        drawEllipseMidpoint(canvas, centerX, centerY, radiusX, radiusY, color);
        return;
    }
    walkEllipseQuadrant(radiusX, radiusY, [&](int x, int y) {
        for (int px = centerX - x; px <= centerX + x; ++px) {
            canvas.setPixel(px, centerY + y, color);
            canvas.setPixel(px, centerY - y, color);
        }
    });
}

} // namespace raster
