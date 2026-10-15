#include "raster/canvas.h"
#include "raster/line.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {

const raster::Color kWhite{255, 255, 255};
const raster::Color kBlack{0, 0, 0};

using LineFunction = void (*)(raster::Canvas&, int, int, int, int, raster::Color);

bool isBlack(const raster::Canvas& canvas, int x, int y) {
    raster::Color c = canvas.getPixel(x, y);
    return c.r == 0 && c.g == 0 && c.b == 0;
}

int countBlack(const raster::Canvas& canvas) {
    int count = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            if (isBlack(canvas, x, y)) {
                ++count;
            }
        }
    }
    return count;
}

std::string describe(const char* name, int x0, int y0, int x1, int y1) {
    return std::string(name) + " (" + std::to_string(x0) + "," + std::to_string(y0) + ")->(" +
           std::to_string(x1) + "," + std::to_string(y1) + ")";
}

// Draws one line on a blank canvas and checks the properties every correct
// 1-pixel line must have:
//   - both endpoints are drawn
//   - exactly one pixel per step along the major axis (so no gaps, no extras):
//     total pixels = max(|dx|, |dy|) + 1
//   - consecutive pixels touch (minor coordinate changes by at most 1)
//   - every pixel is within half a pixel of the ideal line, measured along
//     the minor axis
void checkLineProperties(const char* name, LineFunction draw,
                         int x0, int y0, int x1, int y1, double tolerance) {
    raster::Canvas canvas(40, 40);
    canvas.clear(255, 255, 255);
    draw(canvas, x0, y0, x1, y1, kBlack);

    const std::string what = describe(name, x0, y0, x1, y1);
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int major = std::max(std::abs(dx), std::abs(dy));

    CHECK_MSG(isBlack(canvas, x0, y0), what + ": start pixel missing");
    CHECK_MSG(isBlack(canvas, x1, y1), what + ": end pixel missing");
    CHECK_MSG(countBlack(canvas) == major + 1, what + ": wrong pixel count");

    if (major == 0) {
        return;
    }

    const bool steep = std::abs(dy) > std::abs(dx);
    const int step = steep ? (dy > 0 ? 1 : -1) : (dx > 0 ? 1 : -1);
    const int majorStart = steep ? y0 : x0;
    const int majorEnd = steep ? y1 : x1;

    int previousMinor = steep ? x0 : y0;
    for (int m = majorStart; m != majorEnd + step; m += step) {
        // Find the pixels drawn in this column (shallow) or row (steep).
        int found = 0;
        int minor = 0;
        for (int k = 0; k < 40; ++k) {
            const bool black = steep ? isBlack(canvas, k, m) : isBlack(canvas, m, k);
            if (black) {
                ++found;
                minor = k;
            }
        }
        if (!CHECK_MSG(found == 1, what + ": expected exactly 1 pixel at major coordinate " +
                                       std::to_string(m) + ", found " + std::to_string(found))) {
            return;
        }

        // Ideal minor coordinate from the line equation.
        const double t = static_cast<double>(m - majorStart) / (majorEnd - majorStart);
        const double ideal = steep ? x0 + t * dx : y0 + t * dy;
        CHECK_MSG(std::fabs(minor - ideal) <= 0.5 + tolerance,
                  what + ": pixel at major " + std::to_string(m) + " is more than half a pixel off");
        CHECK_MSG(std::abs(minor - previousMinor) <= 1, what + ": gap between consecutive pixels");
        previousMinor = minor;
    }
}

bool clipCase(float x0, float y0, float x1, float y1, raster::Rect window,
              float& outX0, float& outY0, float& outX1, float& outY1) {
    outX0 = x0;
    outY0 = y0;
    outX1 = x1;
    outY1 = y1;
    return raster::clipLineCohenSutherland(outX0, outY0, outX1, outY1, window);
}

