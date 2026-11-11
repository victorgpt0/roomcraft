// Tests for xform::Mat3: 3x3 homogeneous 2D transforms.
//
// Every expected value below is worked out by hand from the definition of
// the transform, or comes from the Checkpoint 1 polygon functions, which
// have their own tests.

#include "raster/polygon.h"
#include "xform/mat3.h"

#include "test_helpers.h"

#include <cstddef>

using raster::Point;
using raster::Polygon;
using xform::Mat3;

namespace {

#define CHECK_POINT(actual, ex, ey, tolerance)      \
    do {                                             \
        const raster::Point checkedPoint = (actual); \
        CHECK_NEAR(checkedPoint.x, (ex), (tolerance)); \
        CHECK_NEAR(checkedPoint.y, (ey), (tolerance)); \
    } while (false)

void checkMatricesNear(const Mat3& a, const Mat3& b, double tolerance) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            CHECK_NEAR(a.m[i][j], b.m[i][j], tolerance);
        }
    }
}

void checkPolygonsNear(const Polygon& a, const Polygon& b, double tolerance) {
    if (!CHECK(a.vertices.size() == b.vertices.size())) {
        return;
    }
    for (std::size_t i = 0; i < a.vertices.size(); ++i) {
        CHECK_NEAR(a.vertices[i].x, b.vertices[i].x, tolerance);
        CHECK_NEAR(a.vertices[i].y, b.vertices[i].y, tolerance);
    }
}

// An armchair-sized footprint somewhere in the room, deliberately not
// symmetric, so a wrong rotation direction or a mirror image is detected.
Polygon makeTestShape() {
    Polygon p;
    p.vertices = {{275, 75}, {345, 75}, {345, 145}, {320, 145}, {320, 100}, {275, 100}};
    return p;
}

// Each basic matrix maps known points to hand-computed results.
void testBasicMatrices() {
    const Point p{3, 4};

    CHECK_POINT(xform::transformPoint(Mat3::identity(), p), 3, 4, 0);

    // Translation adds the offset.
    CHECK_POINT(xform::transformPoint(Mat3::translation(10, -5), p), 13, -1, 0);

    // Rotation by +90: the point on the +x axis (right) goes to the +y axis,
    // which is DOWN on screen, i.e. a clockwise quarter turn.
    CHECK_POINT(xform::transformPoint(Mat3::rotation(90), {1, 0}), 0, 1, 1e-6);
    CHECK_POINT(xform::transformPoint(Mat3::rotation(90), {0, 1}), -1, 0, 1e-6);
    CHECK_POINT(xform::transformPoint(Mat3::rotation(180), p), -3, -4, 1e-5);
    CHECK_POINT(xform::transformPoint(Mat3::rotation(-90), {1, 0}), 0, -1, 1e-6);
    // 3-4-5 triangle turned by 90: (3, 4) -> (-4, 3).
    CHECK_POINT(xform::transformPoint(Mat3::rotation(90), p), -4, 3, 1e-5);
    // Rotation keeps the distance from the origin.
    const Point turned = xform::transformPoint(Mat3::rotation(37), p);
    CHECK_NEAR(std::sqrt(turned.x * turned.x + turned.y * turned.y), 5.0, 1e-5);

    // Scaling multiplies each coordinate.
    CHECK_POINT(xform::transformPoint(Mat3::scaling(2, 3), p), 6, 12, 0);
    CHECK_POINT(xform::transformPoint(Mat3::scaling(0.5f, 1), p), 1.5, 4, 0);

    // Shear: x' = x + shx * y,  y' = y + shy * x.
    CHECK_POINT(xform::transformPoint(Mat3::shear(0.5f, 0), {10, 20}), 20, 20, 0);
    CHECK_POINT(xform::transformPoint(Mat3::shear(0, 2), {10, 20}), 10, 40, 0);
    // A point on the x axis is not moved by an x-shear.
    CHECK_POINT(xform::transformPoint(Mat3::shear(0.5f, 0), {10, 0}), 10, 0, 0);

    // Reflections flip one coordinate.
    CHECK_POINT(xform::transformPoint(Mat3::reflectionAcrossXAxis(), p), 3, -4, 0);
    CHECK_POINT(xform::transformPoint(Mat3::reflectionAcrossYAxis(), p), -3, 4, 0);

    // The origin stays put under everything except translation: that is why
    // translation needs the third (homogeneous) column.
    CHECK_POINT(xform::transformPoint(Mat3::rotation(33), {0, 0}), 0, 0, 0);
    CHECK_POINT(xform::transformPoint(Mat3::scaling(7, 9), {0, 0}), 0, 0, 0);
    CHECK_POINT(xform::transformPoint(Mat3::shear(2, 3), {0, 0}), 0, 0, 0);
    CHECK_POINT(xform::transformPoint(Mat3::translation(7, 9), {0, 0}), 7, 9, 0);

    // Bottom row of every affine matrix is 0 0 1.
    const Mat3 t = Mat3::translation(10, -5);
    CHECK(t.m[0][2] == 10.0f && t.m[1][2] == -5.0f);
    CHECK(t.m[2][0] == 0.0f && t.m[2][1] == 0.0f && t.m[2][2] == 1.0f);
}

