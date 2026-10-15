#include "raster/polygon.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace raster {

Polygon makeRectangle(float x, float y, float width, float height) {
    Polygon rectangle;
    rectangle.vertices = {
        {x, y},
        {x + width, y},
        {x + width, y + height},
        {x, y + height},
    };
    return rectangle;
}

void translate(Polygon& polygon, float dx, float dy) {
    for (Point& v : polygon.vertices) {
        v.x += dx;
        v.y += dy;
    }
}

// Rotation about a pivot = three steps:
//   1. translate so the pivot is at the origin:   (dx, dy) = v - pivot
//   2. rotate around the origin:
//        x' = dx * cos(a) - dy * sin(a)
//        y' = dx * sin(a) + dy * cos(a)
//   3. translate back:                             v' = (x', y') + pivot
// This is the usual rotation matrix. In maths (y up) a positive angle turns
// counter-clockwise, but our y axis points down, which mirrors the picture,
// so on screen a positive angle turns clockwise.
void rotate(Polygon& polygon, float angleDegrees, Point pivot) {
    const double radians = angleDegrees * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);

    for (Point& v : polygon.vertices) {
        const double dx = v.x - pivot.x;
        const double dy = v.y - pivot.y;
        v.x = static_cast<float>(pivot.x + dx * c - dy * s);
        v.y = static_cast<float>(pivot.y + dx * s + dy * c);
    }
}

// Scaling about a pivot: move the pivot to the origin, multiply, move back.
//   v' = pivot + (v - pivot) * (sx, sy)
void scale(Polygon& polygon, float sx, float sy, Point pivot) {
    for (Point& v : polygon.vertices) {
        v.x = pivot.x + (v.x - pivot.x) * sx;
        v.y = pivot.y + (v.y - pivot.y) * sy;
    }
}

// Shoelace formula:
//   A = 1/2 * sum over edges (i -> j = i+1) of (x_i * y_j - x_j * y_i)
// Each term is twice the signed area of the triangle (origin, v_i, v_j).
// Adding these triangles around the polygon, the parts outside the polygon
// cancel out (they are counted once with + and once with -) and only the
// enclosed area remains. The sign tells the winding direction.
//
// Sums use double so that large coordinates do not lose precision.
float computeSignedArea(const Polygon& polygon) {
    const std::vector<Point>& v = polygon.vertices;
    if (v.size() < 3) {
        return 0.0f;
    }
    double twiceArea = 0.0;
    for (size_t i = 0; i < v.size(); ++i) {
        const Point& a = v[i];
        const Point& b = v[(i + 1) % v.size()]; // wraps around: closing edge
        twiceArea += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    return static_cast<float>(twiceArea / 2.0);
}

float computeArea(const Polygon& polygon) {
    return std::fabs(computeSignedArea(polygon));
}

// Area centroid. Split the polygon into the same triangles as the shoelace
// formula. The centroid of a triangle is the average of its 3 corners, so
// the centroid of the whole polygon is the area-weighted average of the
// triangle centroids:
//   Cx = 1 / (6A) * sum (x_i + x_j) * (x_i * y_j - x_j * y_i)
//   Cy = 1 / (6A) * sum (y_i + y_j) * (x_i * y_j - x_j * y_i)
// using the SIGNED area A, so the formula works for either winding.
Point computeCentroid(const Polygon& polygon) {
    const std::vector<Point>& v = polygon.vertices;
    if (v.empty()) {
        return Point{};
    }

    double twiceArea = 0.0;
    double sumX = 0.0;
    double sumY = 0.0;
    for (size_t i = 0; i < v.size(); ++i) {
        const Point& a = v[i];
        const Point& b = v[(i + 1) % v.size()];
        const double cross = static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
        twiceArea += cross;
        sumX += (static_cast<double>(a.x) + b.x) * cross;
        sumY += (static_cast<double>(a.y) + b.y) * cross;
    }

    if (v.size() < 3 || twiceArea == 0.0) {
        // Zero area: the formula would divide by zero. Use the vertex average.
        double avgX = 0.0;
        double avgY = 0.0;
        for (const Point& p : v) {
            avgX += p.x;
            avgY += p.y;
        }
        const double n = static_cast<double>(v.size());
        return Point{static_cast<float>(avgX / n), static_cast<float>(avgY / n)};
    }

    // 6A = 3 * (2A)
    const double sixArea = 3.0 * twiceArea;
    return Point{static_cast<float>(sumX / sixArea), static_cast<float>(sumY / sixArea)};
}

Rect computeBoundingBox(const Polygon& polygon) {
    const std::vector<Point>& v = polygon.vertices;
    if (v.empty()) {
        return Rect{};
    }
    Rect box{v[0].x, v[0].y, v[0].x, v[0].y};
    for (const Point& p : v) {
        box.xmin = std::min(box.xmin, p.x);
        box.ymin = std::min(box.ymin, p.y);
        box.xmax = std::max(box.xmax, p.x);
        box.ymax = std::max(box.ymax, p.y);
    }
    return box;
}

// Even-odd rule: cast a ray from p towards +x (to the right) and count how
// many polygon edges it crosses. Each crossing switches between outside and
// inside, so an ODD count means p is inside. This works for concave shapes
// too, e.g. a point in the notch of an L-shaped sofa crosses an even number
// of edges and is correctly reported outside.
//
// Edge rule: an edge counts only if one end has y > p.y and the other has
// y <= p.y, i.e. (a.y > p.y) != (b.y > p.y). This half-open rule
// means a ray passing exactly through a vertex is counted once, not twice,
// and horizontal edges are skipped (they can never be crossed by a
// horizontal ray). Points exactly on an edge can go either way.
bool pointInPolygon(const Polygon& polygon, Point p) {
    const std::vector<Point>& v = polygon.vertices;
    if (v.size() < 3) {
        return false;
    }

    bool inside = false;
    for (size_t i = 0; i < v.size(); ++i) {
        Point a = v[i];
        Point b = v[(i + 1) % v.size()];
        if ((a.y > p.y) != (b.y > p.y)) {
            // x where the edge crosses the horizontal line y = p.y.
            // b.y != a.y here, because exactly one of them is above p.y.
            // Always compute from the top endpoint, in the same way as the
            // scanline fill (fill.cpp), so both give identical results on
            // ties and an edge shared by two polygons gives the same x.
            if (a.y > b.y) {
                std::swap(a, b);
            }
            const double crossX = a.x + (static_cast<double>(p.y) - a.y) *
                                            (static_cast<double>(b.x) - a.x) /
                                            (static_cast<double>(b.y) - a.y);
            if (p.x < crossX) {
                inside = !inside;
            }
        }
    }
    return inside;
}

} // namespace raster
