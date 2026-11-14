#pragma once

#include "raster/polygon.h"
#include "raster/types.h"

// 2D transformations as 3x3 homogeneous matrices.
//
// Why homogeneous coordinates?
// ----------------------------
// Rotation, scaling, shear and reflection are all "multiply the point by a
// 2x2 matrix". Translation is not: it ADDS an offset, and no 2x2 matrix can
// do that (a 2x2 matrix always leaves the origin where it is). The trick is
// to write the point (x, y) with a third coordinate that is always 1:
//
//     | x' |   | a  b  tx |   | x |       x' = a*x + b*y + tx
//     | y' | = | c  d  ty | * | y |       y' = c*x + d*y + ty
//     | 1  |   | 0  0  1  |   | 1 |       1  = 1
//
// Now translation is a matrix multiplication too, so EVERY transform is one
// 3x3 matrix, and a whole chain of transforms can be multiplied together
// into a single matrix that is then applied to every vertex.
//
// Conventions
// -----------
// - m[row][column].
// - Points are COLUMN vectors and are multiplied on the right: p' = M * p.
//   So in the product A * B, the transform B is applied FIRST and A second.
//   Read a chain from right to left.
// - Screen coordinates (origin top-left, y DOWN - see raster/types.h), so a
//   positive rotation angle turns CLOCKWISE on screen, exactly like
//   raster::rotate() from Checkpoint 1. Angles are in degrees.
//
// RoomCraft use: moving, turning, resizing and mirroring furniture, opening a
// door about its hinge, and (in the other CP2 parts) cameras, hierarchical
// models and animation, which all work by building a Mat3 per frame.

namespace xform {

struct Mat3 {
    float m[3][3];

    // The matrix that changes nothing.
    static Mat3 identity();

    // Moves every point by (tx, ty).
    static Mat3 translation(float tx, float ty);

    // Rotates about the ORIGIN. Positive = clockwise on screen.
    static Mat3 rotation(float angleDegrees);

    // Scales about the ORIGIN by sx horizontally and sy vertically.
    static Mat3 scaling(float sx, float sy);

    // Shear about the origin: x' = x + shx * y,  y' = y + shy * x.
    // With only one of shx / shy non-zero the area is unchanged.
    static Mat3 shear(float shx, float shy);

    // Mirror image in the x axis (the horizontal line y = 0): y -> -y.
    static Mat3 reflectionAcrossXAxis();

    // Mirror image in the y axis (the vertical line x = 0): x -> -x.
    static Mat3 reflectionAcrossYAxis();

    // Rotation / scaling about any point instead of the origin. The pivot
    // itself does not move. Built as composites (see mat3.cpp).
    static Mat3 rotationAbout(float angleDegrees, raster::Point pivot);
    static Mat3 scalingAbout(float sx, float sy, raster::Point pivot);
};

// Matrix product. (a * b) applied to a point does b first, then a.
Mat3 operator*(const Mat3& a, const Mat3& b);

// Applies the matrix to one point (written as the column vector (x, y, 1)).
raster::Point transformPoint(const Mat3& m, raster::Point p);

// Applies the matrix to every vertex. Returns a new polygon; the original is
// not changed. Vertex order is kept.
raster::Polygon transformPolygon(const Mat3& m, const raster::Polygon& polygon);

// Determinant. For our transforms its absolute value is the factor by which
// areas grow, and a negative sign means the shape was mirrored.
float determinant(const Mat3& m);

// Computes the matrix that undoes m, so that out * m = identity.
// Returns false (and leaves `out` untouched) if m cannot be undone, which
// happens when it squashes the plane flat (determinant 0, e.g. scale by 0).
bool inverse(const Mat3& m, Mat3& out);

} // namespace xform
