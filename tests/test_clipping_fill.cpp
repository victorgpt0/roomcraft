#include "raster/canvas.h"
#include "raster/clipping.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <cmath>
#include <string>
#include <vector>

namespace {

const raster::Color kWhite{255, 255, 255};
const raster::Color kBlack{0, 0, 0};

bool sameColor(raster::Color a, raster::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int countColor(const raster::Canvas& canvas, raster::Color color) {
    int count = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            if (sameColor(canvas.getPixel(x, y), color)) {
                ++count;
            }
        }
    }
    return count;
}

// True if `polygon` has a vertex within eps of p (vertex order and starting
// vertex are not fixed by the algorithm, so we look the point up).
bool hasVertex(const raster::Polygon& polygon, raster::Point p, float eps = 1e-3f) {
    for (const raster::Point& v : polygon.vertices) {
        if (std::fabs(v.x - p.x) <= eps && std::fabs(v.y - p.y) <= eps) {
            return true;
        }
    }
    return false;
}

std::string describe(const raster::Polygon& polygon) {
    std::string s;
    for (const raster::Point& v : polygon.vertices) {
        s += "(" + std::to_string(v.x) + "," + std::to_string(v.y) + ") ";
    }
    return s;
}

void testSutherlandHodgman() {
    const raster::Polygon window = raster::makeRectangle(64, 64, 128, 128); // 64..192

    // Big square around the window: the result is exactly the window.
    const raster::Polygon subject = raster::makeRectangle(30, 30, 190, 190); // 30..220
    const raster::Polygon clipped = raster::clipPolygonSutherlandHodgman(subject, window);
    CHECK_MSG(clipped.vertices.size() == 4, describe(clipped));
    CHECK(hasVertex(clipped, {64, 64}));
    CHECK(hasVertex(clipped, {192, 64}));
    CHECK(hasVertex(clipped, {192, 192}));
    CHECK(hasVertex(clipped, {64, 192}));
    CHECK_NEAR(raster::computeArea(clipped), 128.0 * 128.0, 1e-2);

    // Triangle sticking out of the right and bottom of a 0..100 window.
    // Hypotenuse x + y = 170 leaves through x = 100 at y = 70 and through
    // y = 100 at x = 70, cutting off a corner: a 5-sided result.
    // Area = 50 x 80 rectangle minus the 30 x 30 / 2 corner = 4000 - 450.
    const raster::Polygon box = raster::makeRectangle(0, 0, 100, 100);
    raster::Polygon triangle;
    triangle.vertices = {{50, 20}, {150, 20}, {50, 120}};
    const raster::Polygon cut = raster::clipPolygonSutherlandHodgman(triangle, box);
    CHECK_MSG(cut.vertices.size() == 5, describe(cut));
    CHECK(hasVertex(cut, {50, 20}));
    CHECK(hasVertex(cut, {100, 20}));
    CHECK(hasVertex(cut, {100, 70}));
    CHECK(hasVertex(cut, {70, 100}));
    CHECK(hasVertex(cut, {50, 100}));
    CHECK_NEAR(raster::computeArea(cut), 3550.0, 1e-2);

    // Same clip with the window listed counter-clockwise: same result.
    raster::Polygon boxReversed;
    boxReversed.vertices = {box.vertices[3], box.vertices[2], box.vertices[1], box.vertices[0]};
    const raster::Polygon cut2 = raster::clipPolygonSutherlandHodgman(triangle, boxReversed);
    CHECK(cut2.vertices.size() == 5);
    CHECK_NEAR(raster::computeArea(cut2), 3550.0, 1e-2);

    // Fully inside: unchanged. Fully outside: empty.
    const raster::Polygon inside = raster::makeRectangle(10, 10, 20, 20);
    const raster::Polygon same = raster::clipPolygonSutherlandHodgman(inside, box);
    CHECK(same.vertices.size() == 4);
    CHECK_NEAR(raster::computeArea(same), 400.0, 1e-3);
    const raster::Polygon outside = raster::makeRectangle(200, 200, 20, 20);
    CHECK(raster::clipPolygonSutherlandHodgman(outside, box).vertices.empty());

    // A non-rectangular convex window (a triangle) also works.
    raster::Polygon triWindow;
    triWindow.vertices = {{0, 0}, {100, 0}, {0, 100}};
    const raster::Polygon inTri = raster::clipPolygonSutherlandHodgman(box, triWindow);
    CHECK_NEAR(raster::computeArea(inTri), 5000.0, 1e-2);

    // Degenerate window: nothing.
    raster::Polygon flat;
    flat.vertices = {{0, 0}, {10, 0}, {20, 0}};
    CHECK(raster::clipPolygonSutherlandHodgman(subject, flat).vertices.empty());
    CHECK(raster::clipPolygonSutherlandHodgman(raster::Polygon{}, box).vertices.empty());
}