// Mat3 must agree with the Checkpoint 1 functions that move the vertices
// directly, for many angles, pivots and factors.
void testAgreesWithCheckpoint1() {
    const Polygon shape = makeTestShape();
    const Point pivots[] = {{0, 0}, {310, 110}, {40, 40}, {-25, 600}};

    // translate
    {
        Polygon expected = shape;
        raster::translate(expected, 37.5f, -120.0f);
        checkPolygonsNear(xform::transformPolygon(Mat3::translation(37.5f, -120.0f), shape), expected, 1e-4);
    }

    // rotate, every 15 degrees over more than a full turn, about each pivot
    for (const Point pivot : pivots) {
        for (int angle = -360; angle <= 720; angle += 15) {
            Polygon expected = shape;
            raster::rotate(expected, static_cast<float>(angle), pivot);
            const Mat3 m = Mat3::rotationAbout(static_cast<float>(angle), pivot);
            checkPolygonsNear(xform::transformPolygon(m, shape), expected, 2e-3);
        }
    }

    // Rotation about the origin: Mat3::rotation is raster::rotate with pivot (0, 0).
    {
        Polygon expected = shape;
        raster::rotate(expected, 25.0f, {0, 0});
        checkPolygonsNear(xform::transformPolygon(Mat3::rotation(25.0f), shape), expected, 1e-3);
    }

    // scale about each pivot
    const float factors[][2] = {{2, 2}, {0.5f, 1.5f}, {1, 3}, {-1, 1}};
    for (const Point pivot : pivots) {
        for (const auto& f : factors) {
            Polygon expected = shape;
            raster::scale(expected, f[0], f[1], pivot);
            const Mat3 m = Mat3::scalingAbout(f[0], f[1], pivot);
            checkPolygonsNear(xform::transformPolygon(m, shape), expected, 2e-3);
        }
    }
}

// Matrix multiplication is not commutative: the same two transforms in the
// other order give a different result.
void testOrderMatters() {
    const Mat3 t = Mat3::translation(100, 0);
    const Mat3 r = Mat3::rotation(90);
    const Point p{10, 0};

    // T * R: rotate first, (10, 0) -> (0, 10), then move right by 100.
    CHECK_POINT(xform::transformPoint(t * r, p), 100, 10, 1e-4);
    // R * T: move first, (10, 0) -> (110, 0), then rotate about the origin.
    CHECK_POINT(xform::transformPoint(r * t, p), 0, 110, 1e-4);

    // The product does the RIGHT-hand matrix first.
    CHECK_POINT(xform::transformPoint(t * r, p),
                xform::transformPoint(t, xform::transformPoint(r, p)).x,
                xform::transformPoint(t, xform::transformPoint(r, p)).y, 1e-4);
    CHECK_POINT(xform::transformPoint(r * t, p),
                xform::transformPoint(r, xform::transformPoint(t, p)).x,
                xform::transformPoint(r, xform::transformPoint(t, p)).y, 1e-4);

    // Scale and rotate do not commute either (unless the scale is uniform).
    const Mat3 s = Mat3::scaling(2, 1);
    CHECK_POINT(xform::transformPoint(r * s, {1, 0}), 0, 2, 1e-5); // stretch, then turn
    CHECK_POINT(xform::transformPoint(s * r, {1, 0}), 0, 1, 1e-5); // turn, then stretch
    const Mat3 uniform = Mat3::scaling(2, 2);
    checkMatricesNear(r * uniform, uniform * r, 1e-6);

    // Two translations DO commute, and so do two rotations about the origin.
    checkMatricesNear(Mat3::translation(3, 4) * Mat3::translation(-10, 7),
                      Mat3::translation(-10, 7) * Mat3::translation(3, 4), 0);
    checkMatricesNear(Mat3::rotation(20) * Mat3::rotation(50), Mat3::rotation(70), 1e-6);

    // Grouping does not matter (associative), only the order does.
    const Mat3 a = Mat3::rotationAbout(33, {40, 40});
    const Mat3 b = Mat3::shear(0.3f, 0);
    const Mat3 c = Mat3::translation(-70, 15);
    checkMatricesNear((a * b) * c, a * (b * c), 1e-3);

    // Identity is neutral on both sides.
    checkMatricesNear(a * Mat3::identity(), a, 0);
    checkMatricesNear(Mat3::identity() * a, a, 0);
}

