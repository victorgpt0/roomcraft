#include "raster/canvas.h"
#include "raster/curves.h"
#include "raster/line.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

const raster::Color kBlack{0, 0, 0};

// The control points from the original test.
const raster::Point p0{20.0f, 220.0f};
const raster::Point p1{80.0f, 20.0f};
const raster::Point p2{180.0f, 20.0f};
const raster::Point p3{240.0f, 220.0f};

bool isBlack(const raster::Canvas& canvas, int x, int y) {
    raster::Color c = canvas.getPixel(x, y);
    return c.r == 0 && c.g == 0 && c.b == 0;
}

double distance(raster::Point a, raster::Point b) {
    return std::hypot(static_cast<double>(a.x) - b.x, static_cast<double>(a.y) - b.y);
}

bool insideBox(raster::Point p, const raster::Rect& box, double eps) {
    return p.x >= box.xmin - eps && p.x <= box.xmax + eps && p.y >= box.ymin - eps && p.y <= box.ymax + eps;
}

void testBezier() {
    // End points match the first and last control points exactly.
    const raster::Point start = raster::evaluateBezierCubic(p0, p1, p2, p3, 0.0f);
    const raster::Point end = raster::evaluateBezierCubic(p0, p1, p2, p3, 1.0f);
    CHECK(start.x == p0.x && start.y == p0.y);
    CHECK(end.x == p3.x && end.y == p3.y);
    const raster::Point startC = raster::evaluateBezierDeCasteljau(p0, p1, p2, p3, 0.0f);
    const raster::Point endC = raster::evaluateBezierDeCasteljau(p0, p1, p2, p3, 1.0f);
    CHECK(startC.x == p0.x && startC.y == p0.y);
    CHECK(endC.x == p3.x && endC.y == p3.y);

    // Known value: B(1/2) = (p0 + 3 p1 + 3 p2 + p3) / 8 = (130, 70).
    const raster::Point mid = raster::evaluateBezierCubic(p0, p1, p2, p3, 0.5f);
    CHECK_NEAR(mid.x, 130.0, 1e-4);
    CHECK_NEAR(mid.y, 70.0, 1e-4);

    // Bernstein and de Casteljau agree everywhere on [0, 1], and every point
    // is inside the bounding box of the control points (convex hull property).
    raster::Polygon control;
    control.vertices = {p0, p1, p2, p3};
    const raster::Rect hull = raster::computeBoundingBox(control);
    double maxDifference = 0.0;
    bool contained = true;
    for (int i = 0; i <= 1000; ++i) {
        const float t = static_cast<float>(i) / 1000.0f;
        const raster::Point a = raster::evaluateBezierCubic(p0, p1, p2, p3, t);
        const raster::Point b = raster::evaluateBezierDeCasteljau(p0, p1, p2, p3, t);
        maxDifference = std::max(maxDifference, distance(a, b));
        contained = contained && insideBox(a, hull, 1e-4);
    }
    CHECK_MSG(maxDifference < 1e-3, "max difference " + std::to_string(maxDifference));
    CHECK(contained);

    // Same agreement for a second, asymmetric curve with negative coordinates.
    const raster::Point q0{-50.5f, 10.0f}, q1{300.0f, -80.25f}, q2{-20.0f, 400.0f}, q3{90.0f, 33.0f};
    maxDifference = 0.0;
    for (int i = 0; i <= 1000; ++i) {
        const float t = static_cast<float>(i) / 1000.0f;
        maxDifference = std::max(maxDifference,
                                 distance(raster::evaluateBezierCubic(q0, q1, q2, q3, t),
                                          raster::evaluateBezierDeCasteljau(q0, q1, q2, q3, t)));
    }
    CHECK_MSG(maxDifference < 1e-3, "max difference " + std::to_string(maxDifference));

    // Tangent at the start points towards p1: B'(0) = 3 (p1 - p0).
    const float h = 1e-3f;
    const raster::Point near0 = raster::evaluateBezierCubic(p0, p1, p2, p3, h);
    CHECK_NEAR((near0.x - p0.x) / h, 3.0 * (p1.x - p0.x), 1.0);
    CHECK_NEAR((near0.y - p0.y) / h, 3.0 * (p1.y - p0.y), 1.0);

    // Control points on a straight line give a straight curve.
    const raster::Point l0{0, 0}, l1{10, 5}, l2{30, 15}, l3{40, 20};
    bool straight = true;
    for (int i = 0; i <= 50; ++i) {
        const raster::Point p = raster::evaluateBezierCubic(l0, l1, l2, l3, static_cast<float>(i) / 50.0f);
        straight = straight && std::fabs(p.y - p.x / 2.0f) < 1e-4f;
    }
    CHECK(straight);

    // Sampling: segments + 1 points, first and last exact.
    const std::vector<raster::Point> samples = raster::sampleBezierCubic(p0, p1, p2, p3, 64);
    CHECK(samples.size() == 65);
    CHECK(samples.front().x == p0.x && samples.front().y == p0.y);
    CHECK(samples.back().x == p3.x && samples.back().y == p3.y);
    CHECK(raster::sampleBezierCubic(p0, p1, p2, p3, 0).size() == 2); // clamped to 1 segment

    // Drawing: both end pixels and the t = 1/2 sample (130, 70) are set.
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);
    raster::drawBezierCurve(canvas, p0, p1, p2, p3, kBlack, 64);
    CHECK(isBlack(canvas, 20, 220));
    CHECK(isBlack(canvas, 240, 220));
    CHECK(isBlack(canvas, 130, 70));
    CHECK(!isBlack(canvas, 130, 120)); // below the arch: empty
}

