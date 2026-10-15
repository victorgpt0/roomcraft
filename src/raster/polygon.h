#pragma once

#include "raster/types.h"

#include <vector>

// Polygon representation and operations.
//
// A polygon is stored as an ordered list of vertices. Consecutive vertices
// are joined by edges, and the last vertex joins back to the first (the
// closing edge is implied; do not repeat the first vertex at the end).
// Coordinates are screen coordinates (origin top-left, y down - see types.h).
//
// RoomCraft use: furniture footprints and the room outline. Transformations
// move, turn and resize furniture in the editor; area/centroid/bounding box
// give floor-space statistics and selection handles; point-in-polygon tells
// which piece of furniture the user clicked.

namespace raster {

struct Polygon {
    std::vector<Point> vertices;
};

// Axis-aligned rectangle polygon with top-left corner (x, y), listed
// top-left, top-right, bottom-right, bottom-left.
Polygon makeRectangle(float x, float y, float width, float height);

// Moves every vertex by (dx, dy).
void translate(Polygon& polygon, float dx, float dy);

// Rotates every vertex by angleDegrees around pivot. Because y points DOWN on
// screen, a positive angle turns CLOCKWISE as seen on screen.
void rotate(Polygon& polygon, float angleDegrees, Point pivot);

// Scales every vertex away from (or towards) pivot by sx horizontally and sy
// vertically. The pivot itself does not move.
void scale(Polygon& polygon, float sx, float sy, Point pivot);

// Shoelace formula. Positive when the vertices run CLOCKWISE on screen
// (y down), negative when counter-clockwise. 0 for fewer than 3 vertices.
float computeSignedArea(const Polygon& polygon);

// Area enclosed by the polygon (always >= 0). Valid for any simple polygon,
// convex or concave, as long as its edges do not cross each other.
float computeArea(const Polygon& polygon);

// Centre of mass of the polygon's area (not the average of its vertices).
// For a degenerate polygon with zero area (a line or a point), falls back to
// the average of the vertices. Returns {0, 0} for an empty polygon.
Point computeCentroid(const Polygon& polygon);

// Smallest axis-aligned rectangle containing every vertex.
// Returns an all-zero Rect for an empty polygon.
Rect computeBoundingBox(const Polygon& polygon);

// Even-odd (ray crossing) test: is point p inside the polygon?
// Works for concave polygons. Points exactly on an edge may be reported
// either inside or outside (see polygon.cpp for the exact rule).
bool pointInPolygon(const Polygon& polygon, Point p);

} // namespace raster