// Composite transforms about a pivot.
void testPivotTransforms() {
    const Point pivot{440, 440}; // the door hinge on the floor plan

    for (int angle = 0; angle < 360; angle += 30) {
        const Mat3 m = Mat3::rotationAbout(static_cast<float>(angle), pivot);
        CHECK_POINT(xform::transformPoint(m, pivot), 440, 440, 1e-3);
    }

    // The far end of an 80 cm door lying along the wall to the right of the
    // hinge. Opening it by -90 (counter-clockwise on screen) points it UP.
    const Point doorEnd{520, 440};
    CHECK_POINT(xform::transformPoint(Mat3::rotationAbout(-90, pivot), doorEnd), 440, 360, 1e-3);
    CHECK_POINT(xform::transformPoint(Mat3::rotationAbout(90, pivot), doorEnd), 440, 520, 1e-3);
    CHECK_POINT(xform::transformPoint(Mat3::rotationAbout(180, pivot), doorEnd), 360, 440, 1e-3);

    // rotationAbout is exactly the composite T(+P) * R * T(-P).
    const Mat3 composite = Mat3::translation(pivot.x, pivot.y) * Mat3::rotation(37) *
                           Mat3::translation(-pivot.x, -pivot.y);
    checkMatricesNear(Mat3::rotationAbout(37, pivot), composite, 0);

    // Rotating about a different pivot moves the shape somewhere else.
    const Point aboutOrigin = xform::transformPoint(Mat3::rotation(90), doorEnd);
    CHECK_POINT(aboutOrigin, -440, 520, 1e-3);

    // scalingAbout keeps the pivot fixed and doubles distances from it.
    const Mat3 s = Mat3::scalingAbout(2, 3, pivot);
    CHECK_POINT(xform::transformPoint(s, pivot), 440, 440, 1e-4);
    CHECK_POINT(xform::transformPoint(s, {450, 450}), 460, 470, 1e-4);

    // Mirror in a vertical line x = 320 (the middle of the room), built the
    // same way: move the line onto the y axis, reflect, move back.
    const Mat3 mirror = Mat3::translation(320, 0) * Mat3::reflectionAcrossYAxis() * Mat3::translation(-320, 0);
    CHECK_POINT(xform::transformPoint(mirror, {580, 60}), 60, 60, 1e-4);
    CHECK_POINT(xform::transformPoint(mirror, {320, 99}), 320, 99, 1e-4); // on the line: fixed
}

