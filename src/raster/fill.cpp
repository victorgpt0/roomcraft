#include "raster/fill.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace raster {

namespace {

// One non-horizontal polygon edge, prepared for the scanline loop.
struct Edge {
    int firstRow;  // first scanline (pixel row) the edge crosses
    int endRow;    // one past the last scanline it crosses
    Point top;     // endpoint with the smaller y
    Point bottom;  // endpoint with the larger y
    double x;      // x where the edge crosses the CURRENT scanline
};

// x where the edge crosses the horizontal line at height y (line equation).
// pointInPolygon() in polygon.cpp uses exactly the same expression, so the
// two always agree about which pixel centres are inside.
double edgeXAtY(Point top, Point bottom, double y) {
    return top.x + (y - top.y) * (static_cast<double>(bottom.x) - top.x) /
                       (static_cast<double>(bottom.y) - top.y);
}

bool sameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int ceilToInt(double v) {
    return static_cast<int>(std::ceil(v));
}

} // namespace

// Scanline fill with an Edge Table (ET) and an Active Edge Table (AET).
//
// Idea: go down the canvas one pixel row (scanline) at a time. On each
// scanline, find where it crosses the polygon's edges and sort those x
// values. With the even-odd rule, the inside parts are between crossings
// 1-2, 3-4, 5-6, ... so we fill those spans.
//
// 1. Edge table: for each non-horizontal edge, store the rows it crosses
//    and its x on the first of those rows. Horizontal edges are skipped:
//    a scanline never crosses one, and the edges next to them already
//    mark where the span starts and stops.
// 2. For each row, top to bottom:
//      - add edges that start on this row to the active edge table,
//      - remove edges that have ended,
//      - work out each active edge's x on this row,
//      - sort the active edges by x and fill between pairs.
//
// Note on updating x: the textbook version adds 1/slope to x on every row.
// We recompute x from the line equation instead. Repeated float additions
// slowly drift, and when an edge crosses exactly through a pixel centre the
// drift can push the pixel to the wrong side (e.g. a slope of 1/3 is not
// exact in binary). Recomputing costs one multiply and division per edge
// per row and is always exact to the nearest float.
//
// Half-open rule (what makes neighbouring polygons fit exactly):
//   - Rows: an edge from y = ya to y = yb (ya < yb) covers the rows y with
//     ya <= y < yb, i.e. rows ceil(ya) .. ceil(yb) - 1. The top end counts,
//     the bottom end does not, so a vertex where one edge ends and the next
//     begins is counted once, and a shared horizontal boundary belongs to
//     only one of the two polygons.
//   - Columns: a span between crossings xl and xr fills the pixels x with
//     xl <= x < xr, i.e. ceil(xl) .. ceil(xr) - 1. A shared slanted or
//     vertical edge gives the same crossing x in both polygons, so the
//     left polygon stops just before the pixel where the right one starts.
void fillPolygonScanline(Canvas& canvas, const Polygon& polygon, Color color) {
    const std::vector<Point>& v = polygon.vertices;
    if (v.size() < 3) {
        return;
    }

    // 1. Build the edge table.
    std::vector<Edge> edges;
    for (size_t i = 0; i < v.size(); ++i) {
        Point top = v[i];
        Point bottom = v[(i + 1) % v.size()];
        if (top.y == bottom.y) {
            continue; // horizontal edge
        }
        if (top.y > bottom.y) {
            std::swap(top, bottom); // always walk the edge downwards
        }
        const int firstRow = ceilToInt(top.y);
        const int endRow = ceilToInt(bottom.y);
        if (firstRow >= endRow) {
            continue; // edge lies between two pixel rows: crosses no centre
        }
        edges.push_back(Edge{firstRow, endRow, top, bottom, 0.0});
    }
    if (edges.empty()) {
        return;
    }

    std::sort(edges.begin(), edges.end(),
              [](const Edge& a, const Edge& b) { return a.firstRow < b.firstRow; });

    int lastRow = edges[0].endRow;
    for (const Edge& e : edges) {
        lastRow = std::max(lastRow, e.endRow);
    }

    // 2. Walk the scanlines.
    std::vector<Edge> active;
    size_t nextEdge = 0;
    for (int y = edges[0].firstRow; y < lastRow && y < canvas.height(); ++y) {
        while (nextEdge < edges.size() && edges[nextEdge].firstRow <= y) {
            active.push_back(edges[nextEdge]);
            ++nextEdge;
        }
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [y](const Edge& e) { return e.endRow <= y; }),
                     active.end());
        for (Edge& e : active) {
            e.x = edgeXAtY(e.top, e.bottom, y);
        }
        std::sort(active.begin(), active.end(),
                  [](const Edge& a, const Edge& b) { return a.x < b.x; });

        if (y >= 0) {
            // Pairs of crossings: inside between 1st-2nd, 3rd-4th, ...
            for (size_t i = 0; i + 1 < active.size(); i += 2) {
                const int xStart = std::max(ceilToInt(active[i].x), 0);
                const int xEnd = std::min(ceilToInt(active[i + 1].x), canvas.width()); // exclusive
                for (int x = xStart; x < xEnd; ++x) {
                    canvas.setPixel(x, y, color);
                }
            }
        }
    }
}

// Boundary fill with an explicit stack.
//
// The textbook version is recursive:
//     fill(x, y): if pixel is not boundary and not filled:
//                     paint it; fill(x+1,y); fill(x-1,y); fill(x,y+1); fill(x,y-1)
// For a large room that recursion can be hundreds of thousands of calls
// deep and crash with a stack overflow. Here the "calls still to do" are
// kept in a std::vector used as a stack instead, which grows on the heap.
//
// Why 4-connected and not 8-connected: boundaries drawn with Bresenham are
// 8-connected, so a diagonal boundary has pixels that only touch at their
// corners. An 8-connected fill would step diagonally between those pixels
// and leak out; a 4-connected fill cannot.
int boundaryFill(Canvas& canvas, int x, int y, Color fillColor, Color boundaryColor) {
    auto shouldPaint = [&](int px, int py) {
        if (!canvas.inBounds(px, py)) {
            return false;
        }
        const Color c = canvas.getPixel(px, py);
        return !sameColor(c, boundaryColor) && !sameColor(c, fillColor);
    };

    if (!shouldPaint(x, y)) {
        return 0;
    }

    int painted = 0;
    std::vector<std::pair<int, int>> stack;
    stack.push_back({x, y});
    while (!stack.empty()) {
        const std::pair<int, int> p = stack.back();
        stack.pop_back();
        // The same pixel can be pushed by several neighbours, so check again.
        if (!shouldPaint(p.first, p.second)) {
            continue;
        }
        canvas.setPixel(p.first, p.second, fillColor);
        ++painted;

        stack.push_back({p.first + 1, p.second});
        stack.push_back({p.first - 1, p.second});
        stack.push_back({p.first, p.second + 1});
        stack.push_back({p.first, p.second - 1});
    }
    return painted;
}

} // namespace raster
