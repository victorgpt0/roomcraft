#include "editor/triangulate.h"

#include "raster/clipping.h"

#include <cmath>

namespace editor {

namespace {

// Twice the signed area of triangle (a, b, c): the 2D cross product of
// (b - a) and (c - a). Its SIGN says which way the corner at b turns; it is 0
// when the three points are on one line. Computed in double so that nearly
// straight corners are classified reliably.
double cross(raster::Point a, raster::Point b, raster::Point c) {
    return (static_cast<double>(b.x) - a.x) * (static_cast<double>(c.y) - a.y) -
           (static_cast<double>(b.y) - a.y) * (static_cast<double>(c.x) - a.x);
}

// Is p inside triangle (a, b, c) or on its border? `sign` is +1 or -1, the
// winding of the triangle. p is inside when it is on the inner side of all
// three edges. Points ON an edge count as inside: a vertex touching the ear
// would otherwise let the cut pass through it.
bool insideOrOnTriangle(raster::Point p, raster::Point a, raster::Point b, raster::Point c, double sign) {
    constexpr double kEpsilon = 1e-9;
    return cross(a, b, p) * sign >= -kEpsilon && cross(b, c, p) * sign >= -kEpsilon &&
           cross(c, a, p) * sign >= -kEpsilon;
}

bool samePoint(raster::Point a, raster::Point b) {
    return a.x == b.x && a.y == b.y;
}

} // namespace

std::vector<Triangle> triangulate(const raster::Polygon& polygon) {
    const std::vector<raster::Point>& v = polygon.vertices;
    std::vector<Triangle> triangles;

    // Working list of vertex indices still in the polygon, without
    // consecutive duplicates (including last == first).
    std::vector<int> remaining;
    for (int i = 0; i < static_cast<int>(v.size()); ++i) {
        if (remaining.empty() || !samePoint(v[static_cast<std::size_t>(remaining.back())], v[static_cast<std::size_t>(i)])) {
            remaining.push_back(i);
        }
    }
    while (remaining.size() > 1 &&
           samePoint(v[static_cast<std::size_t>(remaining.front())], v[static_cast<std::size_t>(remaining.back())])) {
        remaining.pop_back();
    }
    if (remaining.size() < 3) {
        return triangles;
    }

    // Winding of the whole polygon: the sign every convex corner must share.
    // (computeSignedArea is positive for clockwise-on-screen polygons.)
    const float signedArea = raster::computeSignedArea(polygon);
    if (signedArea == 0.0f) {
        return triangles;
    }
    const double sign = signedArea > 0.0f ? 1.0 : -1.0;

    const auto point = [&v](int index) { return v[static_cast<std::size_t>(index)]; };

    // Remove one ear per pass. `guard` counts corners tried since the last
    // cut: if it reaches the number of vertices, no ear exists, which only
    // happens for an invalid (self-intersecting) or numerically degenerate
    // input, and we stop instead of looping forever.
    std::size_t i = 0; // position in `remaining` of the corner being tried
    std::size_t guard = 0;
    while (remaining.size() > 3) {
        const std::size_t n = remaining.size();
        i %= n;
        const int prev = remaining[(i + n - 1) % n];
        const int curr = remaining[i];
        const int next = remaining[(i + 1) % n];
        const raster::Point a = point(prev);
        const raster::Point b = point(curr);
        const raster::Point c = point(next);
        const double turn = cross(a, b, c) * sign;

        if (turn == 0.0) {
            // b lies on the straight line from a to c: it adds no area.
            // Drop it without emitting a triangle.
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            i = (i + remaining.size() - 1) % remaining.size(); // re-check the previous corner
            guard = 0;
            continue;
        }

        bool isEar = turn > 0.0; // condition 1: convex corner
        if (isEar) {
            // Condition 2: no other remaining vertex inside the triangle.
            // Only REFLEX vertices can be inside, but testing all is simpler
            // and the polygons here are small.
            for (const int other : remaining) {
                if (other == prev || other == curr || other == next) {
                    continue;
                }
                const raster::Point p = point(other);
                if (samePoint(p, a) || samePoint(p, b) || samePoint(p, c)) {
                    continue;
                }
                if (insideOrOnTriangle(p, a, b, c, sign)) {
                    isEar = false;
                    break;
                }
            }
        }

        if (isEar) {
            triangles.push_back(Triangle{prev, curr, next});
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
            guard = 0;
            // The corners on both sides of the cut have new neighbours, so
            // their ear status may have changed: try the previous one next.
            i = (i + remaining.size() - 1) % remaining.size();
        } else {
            i = (i + 1) % n;
            if (++guard > n) {
                break; // no ear found: not a simple polygon
            }
        }
    }

    if (remaining.size() == 3) {
        const raster::Point a = point(remaining[0]);
        const raster::Point b = point(remaining[1]);
        const raster::Point c = point(remaining[2]);
        if (cross(a, b, c) * sign > 0.0) {
            triangles.push_back(Triangle{remaining[0], remaining[1], remaining[2]});
        }
    }
    return triangles;
}

float triangulatedArea(const raster::Polygon& polygon, const std::vector<Triangle>& triangles) {
    double total = 0.0;
    for (const Triangle& t : triangles) {
        total += std::fabs(cross(polygon.vertices[static_cast<std::size_t>(t[0])],
                                 polygon.vertices[static_cast<std::size_t>(t[1])],
                                 polygon.vertices[static_cast<std::size_t>(t[2])])) / 2.0;
    }
    return static_cast<float>(total);
}

float overlapArea(const raster::Polygon& a, const raster::Polygon& b) {
    // Quick rejection: if the bounding boxes do not overlap, nothing does.
    const raster::Rect boxA = raster::computeBoundingBox(a);
    const raster::Rect boxB = raster::computeBoundingBox(b);
    if (boxA.xmax <= boxB.xmin || boxB.xmax <= boxA.xmin || boxA.ymax <= boxB.ymin || boxB.ymax <= boxA.ymin) {
        return 0.0f;
    }

    const std::vector<Triangle> trianglesA = triangulate(a);
    const std::vector<Triangle> trianglesB = triangulate(b);

    const auto toPolygon = [](const raster::Polygon& p, const Triangle& t) {
        raster::Polygon triangle;
        triangle.vertices = {p.vertices[static_cast<std::size_t>(t[0])], p.vertices[static_cast<std::size_t>(t[1])],
                             p.vertices[static_cast<std::size_t>(t[2])]};
        return triangle;
    };

    std::vector<raster::Polygon> piecesB;
    std::vector<raster::Rect> boxesB;
    for (const Triangle& t : trianglesB) {
        piecesB.push_back(toPolygon(b, t));
        boxesB.push_back(raster::computeBoundingBox(piecesB.back()));
    }

    double total = 0.0;
    for (const Triangle& t : trianglesA) {
        const raster::Polygon pieceA = toPolygon(a, t);
        const raster::Rect boxPiece = raster::computeBoundingBox(pieceA);
        for (std::size_t j = 0; j < piecesB.size(); ++j) {
            const raster::Rect& box = boxesB[j];
            if (boxPiece.xmax <= box.xmin || box.xmax <= boxPiece.xmin || boxPiece.ymax <= box.ymin ||
                box.ymax <= boxPiece.ymin) {
                continue;
            }
            total += raster::computeArea(raster::clipPolygonSutherlandHodgman(pieceA, piecesB[j]));
        }
    }
    return static_cast<float>(total);
}

} // namespace editor