// What each transform does to area and orientation.
void testAreaAndDeterminant() {
    const Polygon shape = makeTestShape();
    const float area = raster::computeArea(shape);
    const float signedArea = raster::computeSignedArea(shape);
    CHECK(area > 0.0f);

    // Reflecting twice gives the identity.
    checkMatricesNear(Mat3::reflectionAcrossXAxis() * Mat3::reflectionAcrossXAxis(), Mat3::identity(), 0);
    checkMatricesNear(Mat3::reflectionAcrossYAxis() * Mat3::reflectionAcrossYAxis(), Mat3::identity(), 0);
    // Reflecting in both axes is a half turn.
    checkMatricesNear(Mat3::reflectionAcrossXAxis() * Mat3::reflectionAcrossYAxis(), Mat3::rotation(180), 1e-6);

    // A reflection keeps the size but flips the winding (a mirror image).
    CHECK_NEAR(xform::determinant(Mat3::reflectionAcrossYAxis()), -1.0, 0);
    const Polygon mirrored = xform::transformPolygon(Mat3::reflectionAcrossYAxis(), shape);
    CHECK_NEAR(raster::computeSignedArea(mirrored), -signedArea, 1e-2);

    // Shear keeps the area: determinant 1.
    for (const float k : {0.25f, 0.6f, -1.5f, 3.0f}) {
        CHECK_NEAR(xform::determinant(Mat3::shear(k, 0)), 1.0, 0);
        CHECK_NEAR(xform::determinant(Mat3::shear(0, k)), 1.0, 0);
        const Polygon sheared = xform::transformPolygon(Mat3::shear(k, 0), shape);
        CHECK_NEAR(raster::computeArea(sheared), area, area * 1e-5);
    }
    // ...but it does change the shape: a right angle is no longer right.
    const Polygon square = raster::makeRectangle(0, 0, 10, 10);
    const Polygon slanted = xform::transformPolygon(Mat3::shear(0.5f, 0), square);
    CHECK_POINT(slanted.vertices[2], 15, 10, 0); // bottom-right corner pushed right
    CHECK_POINT(slanted.vertices[3], 5, 10, 0);  // bottom-left corner pushed right

    // Scaling multiplies the area by sx * sy.
    const float scales[][2] = {{2, 2}, {3, 0.5f}, {0.1f, 0.1f}, {1, 4}};
    for (const auto& s : scales) {
        const Mat3 m = Mat3::scalingAbout(s[0], s[1], {310, 110});
        CHECK_NEAR(xform::determinant(m), s[0] * s[1], 1e-5);
        CHECK_NEAR(raster::computeArea(xform::transformPolygon(m, shape)), area * s[0] * s[1], area * 1e-4);
    }

    // Rotation and translation keep area and winding: determinant 1.
    CHECK_NEAR(xform::determinant(Mat3::rotation(73)), 1.0, 1e-6);
    CHECK_NEAR(xform::determinant(Mat3::translation(500, -300)), 1.0, 0);
    const Polygon moved = xform::transformPolygon(Mat3::rotationAbout(73, {40, 40}), shape);
    CHECK_NEAR(raster::computeSignedArea(moved), signedArea, area * 1e-4);

    // det(A * B) = det(A) * det(B).
    const Mat3 a = Mat3::scaling(2, 3);
    const Mat3 b = Mat3::shear(0.5f, 0.25f); // det = 1 - 0.125
    CHECK_NEAR(xform::determinant(b), 0.875, 1e-6);
    CHECK_NEAR(xform::determinant(a * b), 6.0 * 0.875, 1e-5);
}

