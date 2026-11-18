#pragma once

#include "raster/polygon.h"

#include <array>
#include <vector>

// Polygon triangulation by ear clipping.
//
// OpenGL draws only points, lines and TRIANGLES. Checkpoint 1 filled any
// polygon, convex or concave, with its own scanline algorithm; the GPU cannot
// do that directly. So every furniture footprint and every top face of a 3D
// piece must first be cut into triangles that cover it exactly, without gaps
// or overlaps.
//
// Ear clipping
// ------------
// An EAR of a polygon is a corner (prev, v, next) where
//   1. the corner is convex (it turns the same way as the polygon as a whole),
//   2. no other vertex of the polygon lies inside the triangle (prev, v, next).
// Then the triangle lies completely inside the polygon, and cutting it off
// leaves a polygon with one vertex fewer. Every simple polygon with more than
// three vertices has at least two ears (the "two ears theorem"), so repeating
// "find an ear, cut it off" always finishes, with n - 2 triangles for n
// vertices.
//
// RoomCraft use: the L-shaped sofa and the kidney-shaped coffee table are
// concave; a "triangle fan" from one corner would draw triangles OUTSIDE them.
// Ear clipping handles them, and the convex pieces, the same way.

namespace editor {

using Triangle = std::array<int, 3>; // indices into the polygon's vertices

// Triangulates a simple polygon (edges do not cross each other), in either
// winding. Returns n - 2 triangles for n distinct, non-collinear vertices.
//
// - Each triangle lists its vertices in the SAME winding as the polygon, so
//   a renderer can rely on a consistent front face.
// - Consecutive duplicate vertices and vertices lying on a straight edge are
//   skipped (they would make triangles of zero area).
// - Returns nothing for fewer than 3 usable vertices or zero area.
std::vector<Triangle> triangulate(const raster::Polygon& polygon);

// Sum of the areas of the triangles (should equal the polygon's area).
float triangulatedArea(const raster::Polygon& polygon, const std::vector<Triangle>& triangles);

// Area where two simple polygons overlap. Both are triangulated and every
// triangle of one is clipped against every triangle of the other with the
// Checkpoint 1 Sutherland-Hodgman algorithm (which needs a CONVEX clip
// window - a triangle always is). The triangles of one polygon do not
// overlap each other, so the pieces simply add up. Works for concave
// polygons, which Sutherland-Hodgman alone cannot clip against.
//
// RoomCraft use: warning when two pieces of furniture stand in the same
// place.
float overlapArea(const raster::Polygon& a, const raster::Polygon& b);

} // namespace editor
