#include "xform/mat3.h"

#include <cmath>

namespace xform {

Mat3 Mat3::identity() {
    return Mat3{{{1.0f, 0.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}}};
}

// The offset sits in the third column. Multiplying by (x, y, 1) gives
//   x' = 1*x + 0*y + tx,   y' = 0*x + 1*y + ty.
Mat3 Mat3::translation(float tx, float ty) {
    return Mat3{{{1.0f, 0.0f, tx},
                 {0.0f, 1.0f, ty},
                 {0.0f, 0.0f, 1.0f}}};
}

// Rotation about the origin:
//   x' = x * cos(a) - y * sin(a)
//   y' = x * sin(a) + y * cos(a)
// This is the same formula as raster::rotate() in polygon.cpp. In maths
// (y up) it turns counter-clockwise; our y axis points down, which mirrors
// the picture, so on screen a positive angle turns clockwise.
Mat3 Mat3::rotation(float angleDegrees) {
    const double radians = angleDegrees * raster::kPi / 180.0;
    const float c = static_cast<float>(std::cos(radians));
    const float s = static_cast<float>(std::sin(radians));
    return Mat3{{{c, -s, 0.0f},
                 {s, c, 0.0f},
                 {0.0f, 0.0f, 1.0f}}};
}

Mat3 Mat3::scaling(float sx, float sy) {
    return Mat3{{{sx, 0.0f, 0.0f},
                 {0.0f, sy, 0.0f},
                 {0.0f, 0.0f, 1.0f}}};
}

// Shear slides each point sideways by an amount that depends on its OTHER
// coordinate: x' = x + shx * y pushes points further right the lower they
// are on screen, turning a rectangle into a parallelogram.
Mat3 Mat3::shear(float shx, float shy) {
    return Mat3{{{1.0f, shx, 0.0f},
                 {shy, 1.0f, 0.0f},
                 {0.0f, 0.0f, 1.0f}}};
}

// Reflection is a scale by -1 along one axis.
Mat3 Mat3::reflectionAcrossXAxis() {
    return scaling(1.0f, -1.0f);
}

Mat3 Mat3::reflectionAcrossYAxis() {
    return scaling(-1.0f, 1.0f);
}

// Rotation about a pivot P.
//
// Mat3::rotation only knows how to turn things around the origin. To turn
// around P instead, we temporarily make P the origin:
//
//   1. translate by -P        (P moves to the origin)
//   2. rotate about the origin
//   3. translate by +P        (P moves back to where it was)
//
// Points are column vectors, so the FIRST step is written on the RIGHT:
//
//   M = T(+P) * R(angle) * T(-P)
//
// P itself goes P -> origin -> origin -> P, so it stays fixed, which is
// exactly what "rotating about P" means.
Mat3 Mat3::rotationAbout(float angleDegrees, raster::Point pivot) {
    return translation(pivot.x, pivot.y) * rotation(angleDegrees) * translation(-pivot.x, -pivot.y);
}

// Scaling about a pivot P: the same three steps with a scale in the middle.
//   M = T(+P) * S(sx, sy) * T(-P)
Mat3 Mat3::scalingAbout(float sx, float sy, raster::Point pivot) {
    return translation(pivot.x, pivot.y) * scaling(sx, sy) * translation(-pivot.x, -pivot.y);
}

// Row-times-column product: result[i][j] = sum over k of a[i][k] * b[k][j].
// The sum is done in double and stored as float, so long chains of products
// lose as little precision as possible.
Mat3 operator*(const Mat3& a, const Mat3& b) {
    Mat3 result;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) {
                sum += static_cast<double>(a.m[i][k]) * b.m[k][j];
            }
            result.m[i][j] = static_cast<float>(sum);
        }
    }
    return result;
}

// M * (x, y, 1). The third component w of the result is 1 for every
// transform built in this file (their bottom row is 0 0 1). Dividing by w
// anyway is what turns a homogeneous point back into an ordinary one, and
// keeps this function correct for any 3x3 matrix.
raster::Point transformPoint(const Mat3& m, raster::Point p) {
    const double x = static_cast<double>(m.m[0][0]) * p.x + static_cast<double>(m.m[0][1]) * p.y + m.m[0][2];
    const double y = static_cast<double>(m.m[1][0]) * p.x + static_cast<double>(m.m[1][1]) * p.y + m.m[1][2];
    const double w = static_cast<double>(m.m[2][0]) * p.x + static_cast<double>(m.m[2][1]) * p.y + m.m[2][2];
    if (w != 1.0 && w != 0.0) {
        return raster::Point{static_cast<float>(x / w), static_cast<float>(y / w)};
    }
    return raster::Point{static_cast<float>(x), static_cast<float>(y)};
}

raster::Polygon transformPolygon(const Mat3& m, const raster::Polygon& polygon) {
    raster::Polygon result;
    result.vertices.reserve(polygon.vertices.size());
    for (const raster::Point& v : polygon.vertices) {
        result.vertices.push_back(transformPoint(m, v));
    }
    return result;
}

namespace {

// Determinant in double, shared by determinant() and inverse().
// Expansion along the first row:
//   det = a(ei - fh) - b(di - fg) + c(dh - eg)   for   | a b c |
//                                                      | d e f |
//                                                      | g h i |
double determinantDouble(const Mat3& m) {
    const double a = m.m[0][0], b = m.m[0][1], c = m.m[0][2];
    const double d = m.m[1][0], e = m.m[1][1], f = m.m[1][2];
    const double g = m.m[2][0], h = m.m[2][1], i = m.m[2][2];
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

} // namespace

float determinant(const Mat3& m) {
    return static_cast<float>(determinantDouble(m));
}

// Inverse by the adjugate:   inverse(M) = adjugate(M) / det(M).
//
// The adjugate is the TRANSPOSE of the cofactor matrix. The cofactor of
// entry (row r, column c) is the determinant of the 2x2 matrix left after
// deleting row r and column c, with sign (-1)^(r+c). Because of the
// transpose, the cofactor of (r, c) is stored at out[c][r].
//
// If det(M) = 0 the matrix has squashed the plane onto a line or a point
// (for example a scale by 0), many points landed on the same spot, and there
// is no way to undo that: no inverse exists.
bool inverse(const Mat3& m, Mat3& out) {
    const double det = determinantDouble(m);
    if (std::fabs(det) < 1e-12) {
        return false;
    }

    const double a = m.m[0][0], b = m.m[0][1], c = m.m[0][2];
    const double d = m.m[1][0], e = m.m[1][1], f = m.m[1][2];
    const double g = m.m[2][0], h = m.m[2][1], i = m.m[2][2];

    Mat3 result;
    result.m[0][0] = static_cast<float>((e * i - f * h) / det);
    result.m[0][1] = static_cast<float>(-(b * i - c * h) / det);
    result.m[0][2] = static_cast<float>((b * f - c * e) / det);
    result.m[1][0] = static_cast<float>(-(d * i - f * g) / det);
    result.m[1][1] = static_cast<float>((a * i - c * g) / det);
    result.m[1][2] = static_cast<float>(-(a * f - c * d) / det);
    result.m[2][0] = static_cast<float>((d * h - e * g) / det);
    result.m[2][1] = static_cast<float>(-(a * h - b * g) / det);
    result.m[2][2] = static_cast<float>((a * e - b * d) / det);
    out = result;
    return true;
}

} // namespace xform