void testInverse() {
    const Polygon shape = makeTestShape();

    const Mat3 composites[] = {
        Mat3::identity(),
        Mat3::translation(120, -45),
        Mat3::rotation(33),
        Mat3::scaling(2, 0.5f),
        Mat3::shear(0.6f, 0),
        Mat3::reflectionAcrossYAxis(),
        Mat3::rotationAbout(-70, {440, 440}),
        Mat3::translation(300, 200) * Mat3::rotation(125) * Mat3::scaling(1.5f, 0.75f),
        Mat3::scalingAbout(0.25f, 3, {40, 40}) * Mat3::shear(0.2f, 0.4f) * Mat3::rotation(10),
        Mat3::translation(320, 0) * Mat3::reflectionAcrossYAxis() * Mat3::translation(-320, 0),
    };

    for (const Mat3& m : composites) {
        Mat3 inv = Mat3::identity();
        if (!CHECK(xform::inverse(m, inv))) {
            continue;
        }
        // Both products are the identity. The translation column holds
        // values in the hundreds, so it gets a looser tolerance.
        checkMatricesNear(m * inv, Mat3::identity(), 2e-3);
        checkMatricesNear(inv * m, Mat3::identity(), 2e-3);
        // Transforming and transforming back returns every vertex. This is
        // how a click on screen is turned back into the piece's own
        // coordinates.
        checkPolygonsNear(xform::transformPolygon(inv, xform::transformPolygon(m, shape)), shape, 5e-3);
        // det(inverse) = 1 / det.
        CHECK_NEAR(xform::determinant(inv) * xform::determinant(m), 1.0, 1e-4);
    }

    // Known inverses.
    Mat3 inv = Mat3::identity();
    CHECK(xform::inverse(Mat3::translation(10, -5), inv));
    checkMatricesNear(inv, Mat3::translation(-10, 5), 1e-6);
    CHECK(xform::inverse(Mat3::rotation(40), inv));
    checkMatricesNear(inv, Mat3::rotation(-40), 1e-6);
    CHECK(xform::inverse(Mat3::scaling(4, 0.5f), inv));
    checkMatricesNear(inv, Mat3::scaling(0.25f, 2), 1e-6);
    CHECK(xform::inverse(Mat3::shear(0.5f, 0), inv));
    checkMatricesNear(inv, Mat3::shear(-0.5f, 0), 1e-6);

    // inverse(A * B) = inverse(B) * inverse(A): undo the LAST step first.
    const Mat3 a = Mat3::translation(50, 60);
    const Mat3 b = Mat3::rotation(30);
    Mat3 invA = Mat3::identity();
    Mat3 invB = Mat3::identity();
    Mat3 invAB = Mat3::identity();
    CHECK(xform::inverse(a, invA));
    CHECK(xform::inverse(b, invB));
    CHECK(xform::inverse(a * b, invAB));
    checkMatricesNear(invAB, invB * invA, 1e-4);

    // Singular matrices cannot be inverted, and `out` is left untouched.
    const Mat3 marker = Mat3::translation(1, 2);
    Mat3 out = marker;
    CHECK(!xform::inverse(Mat3::scaling(0, 1), out));
    CHECK(!xform::inverse(Mat3::scaling(1, 0), out));
    CHECK(!xform::inverse(Mat3::scaling(0, 0), out));
    CHECK(!xform::inverse(Mat3::shear(1, 1), out)); // det = 1 - 1*1 = 0
    CHECK(!xform::inverse(Mat3::rotation(45) * Mat3::scaling(0, 1) * Mat3::translation(5, 5), out));
    checkMatricesNear(out, marker, 0);
    CHECK_NEAR(xform::determinant(Mat3::scaling(0, 1)), 0.0, 0);

    // A very small but real scale is still invertible.
    CHECK(xform::inverse(Mat3::scaling(0.001f, 0.001f), out));
    CHECK_NEAR(out.m[0][0], 1000.0, 1e-2);
}

void testTransformPolygon() {
    // Empty polygon stays empty.
    CHECK(xform::transformPolygon(Mat3::rotation(45), Polygon{}).vertices.empty());

    // Vertex count and order are kept, and the input is not modified.
    const Polygon shape = makeTestShape();
    const Polygon copy = shape;
    const Polygon moved = xform::transformPolygon(Mat3::translation(1, 2), shape);
    CHECK(moved.vertices.size() == shape.vertices.size());
    for (std::size_t i = 0; i < shape.vertices.size(); ++i) {
        CHECK(moved.vertices[i].x == shape.vertices[i].x + 1.0f);
        CHECK(moved.vertices[i].y == shape.vertices[i].y + 2.0f);
        CHECK(shape.vertices[i].x == copy.vertices[i].x && shape.vertices[i].y == copy.vertices[i].y);
    }

    // The identity returns exactly the same floats (needed so that furniture
    // with the default pose behaves exactly as in Checkpoint 1).
    const Polygon same = xform::transformPolygon(Mat3::identity(), shape);
    for (std::size_t i = 0; i < shape.vertices.size(); ++i) {
        CHECK(same.vertices[i].x == shape.vertices[i].x && same.vertices[i].y == shape.vertices[i].y);
    }

    // 360 one-degree steps as ONE accumulated matrix bring the shape back.
    Mat3 accumulated = Mat3::identity();
    const Mat3 step = Mat3::rotationAbout(1, {310, 110});
    for (int i = 0; i < 360; ++i) {
        accumulated = step * accumulated;
    }
    checkPolygonsNear(xform::transformPolygon(accumulated, shape), shape, 0.05);

    // Homogeneous divide: a matrix whose bottom-right entry is 2 represents
    // the same transform as the matrix divided by 2.
    Mat3 doubled = Mat3::translation(10, 20);
    for (auto& row : doubled.m) {
        for (float& value : row) {
            value *= 2.0f;
        }
    }
    CHECK_POINT(xform::transformPoint(doubled, {3, 4}), 13, 24, 1e-5);
}

} // namespace

int main() {
    testBasicMatrices();
    testAgreesWithCheckpoint1();
    testOrderMatters();
    testPivotTransforms();
    testAreaAndDeterminant();
    testInverse();
    testTransformPolygon();
    return test::summary("test_mat3");
}
