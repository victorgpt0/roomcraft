#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/ellipse.h"
#include "raster/fill.h"
#include "raster/line.h"

#include "test_helpers.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

const raster::Color kBlack{0, 0, 0};
const raster::Color kGrey{128, 128, 128};

constexpr int kSize = 200;
constexpr int kCX = 100;
constexpr int kCY = 100;

bool isBlack(const raster::Canvas& canvas, int x, int y) {
    raster::Color c = canvas.getPixel(x, y);
    return c.r == 0 && c.g == 0 && c.b == 0;
}

int countBlack(const raster::Canvas& canvas) {
    int n = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            if (isBlack(canvas, x, y)) {
                ++n;
            }
        }
    }
    return n;
}

raster::Canvas blank() {
    raster::Canvas canvas(kSize, kSize);
    canvas.clear(255, 255, 255);
    return canvas;
}

// How far pixel offset (dx, dy) is from the ideal ellipse x^2/rx^2 + y^2/ry^2 = 1,
// measured vertically or horizontally, whichever is smaller. A midpoint
// algorithm picks, along its stepping axis, the pixel nearest the curve,
// so this must be at most 0.5.
double distanceToEllipse(int dx, int dy, int rx, int ry) {
    const double ax = std::abs(dx);
    const double ay = std::abs(dy);
    double best = 1e9;
    if (ax <= rx) {
        const double idealY = ry * std::sqrt(1.0 - (ax / rx) * (ax / rx));
        best = std::min(best, std::fabs(ay - idealY));
    }
    if (ay <= ry) {
        const double idealX = rx * std::sqrt(1.0 - (ay / ry) * (ay / ry));
        best = std::min(best, std::fabs(ax - idealX));
    }
    return best;
}

// Properties every outline from drawCircleMidpoint / drawEllipseMidpoint
// must have. `isCircle` adds the diagonal (8-way) symmetry check.
void checkOutline(const std::string& what, const raster::Canvas& canvas, int rx, int ry, bool isCircle) {
    // The four extreme points.
    CHECK_MSG(isBlack(canvas, kCX + rx, kCY) && isBlack(canvas, kCX - rx, kCY), what + ": left/right extremes");
    CHECK_MSG(isBlack(canvas, kCX, kCY + ry) && isBlack(canvas, kCX, kCY - ry), what + ": top/bottom extremes");

    bool symmetric = true;
    bool diagonal = true;
    bool close = true;
    bool bounded = true;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (!isBlack(canvas, x, y)) {
                continue;
            }
            const int dx = x - kCX;
            const int dy = y - kCY;
            if (!isBlack(canvas, kCX - dx, kCY + dy) || !isBlack(canvas, kCX + dx, kCY - dy)) {
                symmetric = false;
            }
            if (isCircle && !isBlack(canvas, kCX + dy, kCY + dx)) {
                diagonal = false;
            }
            if (std::abs(dx) > rx || std::abs(dy) > ry) {
                bounded = false;
            }
            if (distanceToEllipse(dx, dy, rx, ry) > 0.5 + 1e-9) {
                close = false;
            }
        }
    }
    CHECK_MSG(symmetric, what + ": not symmetric about both axes");
    CHECK_MSG(diagonal, what + ": not symmetric about the diagonal");
    CHECK_MSG(bounded, what + ": pixel outside the bounding box");
    CHECK_MSG(close, what + ": pixel more than half a pixel from the ideal curve");

    // Closed outline (no gaps): a 4-connected fill from the centre must stay
    // inside. Needs an interior, so only for radii >= 2.
    if (rx >= 2 && ry >= 2) {
        raster::Canvas copy = canvas;
        raster::boundaryFill(copy, kCX, kCY, kGrey, kBlack);
        const raster::Color corner = copy.getPixel(0, 0);
        CHECK_MSG(corner.r == 255, what + ": fill leaked out, the outline has a gap");
    }
}