void testCatmullRom() {
    // Single segment: from p1 (t = 0) to p2 (t = 1).
    const raster::Point a = raster::catmullRom(p0, p1, p2, p3, 0.0f);
    const raster::Point b = raster::catmullRom(p0, p1, p2, p3, 1.0f);
    CHECK(a.x == p1.x && a.y == p1.y);
    CHECK_NEAR(b.x, p2.x, 1e-4);
    CHECK_NEAR(b.y, p2.y, 1e-4);

    // Tangent at p1 is (p2 - p0) / 2.
    const float h = 1e-3f;
    const raster::Point n = raster::catmullRom(p0, p1, p2, p3, h);
    CHECK_NEAR((n.x - p1.x) / h, (p2.x - p0.x) / 2.0, 0.5);
    CHECK_NEAR((n.y - p1.y) / h, (p2.y - p0.y) / 2.0, 0.5);

    // Spline through N points passes through EVERY point: sample number
    // k * samplesPerSegment is points[k].
    const std::vector<raster::Point> points = {{20, 200}, {60, 40}, {120, 160}, {170, 30}, {230, 120}, {200, 230}};
    const int perSegment = 16;
    const std::vector<raster::Point> spline = raster::catmullRomSpline(points, perSegment);
    CHECK(spline.size() == (points.size() - 1) * perSegment + 1);
    bool throughAll = true;
    for (size_t k = 0; k < points.size(); ++k) {
        const raster::Point s = spline[k * static_cast<size_t>(perSegment)];
        if (distance(s, points[k]) > 1e-3) {
            throughAll = false;
            std::fprintf(stderr, "  spline misses point %zu\n", k);
        }
    }
    CHECK(throughAll);

    // Smooth (C1) at the joints: the derivative at the end of one segment
    // equals the derivative at the start of the next, and both equal
    // (next point - previous point) / 2.
    // Segment k-1 runs points[k-1] -> points[k], segment k runs points[k] -> points[k+1].
    // Derivatives are estimated with the 4-point one-sided difference
    //   f'(a) = (-11 f(a) + 18 f(a+h) - 9 f(a+2h) + 2 f(a+3h)) / (6h)
    // which is EXACT for cubic polynomials (each segment is a cubic), so the
    // only error left is float rounding. The simple (f(a+h) - f(a)) / h would
    // be off by about f''(a) h / 2, and f'' jumps at Catmull-Rom joints.
    const float dh = 0.05f;
    auto oneSided = [](double f0, double f1, double f2, double f3, double step) {
        return (-11.0 * f0 + 18.0 * f1 - 9.0 * f2 + 2.0 * f3) / (6.0 * step);
    };
    for (size_t k = 1; k + 1 < points.size(); ++k) {
        const raster::Point q0 = points[k == 1 ? 0 : k - 2], q1 = points[k - 1], q2 = points[k],
                            q3 = points[k + 1];
        const raster::Point r0 = points[k - 1], r1 = points[k], r2 = points[k + 1],
                            r3 = points[std::min(k + 2, points.size() - 1)];
        // Going backwards from t = 1 on the incoming segment (step -dh) ...
        raster::Point e[4];
        raster::Point s[4];
        for (int m = 0; m < 4; ++m) {
            e[m] = raster::catmullRom(q0, q1, q2, q3, 1.0f - static_cast<float>(m) * dh);
            s[m] = raster::catmullRom(r0, r1, r2, r3, static_cast<float>(m) * dh);
        }
        // ... gives -f'(1), hence the minus sign.
        const double inX = -oneSided(e[0].x, e[1].x, e[2].x, e[3].x, dh);
        const double inY = -oneSided(e[0].y, e[1].y, e[2].y, e[3].y, dh);
        const double outX = oneSided(s[0].x, s[1].x, s[2].x, s[3].x, dh);
        const double outY = oneSided(s[0].y, s[1].y, s[2].y, s[3].y, dh);
        const double tangentX = (points[k + 1].x - points[k - 1].x) / 2.0;
        const double tangentY = (points[k + 1].y - points[k - 1].y) / 2.0;
        CHECK_NEAR(inX, tangentX, 0.05);
        CHECK_NEAR(inY, tangentY, 0.05);
        CHECK_NEAR(outX, tangentX, 0.05);
        CHECK_NEAR(outY, tangentY, 0.05);
    }

    // Global-t evaluation: t = k / (N - 1) is points[k]; t is clamped.
    for (size_t k = 0; k < points.size(); ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(points.size() - 1);
        CHECK(distance(raster::evaluateCatmullRomSpline(points, t), points[k]) < 1e-3);
    }
    const raster::Point before0 = raster::evaluateCatmullRomSpline(points, -1.0f);
    const raster::Point after1 = raster::evaluateCatmullRomSpline(points, 2.0f);
    CHECK(before0.x == points.front().x && before0.y == points.front().y);
    CHECK(after1.x == points.back().x && after1.y == points.back().y);
    // The original test call: 4 points, t = 0.5 is the middle of the middle segment.
    const std::vector<raster::Point> four = {p0, p1, p2, p3};
    const raster::Point middle = raster::evaluateCatmullRomSpline(four, 0.5f);
    const raster::Point direct = raster::catmullRom(p0, p1, p2, p3, 0.5f);
    CHECK(distance(middle, direct) < 1e-4);

    // Equally spaced points on a line give a straight spline.
    const std::vector<raster::Point> line = {{0, 0}, {10, 20}, {20, 40}, {30, 60}};
    bool straight = true;
    for (const raster::Point& p : raster::catmullRomSpline(line, 10)) {
        straight = straight && std::fabs(p.y - 2.0f * p.x) < 1e-3f;
    }
    CHECK(straight);

    // Small inputs.
    CHECK(raster::catmullRomSpline({}, 8).empty());
    CHECK(raster::catmullRomSpline({{5, 5}}, 8).size() == 1);
    const std::vector<raster::Point> two = raster::catmullRomSpline({{0, 0}, {40, 20}}, 8);
    CHECK(two.size() == 9);
    bool twoStraight = true;
    for (const raster::Point& p : two) {
        twoStraight = twoStraight && std::fabs(p.y - p.x / 2.0f) < 1e-3f;
    }
    CHECK(twoStraight);
    const raster::Point e = raster::evaluateCatmullRomSpline({}, 0.5f);
    CHECK(e.x == 0.0f && e.y == 0.0f);

    // Drawing passes through every point's pixel.
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);
    raster::drawCatmullRomSpline(canvas, points, perSegment, kBlack);
    bool drawn = true;
    for (const raster::Point& p : points) {
        drawn = drawn && isBlack(canvas, raster::roundToPixel(p.x), raster::roundToPixel(p.y));
    }
    CHECK(drawn);
}

