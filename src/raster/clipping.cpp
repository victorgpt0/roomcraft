#include "raster/clipping.h"

#include <cmath>
#include <vector>

namespace raster {

namespace {

// Which side of the infinite line through a -> b is p on?
// This is the 2D cross product (b - a) x (p - a):
//   > 0 on one side, < 0 on the other, 0 exactly on the line.
// Multiplying by `orientation` (+1 or -1, from the clip window's winding)
// makes the result >= 0 mean "inside the window" for either winding.
double side(Point a, Point b, Point p, double orientation) {
    const double cross = (static_cast<double>(b.x) - a.x) * (static_cast<double>(p.y) - a.y) -
                         (static_cast<double>(b.y) - a.y) * (static_cast<double>(p.x) - a.x);
    return cross * orientation;
}

// Point where segment s -> e crosses the clip line, given the side values
// of its endpoints (one >= 0, one < 0). The side value changes linearly
// along the segment, so it is zero at t = sideS / (sideS - sideE).
Point intersection(Point s, Point e, double sideS, double sideE) {
    const double t = sideS / (sideS - sideE);
    return Point{static_cast<float>(s.x + t * (static_cast<double>(e.x) - s.x)),
                 static_cast<float>(s.y + t * (static_cast<double>(e.y) - s.y))};
}

bool samePoint(Point a, Point b) {
    return std::fabs(a.x - b.x) < 1e-6f && std::fabs(a.y - b.y) < 1e-6f;
}

} // namespace

// Sutherland-Hodgman: clip the subject against ONE clip edge at a time.
// After clipping against every edge of the (convex) window, only the part
// inside all edges - i.e. inside the window - is left.
//
// Clipping against one edge walks around the polygon, looking at each
// polygon edge S -> E (S = previous vertex, E = current vertex):
//
//   S inside,  E inside   -> output E
//   S inside,  E outside  -> output the intersection I   (leaving)
//   S outside, E inside   -> output I, then E             (entering)
//   S outside, E outside  -> output nothing
//
// Why the window must be convex: a convex region is exactly the
// intersection of the "inside" half-planes of its edges, which is what
// clipping one edge at a time computes. A concave window is not.
Polygon clipPolygonSutherlandHodgman(const Polygon& subject, const Polygon& clipWindow) {
    const std::vector<Point>& window = clipWindow.vertices;
    const float windowArea = computeSignedArea(clipWindow);
    if (window.size() < 3 || windowArea == 0.0f) {
        return Polygon{};
    }
    // With y down, positive signed area = clockwise on screen, and for a
    // clockwise window the inside is where side() is positive.
    const double orientation = windowArea > 0.0f ? 1.0 : -1.0;

    std::vector<Point> output = subject.vertices;

    for (size_t i = 0; i < window.size() && !output.empty(); ++i) {
        const Point a = window[i];
        const Point b = window[(i + 1) % window.size()];

        const std::vector<Point> input = output;
        output.clear();

        Point s = input.back(); // start with the closing edge (last -> first)
        double sideS = side(a, b, s, orientation);
        for (const Point& e : input) {
            const double sideE = side(a, b, e, orientation);
            if (sideE >= 0.0) {
                if (sideS < 0.0) {
                    output.push_back(intersection(s, e, sideS, sideE)); // entering
                }
                output.push_back(e);
            } else if (sideS >= 0.0) {
                output.push_back(intersection(s, e, sideS, sideE)); // leaving
            }
            s = e;
            sideS = sideE;
        }
    }

    // A vertex lying exactly on a clip edge can appear twice in a row
    // (once as "E", once as an intersection). Remove such duplicates.
    Polygon result;
    for (const Point& p : output) {
        if (result.vertices.empty() || !samePoint(result.vertices.back(), p)) {
            result.vertices.push_back(p);
        }
    }
    while (result.vertices.size() > 1 && samePoint(result.vertices.front(), result.vertices.back())) {
        result.vertices.pop_back();
    }
    if (result.vertices.size() < 3) {
        result.vertices.clear(); // a point or a line encloses nothing
    }
    return result;
}

} // namespace raster