// The fill must cover exactly the outline and what is inside it: on each
// row, the filled pixels form one run from the leftmost to the rightmost
// outline pixel of that row.
void checkFillMatchesOutline(const std::string& what, const raster::Canvas& outline,
                             const raster::Canvas& filled) {
    bool ok = true;
    for (int y = 0; y < kSize; ++y) {
        int left = kSize;
        int right = -1;
        for (int x = 0; x < kSize; ++x) {
            if (isBlack(outline, x, y)) {
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
        for (int x = 0; x < kSize; ++x) {
            const bool expected = x >= left && x <= right;
            if (isBlack(filled, x, y) != expected) {
                ok = false;
            }
        }
    }
    CHECK_MSG(ok, what + ": fill does not match outline");
}

void testCircles() {
    for (int r : {1, 2, 3, 4, 5, 7, 10, 17, 33, 50, 64, 90}) {
        const std::string what = "circle r=" + std::to_string(r);
        raster::Canvas outline = blank();
        raster::drawCircleMidpoint(outline, kCX, kCY, r, kBlack);
        checkOutline(what, outline, r, r, true);

        raster::Canvas filled = blank();
        raster::fillCircleMidpoint(filled, kCX, kCY, r, kBlack);
        checkFillMatchesOutline(what, outline, filled);

        // Pixel count close to pi r^2. The outline pixels lie up to half a
        // pixel outside the true circle, so the count is about pi (r + 1/2)^2;
        // allow a difference of up to the circumference, 2 pi r.
        const double area = raster::kPi * r * r;
        CHECK_MSG(std::fabs(countBlack(filled) - area) <= 2.0 * raster::kPi * r,
                  what + ": filled " + std::to_string(countBlack(filled)) + " pixels, pi r^2 = " +
                      std::to_string(area));
    }

    // Every radius up to 60.
    for (int r = 1; r <= 60; ++r) {
        raster::Canvas outline = blank();
        raster::drawCircleMidpoint(outline, kCX, kCY, r, kBlack);
        checkOutline("circle r=" + std::to_string(r), outline, r, r, true);
    }

    // Large radius: within 2.5% of pi r^2.
    raster::Canvas filled = blank();
    raster::fillCircleMidpoint(filled, kCX, kCY, 90, kBlack);
    const double area90 = raster::kPi * 90 * 90;
    CHECK_NEAR(countBlack(filled) / area90, 1.0, 0.025);

    // Degenerate radii.
    raster::Canvas c = blank();
    raster::drawCircleMidpoint(c, kCX, kCY, 0, kBlack);
    CHECK(countBlack(c) == 1 && isBlack(c, kCX, kCY));
    c = blank();
    raster::drawCircleMidpoint(c, kCX, kCY, -3, kBlack);
    raster::fillCircleMidpoint(c, kCX, kCY, -3, kBlack);
    CHECK(countBlack(c) == 0);

    // Partly off the canvas: no crash, the visible part is drawn.
    c = blank();
    raster::fillCircleMidpoint(c, 0, 0, 20, kBlack);
    CHECK(isBlack(c, 0, 0) && isBlack(c, 19, 0) && !isBlack(c, 21, 0));
}

void testEllipses() {
    const int sizes[][2] = {{60, 30}, {30, 60}, {5, 40}, {40, 5}, {1, 10}, {10, 1},
                            {2, 3},   {3, 2},   {45, 20}, {90, 89}, {7, 7}, {50, 50}};
    for (const auto& s : sizes) {
        const int rx = s[0];
        const int ry = s[1];
        const std::string what = "ellipse " + std::to_string(rx) + "x" + std::to_string(ry);
        raster::Canvas outline = blank();
        raster::drawEllipseMidpoint(outline, kCX, kCY, rx, ry, kBlack);
        checkOutline(what, outline, rx, ry, false);

        raster::Canvas filled = blank();
        raster::fillEllipseMidpoint(filled, kCX, kCY, rx, ry, kBlack);
        checkFillMatchesOutline(what, outline, filled);

        const double area = raster::kPi * rx * ry;
        CHECK_MSG(std::fabs(countBlack(filled) - area) <= raster::kPi * (rx + ry),
                  what + ": filled " + std::to_string(countBlack(filled)) + " pixels, pi rx ry = " +
                      std::to_string(area));
    }

    // Sweep of small sizes, where midpoint ellipses are most fragile (thin
    // shapes, region changes after one or two steps).
    for (int rx = 1; rx <= 16; ++rx) {
        for (int ry = 1; ry <= 16; ++ry) {
            raster::Canvas outline = blank();
            raster::drawEllipseMidpoint(outline, kCX, kCY, rx, ry, kBlack);
            checkOutline("ellipse " + std::to_string(rx) + "x" + std::to_string(ry), outline, rx, ry, false);
        }
    }

    // An ellipse with rx == ry is a circle: compare with the circle algorithm.
    raster::Canvas e = blank();
    raster::Canvas c = blank();
    raster::drawEllipseMidpoint(e, kCX, kCY, 40, 40, kBlack);
    raster::drawCircleMidpoint(c, kCX, kCY, 40, kBlack);
    int differences = 0;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (isBlack(e, x, y) != isBlack(c, x, y)) {
                ++differences;
            }
        }
    }
    CHECK_MSG(differences == 0, std::to_string(differences) + " pixels differ between ellipse and circle r=40");

    // Flat ellipses are straight lines.
    e = blank();
    raster::drawEllipseMidpoint(e, kCX, kCY, 30, 0, kBlack);
    CHECK(countBlack(e) == 61);
    e = blank();
    raster::fillEllipseMidpoint(e, kCX, kCY, 0, 12, kBlack);
    CHECK(countBlack(e) == 25);
    e = blank();
    raster::drawEllipseMidpoint(e, kCX, kCY, -1, 5, kBlack);
    CHECK(countBlack(e) == 0);

    // Large radii do not overflow (rx^2 * ry^2 is far beyond int range).
    raster::Canvas big(10, 10);
    raster::drawEllipseMidpoint(big, 0, 0, 3000, 2000, kBlack);
}