void testBSpline() {
    const std::vector<raster::Point> points = {{20, 200}, {60, 40}, {120, 160}, {170, 30}, {230, 120}, {200, 230}};
    const int perSegment = 20;
    const std::vector<raster::Point> curve = raster::uniformBSpline(points, perSegment);
    CHECK(curve.size() == (points.size() - 3) * perSegment + 1);

    // Segment i starts at (P_i + 4 P_i+1 + P_i+2) / 6.
    for (size_t i = 0; i + 3 < points.size(); ++i) {
        const raster::Point s = curve[i * static_cast<size_t>(perSegment)];
        CHECK_NEAR(s.x, (points[i].x + 4.0 * points[i + 1].x + points[i + 2].x) / 6.0, 1e-3);
        CHECK_NEAR(s.y, (points[i].y + 4.0 * points[i + 1].y + points[i + 2].y) / 6.0, 1e-3);
    }
    // ... so it does NOT pass through the control points (approximating).
    CHECK(distance(curve[0], points[1]) > 1.0);

    // Every sample of segment i stays inside the box of its 4 control points.
    bool contained = true;
    for (size_t i = 0; i + 3 < points.size(); ++i) {
        raster::Polygon local;
        local.vertices = {points[i], points[i + 1], points[i + 2], points[i + 3]};
        const raster::Rect box = raster::computeBoundingBox(local);
        for (int s = 0; s <= perSegment; ++s) {
            contained = contained && insideBox(curve[i * static_cast<size_t>(perSegment) + static_cast<size_t>(s)], box, 1e-3);
        }
    }
    CHECK(contained);

    // Smooth joints (first derivative continuous): with very fine sampling,
    // the step into each joint sample equals the step out of it. Steps here
    // are ~0.1 px; a corner would make them differ by a similar amount,
    // while smooth curvature only changes them by ~f'' dt^2 ~ 1e-4.
    const int fine = 2000;
    const std::vector<raster::Point> dense = raster::uniformBSpline(points, fine);
    for (size_t i = 1; i + 3 < points.size(); ++i) {
        const size_t j = i * static_cast<size_t>(fine);
        const double inX = dense[j].x - dense[j - 1].x, outX = dense[j + 1].x - dense[j].x;
        const double inY = dense[j].y - dense[j - 1].y, outY = dense[j + 1].y - dense[j].y;
        CHECK_NEAR(inX, outX, 2e-3);
        CHECK_NEAR(inY, outY, 2e-3);
    }

    // Four identical control points: the curve is that point.
    const std::vector<raster::Point> same(4, raster::Point{7, 9});
    bool collapsed = true;
    for (const raster::Point& p : raster::uniformBSpline(same, 5)) {
        collapsed = collapsed && std::fabs(p.x - 7.0f) < 1e-4f && std::fabs(p.y - 9.0f) < 1e-4f;
    }
    CHECK(collapsed);

    CHECK(raster::uniformBSpline({{0, 0}, {1, 1}, {2, 2}}, 5).empty());
}

