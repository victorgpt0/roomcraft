#include "view/mat4.h"

#include "raster/types.h"

#include <cmath>

namespace view {

namespace {

void cosSin(float angleDegrees, float& c, float& s) {
    const double radians = angleDegrees * raster::kPi / 180.0;
    c = static_cast<float>(std::cos(radians));
    s = static_cast<float>(std::sin(radians));
}

} // namespace

Mat4 Mat4::identity() {
    return Mat4{{{1.0f, 0.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

// The offset sits in the fourth column, multiplied by the point's w = 1.
Mat4 Mat4::translation(float tx, float ty, float tz) {
    return Mat4{{{1.0f, 0.0f, 0.0f, tx},
                 {0.0f, 1.0f, 0.0f, ty},
                 {0.0f, 0.0f, 1.0f, tz},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

Mat4 Mat4::scaling(float sx, float sy, float sz) {
    return Mat4{{{sx, 0.0f, 0.0f, 0.0f},
                 {0.0f, sy, 0.0f, 0.0f},
                 {0.0f, 0.0f, sz, 0.0f},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

// Each rotation is the 2D rotation applied to the two coordinates
// perpendicular to the axis, leaving the axis coordinate alone. The pairs
// are taken in the cyclic order x -> y -> z -> x, which is what makes every
// positive angle counter-clockwise when seen from the positive end of its
// axis (the right-hand rule):
//
//   about X:  (y, z):  y' = y c - z s,  z' = y s + z c      y turns towards z
//   about Y:  (z, x):  z' = z c - x s,  x' = z s + x c      z turns towards x
//   about Z:  (x, y):  x' = x c - y s,  y' = x s + y c      x turns towards y
//
// rotationY looks "odd" (the -s is in the bottom-left) only because the pair
// is (z, x) rather than (x, z).
Mat4 Mat4::rotationX(float angleDegrees) {
    float c = 1.0f, s = 0.0f;
    cosSin(angleDegrees, c, s);
    return Mat4{{{1.0f, 0.0f, 0.0f, 0.0f},
                 {0.0f, c, -s, 0.0f},
                 {0.0f, s, c, 0.0f},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

Mat4 Mat4::rotationY(float angleDegrees) {
    float c = 1.0f, s = 0.0f;
    cosSin(angleDegrees, c, s);
    return Mat4{{{c, 0.0f, s, 0.0f},
                 {0.0f, 1.0f, 0.0f, 0.0f},
                 {-s, 0.0f, c, 0.0f},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

Mat4 Mat4::rotationZ(float angleDegrees) {
    float c = 1.0f, s = 0.0f;
    cosSin(angleDegrees, c, s);
    return Mat4{{{c, -s, 0.0f, 0.0f},
                 {s, c, 0.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

// The view matrix (camera basis)
// ------------------------------
// We want coordinates relative to the camera: the camera at the origin,
// looking down -z, with "up on the screen" along +y. Build the camera's own
// three axes, expressed in world coordinates:
//
//   f = normalize(target - eye)   forward: the viewing direction
//   r = normalize(f x up)         right: perpendicular to forward and up
//   u = r x f                     true up: perpendicular to both; it is the
//                                 given `up` straightened to be at 90 degrees
//                                 to f, already of length 1
//
// r, u and -f are three perpendicular unit vectors (a right-handed basis,
// like x, y, z). The coordinates of a point p in that basis are its dot
// products with the axes, measured from the eye:
//
//   x_cam = r . (p - eye),  y_cam = u . (p - eye),  z_cam = -f . (p - eye)
//
// As a matrix: a ROTATION whose rows are r, u and -f, applied after a
// TRANSLATION by -eye:
//
//          | rx  ry  rz  0 |   | 1 0 0 -ex |   | rx  ry  rz  -r.eye |
//   View = | ux  uy  uz  0 | * | 0 1 0 -ey | = | ux  uy  uz  -u.eye |
//          |-fx -fy -fz  0 |   | 0 0 1 -ez |   |-fx -fy -fz   f.eye |
//          | 0   0   0   1 |   | 0 0 0  1  |   | 0   0   0    1     |
//
// Check: the eye -> (0, 0, 0); the target, at distance d along f, ->
// (0, 0, -d), straight ahead on the negative z axis.
Mat4 Mat4::lookAt(xform::Vec3 eye, xform::Vec3 target, xform::Vec3 up) {
    const xform::Vec3 f = normalize(subtract(target, eye));
    const xform::Vec3 r = normalize(cross(f, up));
    const xform::Vec3 u = cross(r, f);
    return Mat4{{{r.x, r.y, r.z, -dot(r, eye)},
                 {u.x, u.y, u.z, -dot(u, eye)},
                 {-f.x, -f.y, -f.z, dot(f, eye)},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

// Perspective projection
// ----------------------
// In camera coordinates the camera looks down -z, so a point in front of it
// has z < 0 and is at distance d = -z.
//
// x and y: similar triangles. A point at height y and distance d appears on
// a screen at distance 1 from the eye at height y / d. The screen edge that
// should reach +1 is at height t = tan(fovY / 2) there, so
//
//   y_ndc = (y / d) / t = (1/t) * y / (-z),    x_ndc = (1/t / aspect) * x / (-z)
//
// Dividing by -z is not a linear operation, so no matrix can do it directly.
// Instead the matrix puts -z into w (bottom row 0 0 -1 0), and the division
// is done afterwards by dividing x, y and z by w: the PERSPECTIVE DIVIDE.
//
// z (depth): we also want z_ndc = -1 at the near plane (z = -n) and +1 at
// the far plane (z = -f). With the third row (0, 0, A, B):
//
//   z_ndc = (A z + B) / (-z)
//   z = -n:  (-A n + B) / n = -1   ->   B - A n = -n
//   z = -f:  (-A f + B) / f = +1   ->   B - A f =  f
//   subtract: A (f - n) = -(f + n)  ->  A = (f + n) / (n - f),  B = 2 f n / (n - f)
//
//          | 1/(t*aspect)  0     0     0 |
//   Proj = | 0             1/t   0     0 |
//          | 0             0     A     B |
//          | 0             0    -1     0 |
Mat4 Mat4::perspective(float fovYDegrees, float aspect, float zNear, float zFar) {
    const double halfFov = fovYDegrees * raster::kPi / 360.0;
    const float g = static_cast<float>(1.0 / std::tan(halfFov)); // 1 / t
    const float a = (zFar + zNear) / (zNear - zFar);
    const float b = 2.0f * zFar * zNear / (zNear - zFar);
    return Mat4{{{g / aspect, 0.0f, 0.0f, 0.0f},
                 {0.0f, g, 0.0f, 0.0f},
                 {0.0f, 0.0f, a, b},
                 {0.0f, 0.0f, -1.0f, 0.0f}}};
}

// Orthographic projection
// -----------------------
// Parallel lines of sight: x and y are used as they are, whatever the
// distance. Each axis is a window-to-viewport mapping from an interval onto
// -1..1 (the same idea as view::windowToViewport, in 1D):
//
//   x_ndc = 2 (x - l) / (r - l) - 1 = 2x / (r - l) - (r + l) / (r - l)
//   y_ndc = 2y / (t - b) - (t + b) / (t - b)
//
// For depth the interval is z = -n .. -f (in front of the camera), mapped to
// -1 .. +1:
//
//   z_ndc = -2z / (f - n) - (f + n) / (f - n)
//
// The bottom row stays (0 0 0 1), so w = 1 and the divide changes nothing.
Mat4 Mat4::orthographic(float left, float right, float bottom, float top, float zNear, float zFar) {
    return Mat4{{{2.0f / (right - left), 0.0f, 0.0f, -(right + left) / (right - left)},
                 {0.0f, 2.0f / (top - bottom), 0.0f, -(top + bottom) / (top - bottom)},
                 {0.0f, 0.0f, -2.0f / (zFar - zNear), -(zFar + zNear) / (zFar - zNear)},
                 {0.0f, 0.0f, 0.0f, 1.0f}}};
}

Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[row][k] * b.m[k][col];
            }
            result.m[row][col] = sum;
        }
    }
    return result;
}

xform::Vec4 operator*(const Mat4& m, xform::Vec4 v) {
    const float in[4] = {v.x, v.y, v.z, v.w};
    float out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int row = 0; row < 4; ++row) {
        for (int k = 0; k < 4; ++k) {
            out[row] += m.m[row][k] * in[k];
        }
    }
    return xform::Vec4{out[0], out[1], out[2], out[3]};
}

xform::Vec3 transformPoint(const Mat4& m, xform::Vec3 p) {
    const xform::Vec4 q = m * xform::Vec4{p.x, p.y, p.z, 1.0f};
    return xform::Vec3{q.x, q.y, q.z};
}

xform::Vec3 add(xform::Vec3 a, xform::Vec3 b) {
    return xform::Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

xform::Vec3 subtract(xform::Vec3 a, xform::Vec3 b) {
    return xform::Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

xform::Vec3 scaled(xform::Vec3 v, float s) {
    return xform::Vec3{v.x * s, v.y * s, v.z * s};
}

float dot(xform::Vec3 a, xform::Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Perpendicular to both a and b, pointing by the right-hand rule
// (x cross y = z), with length |a| |b| sin(angle between them).
xform::Vec3 cross(xform::Vec3 a, xform::Vec3 b) {
    return xform::Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float length(xform::Vec3 v) {
    return std::sqrt(dot(v, v));
}

xform::Vec3 normalize(xform::Vec3 v) {
    const float len = length(v);
    return len == 0.0f ? v : scaled(v, 1.0f / len);
}

} // namespace view
