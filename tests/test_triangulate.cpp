// Tests for ear-clipping triangulation and the polygon overlap area.

#include "editor/catalogue.h"
#include "editor/triangulate.h"
#include "raster/polygon.h"
#include "xform/mat3.h"

#include "test_helpers.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

using raster::Point;
using raster::Polygon;

namespace {

double cross(Point a, Point b, Point c) {
    return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y) -
           (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
}

// Everything that must hold for a correct triangulation of a simple polygon:
//   - n - 2 triangles for n vertices (when none are collinear/duplicate),
//   - every index valid and the three corners different,
//   - every triangle has positive area and the polygon's winding,
//   - the triangles' areas add up to the polygon's area (so, together with
//     the next check, no gaps and no overlaps),
//   - every triangle's centroid lies inside the polygon (no triangle outside).
void checkTriangulation(const Polygon& polygon, int expectedTriangles, const std::string& name) {
    const std::vector<editor::Triangle> triangles = editor::triangulate(polygon);
    if (expectedTriangles >= 0) {
        CHECK_MSG(static_cast<int>(triangles.size()) == expectedTriangles,
                  name + ": " + std::to_string(triangles.size()) + " triangles");
    }
    const double sign = raster::computeSignedArea(polygon) > 0 ? 1.0 : -1.0;
    const int n = static_cast<int>(polygon.vertices.size());
    int bad = 0;
    for (const editor::Triangle& t : triangles) {
        if (t[0] < 0 || t[0] >= n || t[1] < 0 || t[1] >= n || t[2] < 0 || t[2] >= n || t[0] == t[1] ||
            t[1] == t[2] || t[0] == t[2]) {
            ++bad;
            continue;
        }
        const Point a = polygon.vertices[static_cast<std::size_t>(t[0])];
        const Point b = polygon.vertices[static_cast<std::size_t>(t[1])];
        const Point c = polygon.vertices[static_cast<std::size_t>(t[2])];
        if (cross(a, b, c) * sign <= 0.0) {
            ++bad; // zero area or the wrong winding
        }
        const Point centroid{(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f};
        if (!raster::pointInPolygon(polygon, centroid)) {
            ++bad;
        }
    }
    CHECK_MSG(bad == 0, name + ": " + std::to_string(bad) + " bad triangles");
    const double area = raster::computeArea(polygon);
    CHECK_MSG(std::fabs(editor::triangulatedArea(polygon, triangles) - area) <= area * 1e-4 + 1e-3,
              name + ": area mismatch");
}

Polygon reversed(Polygon p) {
    std::reverse(p.vertices.begin(), p.vertices.end());
    return p;
}

void testBasicShapes() {
    checkTriangulation(raster::makeRectangle(0, 0, 10, 10), 2, "square");
    checkTriangulation(reversed(raster::makeRectangle(0, 0, 10, 10)), 2, "square, reversed");

    Polygon triangle;
    triangle.vertices = {{0, 0}, {10, 0}, {0, 10}};
    checkTriangulation(triangle, 1, "triangle");

    // Concave L (the L-shaped sofa).
    Polygon l;
    l.vertices = {{-125, -85}, {125, -85}, {125, 85}, {55, 85}, {55, -15}, {-125, -15}};
    checkTriangulation(l, 4, "L");
    checkTriangulation(reversed(l), 4, "L, reversed");

    // Every catalogue footprint and part, including the kidney table (Bezier
    // outline, concave) and the 40-sided round table.
    for (const editor::FurnitureType& type : editor::catalogue()) {
        checkTriangulation(type.footprint, static_cast<int>(type.footprint.vertices.size()) - 2, type.id);
        for (const editor::Part& part : type.parts) {
            checkTriangulation(part.footprint, -1, type.id + " part");
        }
    }

    // A comb: many deep notches (many reflex vertices).
    Polygon comb;
    comb.vertices.push_back({0, 0});
    for (int tooth = 0; tooth < 8; ++tooth) {
        const float x = 10.0f + 20.0f * static_cast<float>(tooth);
        comb.vertices.push_back({x, 0});
        comb.vertices.push_back({x, 80});
        comb.vertices.push_back({x + 10, 80});
        comb.vertices.push_back({x + 10, 0});
    }
    comb.vertices.push_back({180, 0});
    comb.vertices.push_back({180, 100});
    comb.vertices.push_back({0, 100});
    checkTriangulation(comb, static_cast<int>(comb.vertices.size()) - 2, "comb");
    checkTriangulation(reversed(comb), static_cast<int>(comb.vertices.size()) - 2, "comb, reversed");

    // A star (alternating convex and reflex corners).
    Polygon star;
    for (int i = 0; i < 14; ++i) {
        const double angle = raster::kPi * i / 7.0;
        const double r = i % 2 == 0 ? 100.0 : 35.0;
        star.vertices.push_back({static_cast<float>(r * std::cos(angle)), static_cast<float>(r * std::sin(angle))});
    }
    checkTriangulation(star, 12, "star");

    // A spiral-ish "C" whose ears are hard to find.
    Polygon c;
    c.vertices = {{0, 0}, {100, 0}, {100, 20}, {20, 20}, {20, 80}, {100, 80}, {100, 100}, {0, 100}};
    checkTriangulation(c, 6, "C");
}

void testRotatedAndScaled() {
    // The same concave shapes after the transforms the editor applies.
    Polygon l;
    l.vertices = {{-125, -85}, {125, -85}, {125, 85}, {55, 85}, {55, -15}, {-125, -15}};
    for (int angle = 0; angle < 360; angle += 7) {
        const xform::Mat3 m = xform::Mat3::translation(300, 200) * xform::Mat3::rotation(static_cast<float>(angle)) *
                              xform::Mat3::scaling(angle % 2 == 0 ? 1.0f : -1.0f, 0.8f);
        checkTriangulation(xform::transformPolygon(m, l), 4, "transformed L " + std::to_string(angle));
    }
}

void testDegenerate() {
    // Points in the middle of straight edges: never a zero-area triangle
    // (checkTriangulation rejects those), at most n - 2 triangles, and the
    // area is exact. (A midpoint may still be used as a triangle corner.)
    Polygon withMidpoints;
    withMidpoints.vertices = {{0, 0}, {5, 0}, {10, 0}, {10, 5}, {10, 10}, {0, 10}};
    const auto t1 = editor::triangulate(withMidpoints);
    CHECK(t1.size() >= 2 && t1.size() <= 4);
    CHECK_NEAR(editor::triangulatedArea(withMidpoints, t1), 100.0, 1e-4);
    checkTriangulation(withMidpoints, -1, "midpoints");

    // Duplicate consecutive vertices and a repeated first vertex at the end.
    Polygon duplicates;
    duplicates.vertices = {{0, 0}, {0, 0}, {10, 0}, {10, 10}, {10, 10}, {0, 10}, {0, 0}};
    const auto t2 = editor::triangulate(duplicates);
    CHECK(t2.size() == 2);
    CHECK_NEAR(editor::triangulatedArea(duplicates, t2), 100.0, 1e-4);

    // Too few points, or zero area.
    CHECK(editor::triangulate(Polygon{}).empty());
    Polygon two;
    two.vertices = {{0, 0}, {5, 5}};
    CHECK(editor::triangulate(two).empty());
    Polygon line;
    line.vertices = {{0, 0}, {5, 5}, {10, 10}};
    CHECK(editor::triangulate(line).empty());

    // A self-intersecting "bow tie" is not a simple polygon. The result is
    // not meaningful, but it must return (no endless loop) with valid indices.
    Polygon bowTie;
    bowTie.vertices = {{0, 0}, {10, 10}, {10, 0}, {0, 10}};
    const auto t3 = editor::triangulate(bowTie);
    CHECK(t3.size() <= 2);
    for (const auto& t : t3) {
        CHECK(t[0] >= 0 && t[0] < 4 && t[1] >= 0 && t[1] < 4 && t[2] >= 0 && t[2] < 4);
    }
}

void testRandomPolygons() {
    // Random "star-shaped" polygons (radius varies with angle) are simple
    // but usually concave: a good stress test.
    std::mt19937 random(7);
    std::uniform_real_distribution<float> radius(20.0f, 120.0f);
    for (int round = 0; round < 300; ++round) {
        Polygon p;
        const int n = 5 + round % 40;
        for (int i = 0; i < n; ++i) {
            const double angle = 2.0 * raster::kPi * i / n;
            const float r = radius(random);
            p.vertices.push_back({static_cast<float>(r * std::cos(angle)), static_cast<float>(r * std::sin(angle))});
        }
        if (round % 2 == 1) {
            p = reversed(p);
        }
        checkTriangulation(p, -1, "random " + std::to_string(round));
    }
}

void testOverlapArea() {
    const Polygon a = raster::makeRectangle(0, 0, 100, 100);
    // Half overlapping.
    CHECK_NEAR(editor::overlapArea(a, raster::makeRectangle(50, 0, 100, 100)), 5000.0, 1e-2);
    // Corner overlap 20 x 30.
    CHECK_NEAR(editor::overlapArea(a, raster::makeRectangle(80, 70, 100, 100)), 600.0, 1e-2);
    // Contained.
    CHECK_NEAR(editor::overlapArea(a, raster::makeRectangle(10, 10, 20, 20)), 400.0, 1e-2);
    // Touching along an edge, or apart: no overlap.
    CHECK_NEAR(editor::overlapArea(a, raster::makeRectangle(100, 0, 50, 50)), 0.0, 1e-3);
    CHECK_NEAR(editor::overlapArea(a, raster::makeRectangle(300, 300, 50, 50)), 0.0, 0);
    // Symmetric.
    const Polygon b = raster::makeRectangle(-30, 40, 90, 200);
    CHECK_NEAR(editor::overlapArea(a, b), editor::overlapArea(b, a), 1e-2);

    // CONCAVE against concave: the inner corner of the L is empty floor.
    // (Sutherland-Hodgman alone cannot clip against a concave window.)
    Polygon l;
    l.vertices = {{0, 0}, {100, 0}, {100, 100}, {60, 100}, {60, 40}, {0, 40}};
    CHECK_NEAR(editor::overlapArea(l, raster::makeRectangle(0, 50, 50, 50)), 0.0, 1e-3); // in the notch
    CHECK_NEAR(editor::overlapArea(l, raster::makeRectangle(0, 0, 100, 100)), raster::computeArea(l), 1e-2);
    CHECK_NEAR(editor::overlapArea(l, raster::makeRectangle(50, 30, 20, 20)), 20.0 * 20.0 - 10.0 * 10.0, 1e-2);
    CHECK_NEAR(editor::overlapArea(l, l), raster::computeArea(l), 1e-2);

    // A circle against a square through its centre: a quarter disc.
    const Polygon disc = editor::circlePolygon({0, 0}, 50, 64);
    CHECK_NEAR(editor::overlapArea(disc, raster::makeRectangle(0, 0, 100, 100)), raster::computeArea(disc) / 4.0, 1.0);
}

} // namespace

int main() {
    testBasicShapes();
    testRotatedAndScaled();
    testDegenerate();
    testRandomPolygons();
    testOverlapArea();
    return test::summary("test_triangulate");
}