// Visual evidence: the original Bezier, its control polygon, and the
// Catmull-Rom and B-spline curves for the same points.
void writeImage() {
    raster::Canvas canvas(256, 256);
    canvas.clear(255, 255, 255);

    const raster::Color grey{170, 170, 170};
    raster::drawDashedLine(canvas, 20, 220, 80, 20, 3, 3, grey);
    raster::drawDashedLine(canvas, 80, 20, 180, 20, 3, 3, grey);
    raster::drawDashedLine(canvas, 180, 20, 240, 220, 3, 3, grey);
    raster::drawBezierCurve(canvas, p0, p1, p2, p3, raster::Color{255, 128, 0}, 64);

    const std::vector<raster::Point> points = {{20, 150}, {70, 90}, {120, 200}, {170, 110}, {235, 180}};
    raster::drawCatmullRomSpline(canvas, points, 24, raster::Color{0, 0, 255});
    std::vector<raster::Point> bspline = raster::uniformBSpline(points, 24);
    raster::drawPolyline(canvas, bspline, raster::Color{0, 160, 0});
    for (const raster::Point& p : points) {
        const int x = raster::roundToPixel(p.x);
        const int y = raster::roundToPixel(p.y);
        raster::drawLineBresenham(canvas, x - 2, y - 2, x + 2, y + 2, kBlack);
        raster::drawLineBresenham(canvas, x - 2, y + 2, x + 2, y - 2, kBlack);
    }

    CHECK(canvas.savePPM(std::string(TESTS_OUTPUT_DIR) + "/test_curves.ppm"));
    CHECK(canvas.saveBMP(std::string(TESTS_OUTPUT_DIR) + "/test_curves.bmp"));
}

} // namespace

int main() {
    testBezier();
    testCatmullRom();
    testBSpline();
    writeImage();
    return test::summary("test_curves");
}