// Documented limitation: a concave U-shape cut into two separate arms comes
// back as one polygon with a zero-width bridge along the clip edge. Its
// area and fill are still right (the bridge encloses nothing).
//
//   (0,0) +--+ (10,0)   (20,0) +--+ (30,0)
//         |  |                 |  |          clip window: y 0..20
//   ......|..|.................|..|......  <- y = 20
//         |  +-----------------+  |   (10,30)-(20,30)
//         +-----------------------+   (0,40)-(30,40)
void testConcaveSubjectLimitation() {
    raster::Polygon u;
    u.vertices = {{0, 0}, {10, 0}, {10, 30}, {20, 30}, {20, 0}, {30, 0}, {30, 40}, {0, 40}};
    const raster::Polygon window = raster::makeRectangle(-5, 0, 40, 20);
    const raster::Polygon arms = raster::clipPolygonSutherlandHodgman(u, window);

    CHECK_NEAR(raster::computeArea(arms), 2 * 10 * 20, 1e-3);

    raster::Canvas canvas(64, 64);
    canvas.clear(255, 255, 255);
    raster::fillPolygonScanline(canvas, arms, kBlack);
    CHECK(countColor(canvas, kBlack) == 400);       // two 10 x 20 arms
    CHECK(sameColor(canvas.getPixel(15, 10), kWhite)); // gap between arms is empty
}

void testScanlineFill() {
    raster::Canvas canvas(128, 128);

    // Integer rectangle: exactly w * h pixels, with the half-open rule.
    canvas.clear(255, 255, 255);
    raster::fillPolygonScanline(canvas, raster::makeRectangle(10, 10, 30, 20), kBlack);
    CHECK(countColor(canvas, kBlack) == 30 * 20);
    CHECK(sameColor(canvas.getPixel(10, 10), kBlack));  // top-left corner pixel included
    CHECK(sameColor(canvas.getPixel(39, 29), kBlack));  // last pixel inside
    CHECK(sameColor(canvas.getPixel(40, 20), kWhite));  // right edge excluded
    CHECK(sameColor(canvas.getPixel(20, 30), kWhite));  // bottom edge excluded

    // Rectangle at half-pixel offsets: still exactly w * h pixels.
    canvas.clear(255, 255, 255);
    raster::fillPolygonScanline(canvas, raster::makeRectangle(10.5f, 10.5f, 30, 20), kBlack);
    CHECK(countColor(canvas, kBlack) == 30 * 20);

    // Concave L-shape (area 600 with integer corners): exactly 600 pixels,
    // and the notch stays empty.
    canvas.clear(255, 255, 255);
    raster::Polygon l;
    l.vertices = {{0, 0}, {40, 0}, {40, 10}, {10, 10}, {10, 30}, {0, 30}};
    raster::fillPolygonScanline(canvas, l, kBlack);
    CHECK(countColor(canvas, kBlack) == 600);
    CHECK(sameColor(canvas.getPixel(25, 20), kWhite));

    // Triangle: pixel count close to its area (error at most ~ perimeter).
    canvas.clear(255, 255, 255);
    raster::Polygon tri;
    tri.vertices = {{10, 10}, {110, 30}, {40, 100}}; // area 4200
    raster::fillPolygonScanline(canvas, tri, kBlack);
    CHECK_NEAR(countColor(canvas, kBlack), raster::computeArea(tri), 0.02 * 4200);
    // Every filled pixel centre is inside the polygon, every empty one outside.
    bool consistent = true;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const bool filled = sameColor(canvas.getPixel(x, y), kBlack);
            const bool inside = raster::pointInPolygon(
                tri, raster::Point{static_cast<float>(x), static_cast<float>(y)});
            if (filled != inside) {
                consistent = false;
            }
        }
    }
    CHECK(consistent);

    // Self-intersecting star (pentagram): with even-odd the centre is empty.
    canvas.clear(255, 255, 255);
    raster::Polygon star;
    for (int i = 0; i < 5; ++i) {
        const double a = (-90.0 + i * 144.0) * raster::kPi / 180.0;
        star.vertices.push_back({static_cast<float>(64 + 50 * std::cos(a)),
                                 static_cast<float>(64 + 50 * std::sin(a))});
    }
    raster::fillPolygonScanline(canvas, star, kBlack);
    CHECK(sameColor(canvas.getPixel(64, 64), kWhite)); // centre pentagon
    CHECK(sameColor(canvas.getPixel(64, 20), kBlack)); // a point of the star

    // Shape partly off the canvas: no crash, only the visible part is filled.
    canvas.clear(255, 255, 255);
    raster::fillPolygonScanline(canvas, raster::makeRectangle(-20, -20, 40, 40), kBlack);
    CHECK(countColor(canvas, kBlack) == 20 * 20);

    // Fewer than 3 vertices: nothing.
    canvas.clear(255, 255, 255);
    raster::Polygon two;
    two.vertices = {{0, 0}, {50, 50}};
    raster::fillPolygonScanline(canvas, two, kBlack);
    CHECK(countColor(canvas, kBlack) == 0);
}