void testArcs() {
    const int r = 40;

    // A full sweep equals the circle.
    raster::Canvas full = blank();
    raster::Canvas circle = blank();
    raster::drawCircleArc(full, kCX, kCY, r, 0.0f, 360.0f, kBlack);
    raster::drawCircleMidpoint(circle, kCX, kCY, r, kBlack);
    int differences = 0;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (isBlack(full, x, y) != isBlack(circle, x, y)) {
                ++differences;
            }
        }
    }
    CHECK(differences == 0);

    // 0 -> 90 degrees is the quarter from "right" clockwise to "down":
    // every pixel has dx >= 0 and dy >= 0 (y down).
    raster::Canvas quarter = blank();
    raster::drawCircleArc(quarter, kCX, kCY, r, 0.0f, 90.0f, kBlack);
    CHECK(isBlack(quarter, kCX + r, kCY));
    CHECK(isBlack(quarter, kCX, kCY + r));
    CHECK(!isBlack(quarter, kCX - r, kCY));
    CHECK(!isBlack(quarter, kCX, kCY - r));
    bool inQuadrant = true;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (isBlack(quarter, x, y) && (x < kCX || y < kCY)) {
                inQuadrant = false;
            }
        }
    }
    CHECK(inQuadrant);

    // The four quarters together make the whole circle, with no pixel lost.
    raster::Canvas quarters = blank();
    for (int q = 0; q < 4; ++q) {
        const float start = static_cast<float>(q) * 90.0f;
        raster::drawCircleArc(quarters, kCX, kCY, r, start, start + 90.0f, kBlack);
    }
    CHECK(countBlack(quarters) == countBlack(circle));

    // Wrap-around: 270 (up) -> 90 (down) going clockwise is the right half.
    raster::Canvas right = blank();
    raster::drawCircleArc(right, kCX, kCY, r, 270.0f, 90.0f, kBlack);
    CHECK(isBlack(right, kCX + r, kCY));
    CHECK(isBlack(right, kCX, kCY - r) && isBlack(right, kCX, kCY + r));
    CHECK(!isBlack(right, kCX - r, kCY));
    bool rightHalf = true;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (isBlack(right, x, y) && x < kCX) {
                rightHalf = false;
            }
        }
    }
    CHECK(rightHalf);

    // Negative angles are the same as angle + 360.
    raster::Canvas a = blank();
    raster::Canvas b = blank();
    raster::drawCircleArc(a, kCX, kCY, r, -90.0f, 0.0f, kBlack);
    raster::drawCircleArc(b, kCX, kCY, r, 270.0f, 360.0f, kBlack);
    CHECK(countBlack(a) == countBlack(b) && countBlack(a) > 0);
}

// Visual evidence: RoomCraft-style round table with chairs, a rug, a door swing.
void writeImage() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    // Round table (filled + outline) with four chairs.
    const raster::Color wood{181, 136, 99};
    const raster::Color chair{90, 120, 160};
    raster::fillCircleMidpoint(canvas, 80, 80, 35, wood);
    raster::drawCircleMidpoint(canvas, 80, 80, 35, kBlack);
    const int chairs[4][2] = {{80, 30}, {130, 80}, {80, 130}, {30, 80}};
    for (const auto& p : chairs) {
        raster::fillCircleMidpoint(canvas, p[0], p[1], 10, chair);
        raster::drawCircleMidpoint(canvas, p[0], p[1], 10, kBlack);
    }

    // Oval rug with a border, and an ellipse outline (pillow).
    raster::fillEllipseMidpoint(canvas, 180, 190, 60, 30, raster::Color{200, 0, 200});
    raster::fillEllipseMidpoint(canvas, 180, 190, 52, 22, raster::Color{240, 170, 240});
    raster::drawEllipseMidpoint(canvas, 200, 60, 30, 14, kBlack);

    // Door swing: hinge at (30, 240), door 60 px wide, opening upwards from
    // the wall (0 degrees = along the wall to the right, 270 = up).
    raster::drawThickLine(canvas, 0, 240, 30, 240, 3, kBlack);    // wall
    raster::drawThickLine(canvas, 90, 240, 120, 240, 3, kBlack);  // wall
    raster::drawLineBresenham(canvas, 30, 240, 30, 180, kBlack);   // open door leaf
    raster::drawCircleArc(canvas, 30, 240, 60, 270.0f, 360.0f, kGrey);

    CHECK(canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_circle_ellipse.ppm"));
    CHECK(canvas.saveBMP(std::string(TESTS_OUTPUT_DIR) + "/test_circle_ellipse.bmp"));
}

} // namespace

int main() {
    testCircles();
    testEllipses();
    testArcs();
    writeImage();
    return test::summary("test_circle_ellipse");
}
