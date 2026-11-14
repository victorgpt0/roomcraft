#pragma once

#include "xform/vec.h"

// 3D transformations as 4x4 homogeneous matrices.
//
// This is Mat3 (xform/mat3.h) one dimension up. A 3D point (x, y, z) is
// written as the column vector (x, y, z, 1) so that translation, like
// rotation and scaling, is a matrix multiplication:
//
//   | x' |   | a  b  c  tx |   | x |
//   | y' | = | d  e  f  ty | * | y |
//   | z' |   | g  h  i  tz |   | z |
//   | w' |   | 0  0  0  1  |   | 1 |
//
// The bottom row stays (0, 0, 0, 1) for every transform EXCEPT the
// perspective projection, which puts -z into w' so that dividing by w' later
// makes far things smaller (see perspective() in mat4.cpp).
//
// Conventions (the same as Mat3)
// ------------------------------
// - m[row][column].
// - Points are COLUMN vectors: p' = M * p. In A * B, B is applied FIRST.
// - Angles in degrees.
//
// The 3D world (shared by parts 2, 3 and 4): plan point (x, y) at height h is
// world point (X = x, Y = h, Z = y). Y points UP. This is a RIGHT-HANDED
// system, the usual one for 3D graphics, so the rotations are the standard
// right-handed ones: a positive angle turns COUNTER-CLOCKWISE when you look
// down the axis towards the origin.
//
// Looking down from above (down the Y axis) at the floor plan, rotationY(+a)
// therefore turns the plan counter-clockwise, the opposite of
// Mat3::rotation(+a). To turn a 3D piece the way its 2D pose turns it, use
// rotationY(-pose.angleDegrees).
//
// RoomCraft use: the 3D preview of the room. Each frame builds
//   projection * view
// once and sends every vertex of every wireframe box through it.

namespace view {

struct Mat4 {
    float m[4][4];

    static Mat4 identity();
    static Mat4 translation(float tx, float ty, float tz);
    static Mat4 scaling(float sx, float sy, float sz);

    // Rotations about the X, Y and Z axes through the origin (right-handed).
    static Mat4 rotationX(float angleDegrees);
    static Mat4 rotationY(float angleDegrees);
    static Mat4 rotationZ(float angleDegrees);

    // View matrix: moves and turns the world so that the camera at `eye`,
    // looking at `target`, ends up at the origin looking down the NEGATIVE z
    // axis, with `up` pointing (as nearly as possible) along +y.
    // `up` must not be parallel to target - eye.
    static Mat4 lookAt(xform::Vec3 eye, xform::Vec3 target, xform::Vec3 up);

    // Perspective projection of the view frustum (a pyramid with its tip at
    // the camera, cut at distances zNear and zFar) onto the cube -1..1.
    // fovYDegrees is the vertical field of view; aspect = width / height.
    // Needs 0 < zNear < zFar. The result's w is the distance in front of the
    // camera; x, y and z must be divided by it (the perspective divide).
    static Mat4 perspective(float fovYDegrees, float aspect, float zNear, float zFar);

    // Orthographic (parallel) projection of the box
    //   left..right, bottom..top, and distance zNear..zFar in front of the camera
    // onto the cube -1..1. w stays 1: size does not depend on distance.
    static Mat4 orthographic(float left, float right, float bottom, float top,
                             float zNear, float zFar);
};

// Matrix product. (a * b) applied to a point does b first, then a.
Mat4 operator*(const Mat4& a, const Mat4& b);

// Matrix times homogeneous column vector. No division by w.
xform::Vec4 operator*(const Mat4& m, xform::Vec4 v);

// Applies an AFFINE matrix (bottom row 0 0 0 1, so w stays 1: everything
// except a projection) to a 3D point.
xform::Vec3 transformPoint(const Mat4& m, xform::Vec3 p);

// Small vector helpers used by lookAt and the orbit camera. (Named functions
// rather than operators: xform::Vec3 belongs to Part 1, and operators for it
// declared here could clash with any that xform adds later.)
xform::Vec3 add(xform::Vec3 a, xform::Vec3 b);
xform::Vec3 subtract(xform::Vec3 a, xform::Vec3 b);
xform::Vec3 scaled(xform::Vec3 v, float s);
float dot(xform::Vec3 a, xform::Vec3 b);
xform::Vec3 cross(xform::Vec3 a, xform::Vec3 b);
float length(xform::Vec3 v);
xform::Vec3 normalize(xform::Vec3 v); // returns v unchanged if its length is 0

} // namespace view
