#include "raster/canvas.h"
#include "raster/line.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <cmath>
#include <string>

namespace {

raster::Polygon makeTriangle() {
    raster::Polygon triangle;
    triangle.vertices = {{50.0f, 50.0f}, {200.0f, 50.0f}, {125.0f, 200.0f}};
    return triangle;
}

// L-shaped polygon (concave), like a corner sofa:
//   (0,0) +--------------+ (40,0)
//         |              |
//  (0,10) |   (10,10) +--+ (40,10)
//         |           |
//  (0,30) +-----------+ (10,30)
// = a 40x10 bar (area 400, centroid (20,5)) plus a 10x20 leg below it
//   (area 200, centroid (5,20)). Total area 600,
//   centroid = ((400*20 + 200*5) / 600, (400*5 + 200*20) / 600) = (15, 10).
raster::Polygon makeLShape() {
    raster::Polygon shape;
    shape.vertices = {{0, 0}, {40, 0}, {40, 10}, {10, 10}, {10, 30}, {0, 30}};
    return shape;
}

void testAreaAndCentroid() {
    // Triangle with base 150 and height 150: area = 150 * 150 / 2 = 11250.
    // Centroid = average of the corners = ((50+200+125)/3, (50+50+200)/3).
    const raster::Polygon triangle = makeTriangle();
    CHECK(raster::computeArea(triangle) == 11250.0f);
    const raster::Point c = raster::computeCentroid(triangle);
    CHECK(c.x == 125.0f);
    CHECK(c.y == 100.0f);

    // Winding: right, then down-left is clockwise on screen (y down) -> positive.
    CHECK(raster::computeSignedArea(triangle) == 11250.0f);
    raster::Polygon reversed;
    reversed.vertices = {triangle.vertices[2], triangle.vertices[1], triangle.vertices[0]};
    CHECK(raster::computeSignedArea(reversed) == -11250.0f);
    CHECK(raster::computeArea(reversed) == 11250.0f);
    const raster::Point cr = raster::computeCentroid(reversed);
    CHECK(cr.x == 125.0f && cr.y == 100.0f);

    // Concave L-shape.
    const raster::Polygon l = makeLShape();
    CHECK(raster::computeArea(l) == 600.0f);
    const raster::Point cl = raster::computeCentroid(l);
    CHECK_NEAR(cl.x, 15.0, 1e-5);
    CHECK_NEAR(cl.y, 10.0, 1e-5);

    // Rectangle helper.
    const raster::Polygon rect = raster::makeRectangle(10, 20, 30, 40);
    CHECK(rect.vertices.size() == 4);
    CHECK(raster::computeArea(rect) == 1200.0f);
    const raster::Point crect = raster::computeCentroid(rect);
    CHECK(crect.x == 25.0f && crect.y == 40.0f);

    // Degenerate input.
    CHECK(raster::computeArea(raster::Polygon{}) == 0.0f);
    raster::Polygon segment;
    segment.vertices = {{0, 0}, {10, 0}, {20, 0}}; // collinear: zero area
    CHECK(raster::computeArea(segment) == 0.0f);
    const raster::Point cs = raster::computeCentroid(segment);
    CHECK(cs.x == 10.0f && cs.y == 0.0f); // falls back to vertex average
    const raster::Point ce = raster::computeCentroid(raster::Polygon{});
    CHECK(ce.x == 0.0f && ce.y == 0.0f);
}

void testTransformations() {
    // translate: every vertex moves, area unchanged, centroid moves by (dx, dy).
    raster::Polygon t = makeTriangle();
    raster::translate(t, 10.0f, -5.0f);
    CHECK(t.vertices[0].x == 60.0f && t.vertices[0].y == 45.0f);
    CHECK(raster::computeArea(t) == 11250.0f);
    const raster::Point ct = raster::computeCentroid(t);
    CHECK(ct.x == 135.0f && ct.y == 95.0f);

    // rotate 90 degrees about the origin: (10,0) -> (0,10). On screen that is
    // right -> down, i.e. clockwise, as documented.
    raster::Polygon r;
    r.vertices = {{10, 0}, {0, 10}, {-10, 0}};
    raster::rotate(r, 90.0f, raster::Point{0, 0});
    CHECK_NEAR(r.vertices[0].x, 0.0, 1e-5);
    CHECK_NEAR(r.vertices[0].y, 10.0, 1e-5);
    CHECK_NEAR(r.vertices[1].x, -10.0, 1e-5);
    CHECK_NEAR(r.vertices[1].y, 0.0, 1e-5);

    // rotate about a pivot keeps the pivot fixed and preserves area;
    // a full turn returns to the start.
    raster::Polygon rp = makeTriangle();
    rp.vertices.push_back({125.0f, 100.0f}); // a vertex exactly on the pivot
    const float areaBefore = raster::computeArea(rp);
    raster::rotate(rp, 37.0f, raster::Point{125.0f, 100.0f});
    CHECK_NEAR(rp.vertices[3].x, 125.0, 1e-4);
    CHECK_NEAR(rp.vertices[3].y, 100.0, 1e-4);
    CHECK_NEAR(raster::computeArea(rp), areaBefore, 0.05);
    raster::rotate(rp, 323.0f, raster::Point{125.0f, 100.0f});
    CHECK_NEAR(rp.vertices[0].x, 50.0, 1e-3);
    CHECK_NEAR(rp.vertices[0].y, 50.0, 1e-3);

    // scale about a pivot: pivot fixed, area multiplies by sx * sy.
    raster::Polygon s = raster::makeRectangle(10, 10, 20, 10); // area 200
    raster::scale(s, 2.0f, 3.0f, raster::Point{10, 10});
    CHECK(s.vertices[0].x == 10.0f && s.vertices[0].y == 10.0f);
    CHECK(s.vertices[2].x == 50.0f && s.vertices[2].y == 40.0f);
    CHECK(raster::computeArea(s) == 1200.0f);

    // Negative scale mirrors the shape: the winding flips, the area does not.
    raster::Polygon m = makeTriangle();
    raster::scale(m, -1.0f, 1.0f, raster::Point{125.0f, 0.0f});
    CHECK(raster::computeSignedArea(m) == -11250.0f);
}

void testBoundingBox() {
    const raster::Rect box = raster::computeBoundingBox(makeTriangle());
    CHECK(box.xmin == 50.0f && box.ymin == 50.0f && box.xmax == 200.0f && box.ymax == 200.0f);

    const raster::Rect lbox = raster::computeBoundingBox(makeLShape());
    CHECK(lbox.xmin == 0.0f && lbox.ymin == 0.0f && lbox.xmax == 40.0f && lbox.ymax == 30.0f);

    const raster::Rect empty = raster::computeBoundingBox(raster::Polygon{});
    CHECK(empty.xmin == 0.0f && empty.ymin == 0.0f && empty.xmax == 0.0f && empty.ymax == 0.0f);
}

void testPointInPolygon() {
    const raster::Polygon triangle = makeTriangle();
    CHECK(raster::pointInPolygon(triangle, {125, 100}));
    CHECK(raster::pointInPolygon(triangle, {125, 190}));
    CHECK(!raster::pointInPolygon(triangle, {10, 10}));
    CHECK(!raster::pointInPolygon(triangle, {60, 150}));  // left of the slanted edge
    CHECK(!raster::pointInPolygon(triangle, {125, 210})); // below the bottom vertex

    // Ray along y = 50 runs through two vertices joined by a horizontal edge.
    CHECK(!raster::pointInPolygon(triangle, {10, 50}));

    // Concave L-shape: the notch is outside, both arms are inside.
    const raster::Polygon l = makeLShape();
    CHECK(raster::pointInPolygon(l, {25, 5}));
    CHECK(raster::pointInPolygon(l, {5, 25}));
    CHECK(!raster::pointInPolygon(l, {25, 20}));

    // Diamond: the ray from (-10, 50) passes exactly through the left and
    // right vertices. Each vertex must be counted once, not twice.
    raster::Polygon diamond;
    diamond.vertices = {{50, 0}, {100, 50}, {50, 100}, {0, 50}};
    CHECK(!raster::pointInPolygon(diamond, {-10, 50}));
    CHECK(raster::pointInPolygon(diamond, {50, 50}));
    CHECK(!raster::pointInPolygon(diamond, {110, 50}));

    // Fewer than 3 vertices enclose nothing.
    raster::Polygon line;
    line.vertices = {{0, 0}, {10, 10}};
    CHECK(!raster::pointInPolygon(line, {5, 5}));
}

// The original sequence from this test: translate, rotate and scale a
// triangle, then check the area and centroid of the result and draw it.
void testTransformSequenceAndDraw() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    raster::Polygon triangle = makeTriangle();
    raster::drawPolygonOutline(canvas, triangle, raster::Color{170, 170, 170});