// Neighbouring polygons that share edges must neither overlap nor leave a
// gap. A hexagon is split into 6 triangles around a non-integer centre; each
// triangle is filled on its own canvas. Every pixel of the whole hexagon
// must be filled by exactly one triangle.
void testSharedEdges() {
    const raster::Point centre{60.3f, 58.7f};
    std::vector<raster::Point> ring;
    for (int i = 0; i < 6; ++i) {
        const double a = (i * 60.0 + 7.0) * raster::kPi / 180.0;
        ring.push_back({static_cast<float>(centre.x + 45.2 * std::cos(a)),
                        static_cast<float>(centre.y + 45.2 * std::sin(a))});
    }

    std::vector<int> coverCount(128 * 128, 0);
    for (int i = 0; i < 6; ++i) {
        raster::Canvas piece(128, 128);
        piece.clear(255, 255, 255);
        raster::Polygon triangle;
        triangle.vertices = {centre, ring[static_cast<size_t>(i)], ring[static_cast<size_t>((i + 1) % 6)]};
        raster::fillPolygonScanline(piece, triangle, kBlack);
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
                if (sameColor(piece.getPixel(x, y), kBlack)) {
                    ++coverCount[static_cast<size_t>(y * 128 + x)];
                }
            }
        }
    }

    raster::Canvas whole(128, 128);
    whole.clear(255, 255, 255);
    raster::Polygon hexagon;
    hexagon.vertices = ring;
    raster::fillPolygonScanline(whole, hexagon, kBlack);

    int overlaps = 0;
    int gaps = 0;
    int extras = 0;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const int n = coverCount[static_cast<size_t>(y * 128 + x)];
            const bool inHexagon = sameColor(whole.getPixel(x, y), kBlack);
            if (n > 1) {
                ++overlaps;
            }
            if (inHexagon && n == 0) {
                ++gaps;
            }
            if (!inHexagon && n > 0) {
                ++extras;
            }
        }
    }
    CHECK_MSG(overlaps == 0, std::to_string(overlaps) + " pixels filled by two triangles");
    CHECK_MSG(gaps == 0, std::to_string(gaps) + " hexagon pixels filled by no triangle");
    CHECK_MSG(extras == 0, std::to_string(extras) + " triangle pixels outside the hexagon");

    // Two rectangles sharing the edge x = 50.
    raster::Canvas canvas(128, 128);
    canvas.clear(255, 255, 255);
    const raster::Color red{255, 0, 0};
    const raster::Color blue{0, 0, 255};
    raster::fillPolygonScanline(canvas, raster::makeRectangle(20, 20, 30, 40), red);  // x 20..49
    raster::fillPolygonScanline(canvas, raster::makeRectangle(50, 20, 30, 40), blue); // x 50..79
    CHECK(countColor(canvas, red) == 30 * 40);
    CHECK(countColor(canvas, blue) == 30 * 40);
}