void testCohenSutherland() {
    const raster::Rect window{50.0f, 50.0f, 200.0f, 200.0f};
    const double eps = 1e-4;
    float ax = 0, ay = 0, bx = 0, by = 0;

    // Fully inside: accepted and unchanged.
    CHECK(clipCase(60, 70, 150, 180, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 60, eps);
    CHECK_NEAR(ay, 70, eps);
    CHECK_NEAR(bx, 150, eps);
    CHECK_NEAR(by, 180, eps);

    // Trivial rejects: both endpoints outside the same edge.
    CHECK(!clipCase(0, 10, 40, 190, window, ax, ay, bx, by));     // both left
    CHECK(!clipCase(60, 10, 190, 40, window, ax, ay, bx, by));    // both above
    CHECK(!clipCase(210, 60, 300, 250, window, ax, ay, bx, by));  // both right
    CHECK(!clipCase(60, 210, 190, 300, window, ax, ay, bx, by));  // both below

    // Non-trivial reject: one end left, one end above, and the segment passes
    // outside the top-left corner (x + y = 90 never reaches (50,50)).
    CHECK(!clipCase(0, 90, 90, 0, window, ax, ay, bx, by));

    // Diagonal through the whole window, both directions.
    CHECK(clipCase(0, 0, 255, 255, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 50, eps);
    CHECK_NEAR(ay, 50, eps);
    CHECK_NEAR(bx, 200, eps);
    CHECK_NEAR(by, 200, eps);
    CHECK(clipCase(255, 255, 0, 0, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 200, eps);
    CHECK_NEAR(ay, 200, eps);
    CHECK_NEAR(bx, 50, eps);
    CHECK_NEAR(by, 50, eps);

    // Horizontal and vertical lines crossing the window.
    CHECK(clipCase(0, 100, 300, 100, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 50, eps);
    CHECK_NEAR(ay, 100, eps);
    CHECK_NEAR(bx, 200, eps);
    CHECK_NEAR(by, 100, eps);
    CHECK(clipCase(120, 0, 120, 300, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 120, eps);
    CHECK_NEAR(ay, 50, eps);
    CHECK_NEAR(bx, 120, eps);
    CHECK_NEAR(by, 200, eps);

    // One endpoint inside: only the outside end moves. The line from
    // (100,100) to (300,150) has slope 1/4, so it leaves at x = 200, y = 125.
    CHECK(clipCase(100, 100, 300, 150, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 100, eps);
    CHECK_NEAR(ay, 100, eps);
    CHECK_NEAR(bx, 200, eps);
    CHECK_NEAR(by, 125, eps);

    // A line lying exactly on the left edge counts as inside.
    CHECK(clipCase(50, 10, 50, 300, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 50, eps);
    CHECK_NEAR(ay, 50, eps);
    CHECK_NEAR(bx, 50, eps);
    CHECK_NEAR(by, 200, eps);

    // Start point in the top-left corner region (code TOP + LEFT) needs two
    // moves: first onto y = 50 (giving x = 25, still left), then onto x = 50.
    // Line y = 2x from (0,0) to (150,300); the end leaves through the bottom.
    CHECK(clipCase(0, 0, 150, 300, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 50, eps);
    CHECK_NEAR(ay, 100, eps);
    CHECK_NEAR(bx, 100, eps);
    CHECK_NEAR(by, 200, eps);

    // Touching only the bottom-left corner: line y = x + 150 meets the window
    // in the single point (50, 200), which counts as inside.
    CHECK(clipCase(0, 150, 100, 250, window, ax, ay, bx, by));
    CHECK_NEAR(ax, 50, eps);
    CHECK_NEAR(ay, 200, eps);
    CHECK_NEAR(bx, 50, eps);
    CHECK_NEAR(by, 200, eps);

    // Degenerate segments (a single point).
    CHECK(clipCase(100, 100, 100, 100, window, ax, ay, bx, by));
    CHECK(!clipCase(10, 100, 10, 100, window, ax, ay, bx, by));
}

void testThickLine() {
    // Horizontal, thickness 5: a 5-pixel vertical span at each of 40 x values.
    raster::Canvas canvas(64, 64);
    canvas.clear(255, 255, 255);
    raster::drawThickLine(canvas, 10, 20, 49, 20, 5, kBlack);
    CHECK(countBlack(canvas) == 40 * 5);
    CHECK(isBlack(canvas, 10, 18) && isBlack(canvas, 10, 22));
    CHECK(!isBlack(canvas, 10, 17) && !isBlack(canvas, 10, 23));
    CHECK(!isBlack(canvas, 9, 20) && !isBlack(canvas, 50, 20));

    // Vertical, thickness 4: horizontal spans, extra pixel on the right.
    canvas.clear(255, 255, 255);
    raster::drawThickLine(canvas, 30, 5, 30, 54, 4, kBlack);
    CHECK(countBlack(canvas) == 50 * 4);
    CHECK(isBlack(canvas, 29, 30) && isBlack(canvas, 32, 30));
    CHECK(!isBlack(canvas, 28, 30) && !isBlack(canvas, 33, 30));

    // 45 degrees, thickness 3: the minor-axis span must be stretched to
    // 3 * sqrt(2) = 4.24 -> 4 pixels so the line is 3 pixels thick across.
    canvas.clear(255, 255, 255);
    raster::drawThickLine(canvas, 10, 10, 40, 40, 3, kBlack);
    CHECK(countBlack(canvas) == 31 * 4);

    // Thickness 1 (or less) is an ordinary line.
    canvas.clear(255, 255, 255);
    raster::drawThickLine(canvas, 10, 10, 40, 20, 1, kBlack);
    CHECK(countBlack(canvas) == 31);
}

void testDashedLine() {
    raster::Canvas canvas(32, 16);
    canvas.clear(255, 255, 255);
    raster::drawDashedLine(canvas, 0, 5, 19, 5, 4, 2, kBlack);
    for (int x = 0; x <= 19; ++x) {
        const bool expected = (x % 6) < 4; // 4 on, 2 off
        CHECK_MSG(isBlack(canvas, x, 5) == expected, "dash pattern at x = " + std::to_string(x));
    }

    // No gap gives a solid line; no dash draws nothing.
    canvas.clear(255, 255, 255);
    raster::drawDashedLine(canvas, 0, 5, 19, 5, 3, 0, kBlack);
    CHECK(countBlack(canvas) == 20);
    canvas.clear(255, 255, 255);
    raster::drawDashedLine(canvas, 0, 5, 19, 5, 0, 3, kBlack);
    CHECK(countBlack(canvas) == 0);
}

void testPolylineAndOutline() {
    raster::Canvas canvas(64, 64);

    // A 20x20 square outline has 4 * 20 distinct pixels.
    canvas.clear(255, 255, 255);
    raster::Polygon square;
    square.vertices = {{10, 10}, {30, 10}, {30, 30}, {10, 30}};
    raster::drawPolygonOutline(canvas, square, kBlack);
    CHECK(countBlack(canvas) == 80);
    CHECK(isBlack(canvas, 10, 10) && isBlack(canvas, 30, 10) &&
          isBlack(canvas, 30, 30) && isBlack(canvas, 10, 30));
    CHECK(isBlack(canvas, 10, 20)); // closing edge (last -> first) drawn
    CHECK(!isBlack(canvas, 20, 20)); // interior untouched

    // The same points as an open polyline miss the closing edge.
    canvas.clear(255, 255, 255);
    raster::drawPolyline(canvas, square.vertices, kBlack);
    CHECK(countBlack(canvas) == 61);
    CHECK(!isBlack(canvas, 10, 20));

    // Float vertices are rounded to the nearest pixel centre.
    canvas.clear(255, 255, 255);
    raster::drawPolyline(canvas, {{4.6f, 5.4f}}, kBlack);
    CHECK(isBlack(canvas, 5, 5));
    CHECK(countBlack(canvas) == 1);

    // Empty input draws nothing (and does not crash).
    canvas.clear(255, 255, 255);
    raster::drawPolyline(canvas, {}, kBlack);
    raster::drawPolygonOutline(canvas, raster::Polygon{}, kBlack);
    CHECK(countBlack(canvas) == 0);
}

void testDrawLineClipped() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);
    raster::drawLineClipped(canvas, {0, 0}, {255, 255}, raster::Rect{50, 50, 200, 200}, kBlack);
    CHECK(countBlack(canvas) == 151); // (50,50) .. (200,200)
    CHECK(isBlack(canvas, 50, 50) && isBlack(canvas, 200, 200));
    CHECK(!isBlack(canvas, 49, 49) && !isBlack(canvas, 201, 201));

    canvas.clear(255, 255, 255);
    raster::drawLineClipped(canvas, {0, 90}, {90, 0}, raster::Rect{50, 50, 200, 200}, kBlack);
    CHECK(countBlack(canvas) == 0); // fully rejected
}

// Visual evidence: every line feature on one image.
void writeImage() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    const raster::Color red{255, 0, 0};
    const raster::Color blue{0, 0, 255};
    const raster::Color grey{170, 170, 170};
    const raster::Color green{0, 150, 0};

    raster::drawLineDDA(canvas, 20, 20, 230, 100, red);
    raster::drawLineBresenham(canvas, 20, 150, 230, 230, red);

    // Clip window, the original line (grey) and its clipped part (blue).
    const raster::Rect clipWindow{50.0f, 50.0f, 200.0f, 200.0f};
    raster::Polygon windowOutline;
    windowOutline.vertices = {{50, 50}, {200, 50}, {200, 200}, {50, 200}};
    raster::drawPolygonOutline(canvas, windowOutline, kBlack);

    float x0 = 0, y0 = 0, x1 = 255, y1 = 255;
    raster::drawDashedLine(canvas, 0, 0, 255, 255, 3, 3, grey);
    if (raster::clipLineCohenSutherland(x0, y0, x1, y1, clipWindow)) {
        raster::drawThickLine(canvas, raster::roundToPixel(x0), raster::roundToPixel(y0),
                              raster::roundToPixel(x1), raster::roundToPixel(y1), 3, blue);
    }

    raster::drawThickLine(canvas, 20, 240, 120, 200, 6, green);
    raster::drawPolyline(canvas, {{140, 240}, {170, 210}, {200, 240}, {230, 210}}, kBlack);

    CHECK(canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_line.ppm"));
    CHECK(canvas.saveBMP(std::string(TESTS_OUTPUT_DIR) + "/test_line.bmp"));
}

} // namespace

int main() {
    // Exhaustive: every direction with |dx|, |dy| <= 15 covers all eight
    // octants, both axes, both diagonals, exact half-pixel ties and the
    // zero-length line.
    for (int dy = -15; dy <= 15; ++dy) {
        for (int dx = -15; dx <= 15; ++dx) {
            checkLineProperties("Bresenham", raster::drawLineBresenham, 20, 20, 20 + dx, 20 + dy, 1e-9);
            // DDA accumulates a floating-point slope, so allow a tiny rounding error.
            checkLineProperties("DDA", raster::drawLineDDA, 20, 20, 20 + dx, 20 + dy, 1e-6);
        }
    }

    testCohenSutherland();
    testThickLine();
    testDashedLine();
    testPolylineAndOutline();
    testDrawLineClipped();
    writeImage();

    return test::summary("test_line");
}