    raster::Point pivot{125.0f, 100.0f};
    raster::translate(triangle, 10.0f, 0.0f);
    raster::rotate(triangle, 15.0f, pivot);
    raster::scale(triangle, 1.1f, 1.1f, pivot);

    // Area: translate and rotate keep 11250, scaling by 1.1 x 1.1 multiplies it by 1.21.
    const float area = raster::computeArea(triangle);
    CHECK_NEAR(area, 11250.0 * 1.21, 0.1);

    // Centroid: (125,100) -> translate -> (135,100), i.e. offset (10, 0) from
    // the pivot. Rotating by 15 degrees turns that offset into
    // 10 * (cos 15, sin 15); scaling by 1.1 makes it 11 * (cos 15, sin 15).
    const raster::Point centroid = raster::computeCentroid(triangle);
    const double a = 15.0 * raster::kPi / 180.0;
    CHECK_NEAR(centroid.x, 125.0 + 11.0 * std::cos(a), 1e-3);
    CHECK_NEAR(centroid.y, 100.0 + 11.0 * std::sin(a), 1e-3);

    raster::drawPolygonOutline(canvas, triangle, raster::Color{0, 0, 255});

    // Bounding box (dashed) and centroid marker.
    const raster::Rect box = raster::computeBoundingBox(triangle);
    const int bx0 = raster::roundToPixel(box.xmin);
    const int by0 = raster::roundToPixel(box.ymin);
    const int bx1 = raster::roundToPixel(box.xmax);
    const int by1 = raster::roundToPixel(box.ymax);
    const raster::Color green{0, 150, 0};
    raster::drawDashedLine(canvas, bx0, by0, bx1, by0, 4, 3, green);
    raster::drawDashedLine(canvas, bx1, by0, bx1, by1, 4, 3, green);
    raster::drawDashedLine(canvas, bx1, by1, bx0, by1, 4, 3, green);
    raster::drawDashedLine(canvas, bx0, by1, bx0, by0, 4, 3, green);

    const int cx = raster::roundToPixel(centroid.x);
    const int cy = raster::roundToPixel(centroid.y);
    const raster::Color red{255, 0, 0};
    raster::drawLineBresenham(canvas, cx - 4, cy, cx + 4, cy, red);
    raster::drawLineBresenham(canvas, cx, cy - 4, cx, cy + 4, red);

    // The centroid of a triangle is always inside it.
    CHECK(raster::pointInPolygon(triangle, centroid));

    CHECK(canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_polygon.ppm"));
    CHECK(canvas.saveBMP(std::string(TESTS_OUTPUT_DIR) + "/test_polygon.bmp"));
}

} // namespace

int main() {
    testAreaAndCentroid();
    testTransformations();
    testBoundingBox();
    testPointInPolygon();
    testTransformSequenceAndDraw();
    return test::summary("test_polygon");
}