void testBoundaryFill() {
    raster::Canvas canvas(64, 64);
    const raster::Color yellow{255, 220, 0};

    // Square outline from (10,10) to (30,30): the inside is 19 x 19 pixels.
    canvas.clear(255, 255, 255);
    raster::drawPolygonOutline(canvas, raster::makeRectangle(10, 10, 20, 20), kBlack);
    const int painted = raster::boundaryFill(canvas, 20, 20, yellow, kBlack);
    CHECK(painted == 19 * 19);
    CHECK(countColor(canvas, yellow) == 19 * 19);
    CHECK(sameColor(canvas.getPixel(5, 5), kWhite)); // outside untouched
    CHECK(countColor(canvas, kBlack) == 80);         // boundary untouched

    // Filling again from inside does nothing (already filled).
    CHECK(raster::boundaryFill(canvas, 20, 20, yellow, kBlack) == 0);

    // Seed on the boundary or off the canvas: nothing.
    CHECK(raster::boundaryFill(canvas, 10, 10, yellow, kBlack) == 0);
    CHECK(raster::boundaryFill(canvas, -1, 5, yellow, kBlack) == 0);

    // Diamond drawn with diagonal Bresenham lines: its pixels only touch at
    // corners, yet the 4-connected fill must not leak out.
    canvas.clear(255, 255, 255);
    raster::Polygon diamond;
    diamond.vertices = {{32, 4}, {60, 32}, {32, 60}, {4, 32}};
    raster::drawPolygonOutline(canvas, diamond, kBlack);
    raster::boundaryFill(canvas, 32, 32, yellow, kBlack);
    CHECK(sameColor(canvas.getPixel(0, 0), kWhite));
    CHECK(sameColor(canvas.getPixel(63, 63), kWhite));
    CHECK(sameColor(canvas.getPixel(32, 32), yellow));

    // A large area (whole 1000 x 1000 canvas) must not overflow the stack.
    raster::Canvas big(1000, 1000);
    big.clear(255, 255, 255);
    CHECK(raster::boundaryFill(big, 500, 500, yellow, kBlack) == 1000 * 1000);

    // Known limitation: a gap in the boundary lets the fill leak out.
    canvas.clear(255, 255, 255);
    raster::drawPolygonOutline(canvas, raster::makeRectangle(10, 10, 20, 20), kBlack);
    canvas.setPixel(20, 10, kWhite); // open a 1-pixel door in the top wall
    raster::boundaryFill(canvas, 20, 20, yellow, kBlack);
    CHECK(sameColor(canvas.getPixel(5, 5), yellow));
}

// Visual evidence: the original clipping test (square clipped to a window,
// then filled), plus boundary fill, a concave fill and a star.
void writeImage() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::Polygon subject = raster::makeRectangle(30, 30, 190, 190);
    raster::Polygon clipWindow = raster::makeRectangle(64, 64, 128, 128);
    raster::rotate(subject, 20.0f, raster::Point{125, 125});

    raster::Polygon clipped = raster::clipPolygonSutherlandHodgman(subject, clipWindow);
    raster::fillPolygonScanline(canvas, clipped, raster::Color{0, 128, 255});
    raster::drawPolygonOutline(canvas, subject, raster::Color{170, 170, 170});
    raster::drawPolygonOutline(canvas, clipWindow, kBlack);

    // The rotated subject still covers the whole window, so the clipped
    // result is the 128 x 128 window. The black window outline is drawn on
    // its top row and left column, leaving 127 x 127 blue pixels.
    int blue = 0;
    for (int y = 64; y < 192; ++y) {
        for (int x = 64; x < 192; ++x) {
            if (sameColor(canvas.getPixel(x, y), raster::Color{0, 128, 255})) {
                ++blue;
            }
        }
    }
    CHECK(blue == 127 * 127); // interior pixels; the black outline covers the rest

    // Boundary fill inside a triangle outline.
    raster::Polygon tri;
    tri.vertices = {{200, 200}, {250, 250}, {200, 250}};
    raster::drawPolygonOutline(canvas, tri, kBlack);
    raster::boundaryFill(canvas, 210, 240, raster::Color{255, 200, 0}, kBlack);

    // Pentagram, even-odd.
    raster::Polygon star;
    for (int i = 0; i < 5; ++i) {
        const double a = (-90.0 + i * 144.0) * raster::kPi / 180.0;
        star.vertices.push_back({static_cast<float>(30 + 25 * std::cos(a)),
                                 static_cast<float>(225 + 25 * std::sin(a))});
    }
    raster::fillPolygonScanline(canvas, star, raster::Color{200, 0, 120});

    CHECK(canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_clipping_fill.ppm"));
    CHECK(canvas.saveBMP(std::string(TESTS_OUTPUT_DIR) + "/test_clipping_fill.bmp"));
}

} // namespace

int main() {
    testSutherlandHodgman();
    testConcaveSubjectLimitation();
    testScanlineFill();
    testSharedEdges();
    testBoundaryFill();
    writeImage();
    return test::summary("test_clipping_fill");
}
