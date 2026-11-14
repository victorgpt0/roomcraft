// Tests for the view library: 2D camera, Mat4 and the 3D orbit camera.
//
// Every expected value below is worked out by hand from the definitions in
// src/view/ (the working is in the comments next to each check).

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "view/camera2d.h"
#include "view/camera3d.h"
#include "view/mat4.h"
#include "xform/mat3.h"

#include "test_helpers.h"

#include <cmath>
#include <string>
#include <vector>

using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Rect;
using view::Camera2D;
using view::Mat4;
using view::OrbitCamera;
using xform::Vec3;
using xform::Vec4;

namespace {

#define CHECK_POINT(actual, ex, ey, tolerance)          \
    do {                                                 \
        const raster::Point checkedPoint = (actual);     \
        CHECK_NEAR(checkedPoint.x, (ex), (tolerance));   \
        CHECK_NEAR(checkedPoint.y, (ey), (tolerance));   \
    } while (false)

#define CHECK_VEC3(actual, ex, ey, ez, tolerance)        \
    do {                                                 \
        const xform::Vec3 checkedVec = (actual);         \
        CHECK_NEAR(checkedVec.x, (ex), (tolerance));     \
        CHECK_NEAR(checkedVec.y, (ey), (tolerance));     \
        CHECK_NEAR(checkedVec.z, (ez), (tolerance));     \
    } while (false)

const Color kWhite{255, 255, 255};
const Color kRed{220, 30, 30};
const Color kBlue{30, 30, 220};

bool sameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

Canvas whiteCanvas(int width, int height) {
    Canvas canvas(width, height);
    canvas.clear(kWhite.r, kWhite.g, kWhite.b);
    return canvas;
}

int countPixels(const Canvas& canvas, Color color) {
    int count = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            if (sameColor(canvas.getPixel(x, y), color)) {
                ++count;
            }
        }
    }
    return count;
}

int countNonWhiteOutside(const Canvas& canvas, const Rect& viewport) {
    int count = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const bool inside = x >= viewport.xmin && x < viewport.xmax &&
                                y >= viewport.ymin && y < viewport.ymax;
            if (!inside && !sameColor(canvas.getPixel(x, y), kWhite)) {
                ++count;
            }
        }
    }
    return count;
}

Vec3 project(const Mat4& m, Vec3 p) {
    const Vec4 c = m * Vec4{p.x, p.y, p.z, 1.0f};
    return Vec3{c.x / c.w, c.y / c.w, c.z / c.w};
}

// ---- 2D camera -------------------------------------------------------------

// Window 400 x 300 cm starting at (100, 50); viewport 600 x 400 px at (20, 40).
//   sx = 600 / 400 = 1.5,   sy = 400 / 300 = 1.333...
//   x' = 20 + (x - 100) * 1.5,   y' = 40 + (y - 50) * 4/3
Camera2D testCamera() {
    return Camera2D{Rect{100, 50, 500, 350}, Rect{20, 40, 620, 440}};
}

void testWindowToViewport() {
    const Camera2D camera = testCamera();
    const xform::Mat3 m = view::windowToViewport(camera);

    // The four window corners land on the four viewport corners.
    CHECK_POINT(xform::transformPoint(m, {100, 50}), 20, 40, 1e-4);
    CHECK_POINT(xform::transformPoint(m, {500, 50}), 620, 40, 1e-4);
    CHECK_POINT(xform::transformPoint(m, {500, 350}), 620, 440, 1e-4);
    CHECK_POINT(xform::transformPoint(m, {100, 350}), 20, 440, 1e-4);
    // The centre on the centre.
    CHECK_POINT(xform::transformPoint(m, {300, 200}), 320, 240, 1e-4);
    // A point 25% across and 10% down the window: 25% / 10% of the viewport.
    CHECK_POINT(xform::transformPoint(m, {200, 80}), 20 + 150, 40 + 40, 1e-4);

    // The matrix entries are the ones derived in camera2d.cpp.
    CHECK_NEAR(m.m[0][0], 1.5, 1e-6);
    CHECK_NEAR(m.m[1][1], 400.0 / 300.0, 1e-6);
    CHECK_NEAR(m.m[0][2], 20 - 1.5 * 100, 1e-4);         // vxmin - sx * wxmin
    CHECK_NEAR(m.m[1][2], 40 - (400.0 / 300.0) * 50, 1e-4);
    CHECK_NEAR(m.m[0][1], 0.0, 1e-9);
    CHECK_NEAR(m.m[2][2], 1.0, 1e-9);

    CHECK_NEAR(view::scaleX(camera), 1.5, 1e-6);
    CHECK_NEAR(view::scaleY(camera), 400.0 / 300.0, 1e-6);
    CHECK_POINT(view::roomToScreen(camera, {300, 200}), 320, 240, 1e-4);

    // Window == viewport: the identity, so CP1's 1 pixel = 1 cm is a special case.
    const Camera2D unit{Rect{0, 0, 640, 480}, Rect{0, 0, 640, 480}};
    CHECK_POINT(view::roomToScreen(unit, {123.5f, 77.25f}), 123.5, 77.25, 1e-4);

    // A degenerate window gives the identity instead of dividing by zero.
    const Camera2D flat{Rect{10, 10, 10, 50}, Rect{0, 0, 100, 100}};
    const xform::Mat3 id = view::windowToViewport(flat);
    CHECK_POINT(xform::transformPoint(id, {7, 9}), 7, 9, 1e-6);
    Point unused{};
    CHECK(!view::screenToRoom(flat, {5, 5}, unused));
}

void testPan() {
    // Scale 2: window 400 x 300 shown in an 800 x 600 viewport.
    Camera2D camera{Rect{0, 0, 400, 300}, Rect{0, 0, 800, 600}};
    const Point p{150, 100};
    CHECK_POINT(view::roomToScreen(camera, p), 300, 200, 1e-4);

    // Pan the window 30 cm right and 20 cm up: the room slides 60 px LEFT
    // and 40 px DOWN on screen.
    view::pan(camera, 30, -20);
    CHECK_NEAR(camera.window.xmin, 30, 1e-6);
    CHECK_NEAR(camera.window.xmax, 430, 1e-6);
    CHECK_NEAR(camera.window.ymin, -20, 1e-6);
    CHECK_NEAR(camera.window.ymax, 280, 1e-6);
    CHECK_POINT(view::roomToScreen(camera, p), 300 - 60, 200 + 40, 1e-4);
    // Panning does not change the zoom.
    CHECK_NEAR(view::scaleX(camera), 2.0, 1e-6);
    CHECK_NEAR(view::scaleY(camera), 2.0, 1e-6);
    // Panning back undoes it.
    view::pan(camera, -30, 20);
    CHECK_POINT(view::roomToScreen(camera, p), 300, 200, 1e-4);
}

void testZoomAbout() {
    Camera2D camera{Rect{0, 0, 400, 300}, Rect{0, 0, 800, 600}};
    const Point anchor{150, 100};
    const Point other{250, 100}; // 100 cm right of the anchor = 200 px at scale 2
    const Point anchorBefore = view::roomToScreen(camera, anchor);

    view::zoomAbout(camera, 2.0f, anchor);
    // The anchor has not moved on screen...
    CHECK_POINT(view::roomToScreen(camera, anchor), anchorBefore.x, anchorBefore.y, 1e-3);
    // ...the window is half the size, so the scale doubled...
    CHECK_NEAR(camera.window.xmax - camera.window.xmin, 200, 1e-4);
    CHECK_NEAR(camera.window.ymax - camera.window.ymin, 150, 1e-4);
    CHECK_NEAR(view::scaleX(camera), 4.0, 1e-5);
    // ...and other points moved away from the anchor: 200 px became 400 px.
    const Point o = view::roomToScreen(camera, other);
    CHECK_NEAR(o.x - anchorBefore.x, 400, 1e-3);
    CHECK_NEAR(o.y - anchorBefore.y, 0, 1e-3);

    // Zooming out about a DIFFERENT anchor keeps that one still instead.
    const Point anchor2{90, 230};
    const Point before2 = view::roomToScreen(camera, anchor2);
    view::zoomAbout(camera, 0.25f, anchor2);
    CHECK_POINT(view::roomToScreen(camera, anchor2), before2.x, before2.y, 1e-3);
    CHECK_NEAR(view::scaleX(camera), 1.0, 1e-5);

    // Zoom in and out by the same factor about the same point: back to start.
    Camera2D c{Rect{0, 0, 400, 300}, Rect{0, 0, 800, 600}};
    view::zoomAbout(c, 3.0f, anchor);
    view::zoomAbout(c, 1.0f / 3.0f, anchor);
    CHECK_NEAR(c.window.xmin, 0, 1e-3);
    CHECK_NEAR(c.window.ymin, 0, 1e-3);
    CHECK_NEAR(c.window.xmax, 400, 1e-3);
    CHECK_NEAR(c.window.ymax, 300, 1e-3);

    // A factor <= 0 is ignored.
    view::zoomAbout(c, 0.0f, anchor);
    view::zoomAbout(c, -2.0f, anchor);
    CHECK_NEAR(c.window.xmax - c.window.xmin, 400, 1e-3);
}

void testMatchViewportAspect() {
    // Square window in a 2:1 viewport: sx = 2, sy = 1 (circles squashed).
    // The window is widened about its centre (200) to 800 wide.
    Camera2D wide{Rect{0, 0, 400, 400}, Rect{0, 0, 800, 400}};
    CHECK(view::scaleX(wide) != view::scaleY(wide));
    view::matchViewportAspect(wide);
    CHECK_NEAR(wide.window.xmin, -200, 1e-3);
    CHECK_NEAR(wide.window.xmax, 600, 1e-3);
    CHECK_NEAR(wide.window.ymin, 0, 1e-3);
    CHECK_NEAR(wide.window.ymax, 400, 1e-3);
    CHECK_NEAR(view::scaleX(wide), view::scaleY(wide), 1e-5);

    // The CP1 room (560 x 400) in a square viewport: made taller, 560 x 560,
    // still centred on y = 240, and the whole room is still inside.
    Camera2D room{Rect{40, 40, 600, 440}, Rect{0, 0, 300, 300}};
    view::matchViewportAspect(room);
    CHECK_NEAR(room.window.xmin, 40, 1e-3);
    CHECK_NEAR(room.window.xmax, 600, 1e-3);
    CHECK_NEAR(room.window.ymin, 240 - 280, 1e-3);
    CHECK_NEAR(room.window.ymax, 240 + 280, 1e-3);
    CHECK_NEAR(view::scaleX(room), view::scaleY(room), 1e-5);
}

void testScreenToRoom() {
    // Round trip room -> screen -> room through the inverse matrix, for a
    // camera that has been zoomed and panned (a non-trivial matrix).
    Camera2D camera = testCamera();
    view::zoomAbout(camera, 2.5f, {320, 180});
    view::pan(camera, -37, 12);
    const Point samples[] = {{0, 0}, {100, 50}, {275.5f, 75.25f}, {600, 440}, {-80, 900}};
    for (const Point& p : samples) {
        Point back{};
        CHECK(view::screenToRoom(camera, view::roomToScreen(camera, p), back));
        CHECK_NEAR(back.x, p.x, 1e-2);
        CHECK_NEAR(back.y, p.y, 1e-2);
    }

    // The other way: a mouse click at a pixel maps back to the room point
    // that is drawn at that pixel.
    Point room{};
    CHECK(view::screenToRoom(testCamera(), {320, 240}, room));
    CHECK_POINT(room, 300, 200, 1e-3);

    // A click on the zoomed view selects the piece under the mouse.
    std::vector<roomcraft::Furniture> pieces;
    pieces.push_back({"Bed", raster::makeRectangle(60, 60, 140, 200), kBlue, {}});
    pieces.push_back({"Nightstand", raster::makeRectangle(208, 60, 36, 36), kRed, {}});
    Camera2D zoomed{Rect{180, 40, 280, 115}, Rect{0, 0, 640, 480}}; // 6.4x
    const Point nightstandOnScreen = view::roomToScreen(zoomed, {226, 78});
    CHECK(view::screenToRoom(zoomed, nightstandOnScreen, room));
    CHECK(roomcraft::findFurnitureAt(pieces, room) == 1);
    // The left edge of the screen is x = 180 in the room: on the bed.
    CHECK(view::screenToRoom(zoomed, {0, 240}, room));
    CHECK_NEAR(room.x, 180, 1e-3);
    CHECK(roomcraft::findFurnitureAt(pieces, room) == 0);
}

void testDrawThroughCamera() {
    // A huge red piece seen through a window that lies completely inside it,
    // at scale 1: the viewport is filled exactly (half-open rule: 100 x 70
    // pixels) and nothing outside it is touched. No outline appears at the
    // viewport border, because the piece's real edges are all off screen.
    {
        Canvas canvas = whiteCanvas(200, 150);
        const Rect viewport{50, 40, 150, 110};
        const Camera2D camera{Rect{100, 100, 200, 170}, viewport};
        std::vector<roomcraft::Furniture> pieces;
        pieces.push_back({"Floor", raster::makeRectangle(0, 0, 1000, 1000), kRed, {}});
        view::drawFurnitureThroughCamera(canvas, pieces, camera);
        CHECK(countPixels(canvas, kRed) == 100 * 70);
        CHECK(countNonWhiteOutside(canvas, viewport) == 0);
        CHECK(sameColor(canvas.getPixel(50, 40), kRed));    // first viewport pixel
        CHECK(sameColor(canvas.getPixel(149, 109), kRed));  // last viewport pixel
        CHECK(sameColor(canvas.getPixel(150, 109), kWhite));
        CHECK(sameColor(canvas.getPixel(49, 40), kWhite));
    }

    // A concave L-shaped piece cut by the viewport edge, zoomed 3x and
    // rotated by its pose: still nothing outside the viewport, and the
    // outline of its visible edges is drawn.
    {
        Canvas canvas = whiteCanvas(240, 180);
        const Rect viewport{30, 30, 210, 150};
        Camera2D camera{Rect{0, 0, 180, 120}, viewport};
        view::zoomAbout(camera, 3.0f, {90, 60});
        roomcraft::Furniture sofa{"L-shaped sofa", {}, kRed, {}};
        sofa.footprint.vertices = {{-100, -25}, {100, -25}, {100, 55}, {75, 55}, {75, 0}, {-100, 0}};
        sofa.pose.position = {90, 60};
        sofa.pose.angleDegrees = 20.0f;
        view::drawFurnitureThroughCamera(canvas, {sofa}, camera);
        CHECK(countNonWhiteOutside(canvas, viewport) == 0);
        CHECK(countPixels(canvas, kRed) > 0);
        CHECK(countPixels(canvas, Color{40, 40, 40}) > 0);
    }

    // A line through the camera that starts and ends far outside the window
    // is clipped by Cohen-Sutherland to the viewport's pixels exactly.
    {
        Canvas canvas = whiteCanvas(200, 150);
        const Rect viewport{50, 40, 150, 110};
        const Camera2D camera{Rect{0, 0, 100, 70}, viewport};
        view::drawLineThroughCamera(canvas, {-500, 35}, {900, 35}, camera, kBlue);
        CHECK(countPixels(canvas, kBlue) == 100); // columns 50..149 of row 75
        CHECK(sameColor(canvas.getPixel(50, 75), kBlue));
        CHECK(sameColor(canvas.getPixel(149, 75), kBlue));
        CHECK(countNonWhiteOutside(canvas, viewport) == 0);
    }
}

// ---- Mat4 ------------------------------------------------------------------

void testMat4Basics() {
    const Vec4 p{1, 2, 3, 1};

    const Vec4 same = Mat4::identity() * p;
    CHECK_NEAR(same.x, 1, 1e-6);
    CHECK_NEAR(same.y, 2, 1e-6);
    CHECK_NEAR(same.z, 3, 1e-6);
    CHECK_NEAR(same.w, 1, 1e-6);

    const Vec4 moved = Mat4::translation(10, 20, 30) * p;
    CHECK_NEAR(moved.x, 11, 1e-6);
    CHECK_NEAR(moved.y, 22, 1e-6);
    CHECK_NEAR(moved.z, 33, 1e-6);
    CHECK_NEAR(moved.w, 1, 1e-6);
    // A DIRECTION (w = 0) is not moved by a translation.
    const Vec4 direction = Mat4::translation(10, 20, 30) * Vec4{1, 2, 3, 0};
    CHECK_NEAR(direction.x, 1, 1e-6);
    CHECK_NEAR(direction.z, 3, 1e-6);

    CHECK_VEC3(view::transformPoint(Mat4::scaling(2, 3, 4), {1, 1, 1}), 2, 3, 4, 1e-6);
    CHECK_VEC3(view::transformPoint(Mat4::scaling(2, 3, 4), {-5, 0, 0.5f}), -10, 0, 2, 1e-6);

    // Right-handed rotations by 90 degrees, by the cyclic rule x -> y -> z -> x.
    CHECK_VEC3(view::transformPoint(Mat4::rotationX(90), {0, 1, 0}), 0, 0, 1, 1e-6);
    CHECK_VEC3(view::transformPoint(Mat4::rotationX(90), {0, 0, 1}), 0, -1, 0, 1e-6);
    CHECK_VEC3(view::transformPoint(Mat4::rotationZ(90), {1, 0, 0}), 0, 1, 0, 1e-6);
    CHECK_VEC3(view::transformPoint(Mat4::rotationZ(90), {0, 1, 0}), -1, 0, 0, 1e-6);
    // The axis itself never moves.
    CHECK_VEC3(view::transformPoint(Mat4::rotationX(37), {5, 0, 0}), 5, 0, 0, 1e-5);
    CHECK_VEC3(view::transformPoint(Mat4::rotationZ(37), {0, 0, 5}), 0, 0, 5, 1e-5);
}

void testRotationY() {
    // rotationY(90): z turns towards x, x turns towards -z; y is unchanged.
    const Mat4 r = Mat4::rotationY(90);
    CHECK_VEC3(view::transformPoint(r, {1, 0, 0}), 0, 0, -1, 1e-6);
    CHECK_VEC3(view::transformPoint(r, {0, 0, 1}), 1, 0, 0, 1e-6);
    CHECK_VEC3(view::transformPoint(r, {0, 1, 0}), 0, 1, 0, 1e-6);
    CHECK_VEC3(view::transformPoint(r, {3, 7, 4}), 4, 7, -3, 1e-5);
    // 45 degrees: (1, 0, 0) -> (cos 45, 0, -sin 45).
    const float h = std::sqrt(0.5f);
    CHECK_VEC3(view::transformPoint(Mat4::rotationY(45), {1, 0, 0}), h, 0, -h, 1e-6);

    // On the floor plan (X = plan x, Z = plan y), rotationY(-a) turns a piece
    // the same way as the 2D Mat3::rotation(a): clockwise on screen.
    for (float angle : {30.0f, 90.0f, 135.0f, -60.0f}) {
        const Point plan = xform::transformPoint(xform::Mat3::rotation(angle), {100, 40});
        const Vec3 world = view::transformPoint(Mat4::rotationY(-angle), {100, 0, 40});
        CHECK_NEAR(world.x, plan.x, 1e-3);
        CHECK_NEAR(world.z, plan.y, 1e-3);
        CHECK_NEAR(world.y, 0, 1e-6);
    }
}

void testMat4Products() {
    // Order matters: scale-then-move versus move-then-scale.
    const Mat4 t = Mat4::translation(10, 0, 0);
    const Mat4 s = Mat4::scaling(2, 2, 2);
    CHECK_VEC3(view::transformPoint(t * s, {1, 0, 0}), 12, 0, 0, 1e-6); // (1*2) + 10
    CHECK_VEC3(view::transformPoint(s * t, {1, 0, 0}), 22, 0, 0, 1e-6); // (1+10) * 2

    // (A * B) * p == A * (B * p) for a general chain.
    const Mat4 a = Mat4::rotationX(25) * Mat4::translation(3, -4, 5);
    const Mat4 b = Mat4::rotationY(-70) * Mat4::scaling(1.5f, 0.5f, 2);
    const Vec3 q{7, -2, 9};
    const Vec3 lhs = view::transformPoint(a * b, q);
    const Vec3 rhs = view::transformPoint(a, view::transformPoint(b, q));
    CHECK_VEC3(lhs, rhs.x, rhs.y, rhs.z, 1e-4);

    // Rotations about the same axis add; four quarter turns are the identity.
    const Mat4 sum = Mat4::rotationY(30) * Mat4::rotationY(50);
    const Mat4 direct = Mat4::rotationY(80);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK_NEAR(sum.m[i][j], direct.m[i][j], 1e-6);
        }
    }
    const Mat4 q4 = Mat4::rotationZ(90) * Mat4::rotationZ(90) * Mat4::rotationZ(90) * Mat4::rotationZ(90);
    const Mat4 id = Mat4::identity();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK_NEAR(q4.m[i][j], id.m[i][j], 1e-6);
        }
    }

    // Identity is neutral on both sides.
    const Mat4 left = Mat4::identity() * a;
    const Mat4 right = a * Mat4::identity();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK_NEAR(left.m[i][j], a.m[i][j], 1e-6);
            CHECK_NEAR(right.m[i][j], a.m[i][j], 1e-6);
        }
    }
}

void testLookAt() {
    // Simplest case: camera on +z looking at the origin. The view is just a
    // translation by -5 along z.
    const Mat4 simple = Mat4::lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    CHECK_VEC3(view::transformPoint(simple, {1, 2, 0}), 1, 2, -5, 1e-6);

    // A camera in a corner of the room above the floor, looking at the
    // middle of the room.
    const Vec3 eye{650, 220, 600};
    const Vec3 target{320, 40, 240};
    const Mat4 v = Mat4::lookAt(eye, target, {0, 1, 0});
    const float distance = view::length(view::subtract(target, eye));

    CHECK_VEC3(view::transformPoint(v, eye), 0, 0, 0, 1e-3);           // eye -> origin
    CHECK_VEC3(view::transformPoint(v, target), 0, 0, -distance, 1e-2); // target -> -z axis

    // The rows of the rotation part are three perpendicular unit vectors.
    const Vec3 r{v.m[0][0], v.m[0][1], v.m[0][2]};
    const Vec3 u{v.m[1][0], v.m[1][1], v.m[1][2]};
    const Vec3 b{v.m[2][0], v.m[2][1], v.m[2][2]};
    CHECK_NEAR(view::length(r), 1, 1e-5);
    CHECK_NEAR(view::length(u), 1, 1e-5);
    CHECK_NEAR(view::length(b), 1, 1e-5);
    CHECK_NEAR(view::dot(r, u), 0, 1e-5);
    CHECK_NEAR(view::dot(r, b), 0, 1e-5);
    CHECK_NEAR(view::dot(u, b), 0, 1e-5);
    // "Right" is level (no y component) and "up" points upwards.
    CHECK_NEAR(r.y, 0, 1e-5);
    CHECK(u.y > 0);

    // A point straight above the target appears above the centre (y > 0);
    // distances are preserved (the view only moves and turns).
    const Vec3 above = view::transformPoint(v, {320, 140, 240});
    CHECK(above.y > 0);
    CHECK_NEAR(view::length(above), view::length(view::subtract({320, 140, 240}, eye)), 1e-2);
}

void testPerspective() {
    const float n = 10;
    const float f = 1000;
    const Mat4 p = Mat4::perspective(90, 1, n, f); // tan(45) = 1, so 1/t = 1

    // Points on the near and far planes get depth -1 and +1.
    CHECK_NEAR(project(p, {0, 0, -n}).z, -1, 1e-5);
    CHECK_NEAR(project(p, {0, 0, -f}).z, 1, 1e-4);
    CHECK_NEAR(project(p, {30, -20, -n}).z, -1, 1e-5); // anywhere on the plane
    CHECK_NEAR(project(p, {300, 250, -f}).z, 1, 1e-4);
    // In between, depth is between -1 and +1 and grows with distance.
    const float zMid = project(p, {0, 0, -100}).z;
    CHECK(zMid > -1 && zMid < 1);
    CHECK(project(p, {0, 0, -200}).z > zMid);

    // w is the distance in front of the camera.
    const Vec4 c = p * Vec4{5, 5, -250, 1};
    CHECK_NEAR(c.w, 250, 1e-4);

    // Twice as far away -> half as far from the screen centre.
    const Vec3 near1 = project(p, {40, 30, -100});
    const Vec3 far2 = project(p, {40, 30, -200});
    CHECK_NEAR(near1.x, 0.4, 1e-5);   // 40 / 100
    CHECK_NEAR(near1.y, 0.3, 1e-5);
    CHECK_NEAR(far2.x, near1.x / 2, 1e-5);
    CHECK_NEAR(far2.y, near1.y / 2, 1e-5);

    // The edge of the field of view is the edge of the screen. With fov 60
    // and aspect 2: top edge at y = d tan(30), right edge at x = 2 d tan(30).
    const Mat4 p2 = Mat4::perspective(60, 2, 1, 100);
    const float t30 = static_cast<float>(std::tan(raster::kPi / 6));
    CHECK_NEAR(project(p2, {0, 50 * t30, -50}).y, 1, 1e-5);
    CHECK_NEAR(project(p2, {2 * 50 * t30, 0, -50}).x, 1, 1e-5);
}

void testOrthographic() {
    const Mat4 o = Mat4::orthographic(-100, 100, -50, 50, 10, 1000);
    // The corners of the box land on the corners of the cube.
    CHECK_VEC3(project(o, {100, 50, -10}), 1, 1, -1, 1e-5);
    CHECK_VEC3(project(o, {-100, -50, -1000}), -1, -1, 1, 1e-5);
    // w stays 1.
    CHECK_NEAR((o * Vec4{7, 8, -9, 1}).w, 1, 1e-6);
    // Distance does NOT change the size: the same x at two depths.
    CHECK_NEAR(project(o, {50, 0, -20}).x, 0.5, 1e-6);
    CHECK_NEAR(project(o, {50, 0, -900}).x, 0.5, 1e-6);
    // An off-centre box: its middle maps to the middle of the cube.
    const Mat4 off = Mat4::orthographic(40, 600, 40, 440, 1, 3);
    CHECK_VEC3(project(off, {320, 240, -2}), 0, 0, 0, 1e-5);
}

// ---- Orbit camera ----------------------------------------------------------

void testEyePosition() {
    OrbitCamera camera;
    camera.target = {300, 0, 200};
    camera.radius = 500;

    camera.azimuthDegrees = 0;
    camera.elevationDegrees = 0;
    CHECK_VEC3(view::eyePosition(camera), 300, 0, 700, 1e-3);  // on the +Z side
    camera.azimuthDegrees = 90;
    CHECK_VEC3(view::eyePosition(camera), 800, 0, 200, 1e-3);  // on the +X side
    camera.azimuthDegrees = 180;
    CHECK_VEC3(view::eyePosition(camera), 300, 0, -300, 1e-3);
    // Elevation 90: straight above the target, whatever the azimuth.
    camera.elevationDegrees = 90;
    for (float az : {0.0f, 45.0f, 200.0f}) {
        camera.azimuthDegrees = az;
        CHECK_VEC3(view::eyePosition(camera), 300, 500, 200, 1e-3);
    }
    // Elevation 30, azimuth 0: height r sin 30 = 250, floor distance
    // r cos 30 = 433.0 along +Z.
    camera.azimuthDegrees = 0;
    camera.elevationDegrees = 30;
    CHECK_VEC3(view::eyePosition(camera), 300, 250, 200 + 500 * std::sqrt(3.0f) / 2, 1e-2);
    // Elevation 30, azimuth 45: the floor distance 433.0 is split equally.
    camera.azimuthDegrees = 45;
    const float split = 500 * std::sqrt(3.0f) / 2 * std::sqrt(0.5f);
    CHECK_VEC3(view::eyePosition(camera), 300 + split, 250, 200 + split, 1e-2);

    // The eye is always `radius` from the target, and the up vector is a
    // unit vector perpendicular to the line of sight.
    for (float az = 0; az < 360; az += 15) {
        for (float el = -60; el <= 90; el += 15) {
            camera.azimuthDegrees = az;
            camera.elevationDegrees = el;
            const Vec3 offset = view::subtract(view::eyePosition(camera), camera.target);
            CHECK_NEAR(view::length(offset), 500, 1e-2);
            const Vec3 up = view::upVector(camera);
            CHECK_NEAR(view::length(up), 1, 1e-5);
            CHECK_NEAR(view::dot(up, offset), 0, 1e-2);
        }
    }
}

void testViewMatrix() {
    OrbitCamera camera;
    camera.target = {320, 50, 240};
    camera.radius = 900;
    for (float el : {10.0f, 30.0f, 60.0f, 89.0f, 90.0f}) {
        for (float az : {0.0f, 45.0f, 160.0f, 300.0f}) {
            camera.elevationDegrees = el;
            camera.azimuthDegrees = az;
            const Mat4 v = view::viewMatrix(camera);
            CHECK_VEC3(view::transformPoint(v, view::eyePosition(camera)), 0, 0, 0, 1e-2);
            CHECK_VEC3(view::transformPoint(v, camera.target), 0, 0, -900, 1e-2);
        }
    }

    // Straight above (elevation 90, azimuth 0) the view IS the floor plan:
    // plan x to the right, plan y (world +Z) DOWN the screen. This is the
    // case the fixed world up (0, 1, 0) cannot handle.
    camera.elevationDegrees = 90;
    camera.azimuthDegrees = 0;
    const Mat4 top = view::viewMatrix(camera);
    const Vec3 t = camera.target;
    CHECK_VEC3(view::transformPoint(top, {t.x + 10, t.y, t.z}), 10, 0, -900, 1e-2);
    CHECK_VEC3(view::transformPoint(top, {t.x, t.y, t.z + 10}), 0, -10, -900, 1e-2);

    // For elevations below 90 the tangent up gives the same matrix as the
    // usual world up (0, 1, 0).
    camera.elevationDegrees = 35;
    camera.azimuthDegrees = 120;
    const Mat4 tangent = view::viewMatrix(camera);
    const Mat4 worldUp = Mat4::lookAt(view::eyePosition(camera), camera.target, {0, 1, 0});
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            CHECK_NEAR(tangent.m[i][j], worldUp.m[i][j], 1e-3);
        }
    }

    // Top view + orthographic + viewport: the room's plan corners land where
    // a 2D camera would put them (top-left stays top-left).
    const Mat4 ortho = Mat4::orthographic(-280, 280, -200, 200, 1, 2000);
    camera.target = {320, 0, 240};
    camera.elevationDegrees = 90;
    camera.azimuthDegrees = 0;
    const Mat4 topRoom = ortho * view::viewMatrix(camera);
    const Rect viewport{0, 0, 560, 400};
    Point pixel{};
    CHECK(view::projectPoint(topRoom, {40, 0, 40}, viewport, pixel));
    CHECK_POINT(pixel, 0, 0, 1e-2);
    CHECK(view::projectPoint(topRoom, {600, 0, 440}, viewport, pixel));
    CHECK_POINT(pixel, 560, 400, 1e-2);
    CHECK(view::projectPoint(topRoom, {320, 0, 240}, viewport, pixel));
    CHECK_POINT(pixel, 280, 200, 1e-2);
}

// ---- Near-plane clipping and drawWireMesh -----------------------------------

void testNearPlaneClip() {
    // Camera coordinates directly (view = identity), perspective with
    // fov 90, aspect 1, near 1: x_ndc = x / -z.
    const Mat4 p = Mat4::perspective(90, 1, 1, 100);

    // In front (z = -2) to behind (z = +2) the camera. Along the edge
    // z = -2 + 4s, x = 2s; it crosses the near plane z = -1 at s = 1/4,
    // x = 0.5. So the clipped end must be on the near plane at x_ndc = 0.5.
    Vec4 a = p * Vec4{0, 0, -2, 1};
    Vec4 b = p * Vec4{2, 0, 2, 1};
    CHECK(b.w < 0); // behind the camera: dividing now would mirror it
    CHECK(view::clipToNearPlane(a, b));
    CHECK_NEAR(a.x / a.w, 0, 1e-6);                 // the front end is unchanged
    CHECK_NEAR(a.w, 2, 1e-5);
    CHECK_NEAR(b.z + b.w, 0, 1e-5);                 // on the near plane
    CHECK_NEAR(b.w, 1, 1e-5);                       // distance = zNear
    CHECK_NEAR(b.x / b.w, 0.5, 1e-5);

    // The same edge given the other way round.
    Vec4 c = p * Vec4{2, 0, 2, 1};
    Vec4 d = p * Vec4{0, 0, -2, 1};
    CHECK(view::clipToNearPlane(c, d));
    CHECK_NEAR(c.x / c.w, 0.5, 1e-5);
    CHECK_NEAR(c.w, 1, 1e-5);

    // Fully in front: unchanged. Fully behind: rejected.
    Vec4 e = p * Vec4{1, 1, -5, 1};
    Vec4 g = p * Vec4{-1, 2, -50, 1};
    const Vec4 e0 = e;
    CHECK(view::clipToNearPlane(e, g));
    CHECK_NEAR(e.x, e0.x, 1e-6);
    CHECK_NEAR(e.w, e0.w, 1e-6);
    Vec4 h = p * Vec4{-1, 0, 1, 1};
    Vec4 k = p * Vec4{1, 0, 1, 1};
    CHECK(!view::clipToNearPlane(h, k));
    // Between the eye and the near plane counts as behind it too.
    Vec4 m1 = p * Vec4{0, 0, -0.5f, 1};
    Vec4 m2 = p * Vec4{0, 0, -0.2f, 1};
    CHECK(!view::clipToNearPlane(m1, m2));

    // Orthographic: the same test removes what is behind the near plane.
    const Mat4 o = Mat4::orthographic(-1, 1, -1, 1, 1, 10);
    Vec4 oa = o * Vec4{0, 0, -5, 1};
    Vec4 ob = o * Vec4{0, 0, 3, 1};
    CHECK(view::clipToNearPlane(oa, ob));
    CHECK_NEAR(ob.z / ob.w, -1, 1e-5);
}

void testDrawWireMesh() {
    const Mat4 p = Mat4::perspective(90, 1, 1, 100);

    // The edge from the test above, drawn on a 100 x 100 canvas. NDC x = 0
    // is pixel 50 and x = 0.5 is pixel 75 (window -1..1 -> viewport 0..100),
    // and y = 0 is row 50. With near clipping, the line runs from column 50
    // to 75: 26 pixels. Without it the end behind the camera (w = -2) would
    // divide to x_ndc = -1 and the line would run to the LEFT edge instead.
    {
        Canvas canvas = whiteCanvas(100, 100);
        xform::WireMesh edge;
        edge.vertices = {{0, 0, -2}, {2, 0, 2}};
        edge.edges = {{0, 1}};
        view::drawWireMesh(canvas, edge, p, kRed);
        CHECK(countPixels(canvas, kRed) == 26);
        CHECK(sameColor(canvas.getPixel(50, 50), kRed));
        CHECK(sameColor(canvas.getPixel(75, 50), kRed));
        CHECK(sameColor(canvas.getPixel(76, 50), kWhite));
        CHECK(sameColor(canvas.getPixel(49, 50), kWhite));
        CHECK(sameColor(canvas.getPixel(0, 50), kWhite));
    }

    // An edge completely behind the camera draws nothing. (Unclipped, its
    // ends would divide by w = -1 to x_ndc = +1 and -1: a full-width line.)
    {
        Canvas canvas = whiteCanvas(100, 100);
        xform::WireMesh behind;
        behind.vertices = {{-1, 0, 1}, {1, 0, 1}};
        behind.edges = {{0, 1}};
        view::drawWireMesh(canvas, behind, p, kRed);
        CHECK(countPixels(canvas, kRed) == 0);
    }

    // y is flipped: a point ABOVE the centre (y > 0) is drawn in the TOP half.
    {
        Canvas canvas = whiteCanvas(100, 100);
        xform::WireMesh up;
        up.vertices = {{0, 0, -4}, {0, 2, -4}}; // y_ndc 0 .. 0.5
        up.edges = {{0, 1}};
        view::drawWireMesh(canvas, up, p, kRed);
        CHECK(sameColor(canvas.getPixel(50, 50), kRed));
        CHECK(sameColor(canvas.getPixel(50, 25), kRed));
        CHECK(sameColor(canvas.getPixel(50, 75), kWhite));
        CHECK(countPixels(canvas, kRed) == 26);
    }

    // Clipped to the canvas / viewport: an edge running far off the side
    // only touches pixels inside the viewport; a bad index is skipped.
    {
        Canvas canvas = whiteCanvas(200, 100);
        const Rect left{0, 0, 100, 100};
        xform::WireMesh longEdge;
        longEdge.vertices = {{-1, 0, -2}, {30, 0, -2}}; // x_ndc -0.5 .. 15
        longEdge.edges = {{0, 1}, {0, 7}, {-1, 1}};
        view::drawWireMesh(canvas, longEdge, p, kRed, left);
        CHECK(countNonWhiteOutside(canvas, left) == 0);
        CHECK(countPixels(canvas, kRed) == 75); // columns 25..99
    }

    // A whole box seen by an orbit camera from outside: every corner is in
    // front, every edge is drawn, nothing is drawn off the canvas.
    {
        Canvas canvas = whiteCanvas(160, 120);
        xform::WireMesh box;
        for (int i = 0; i < 8; ++i) {
            box.vertices.push_back({(i & 1) ? 100.0f : 0.0f, (i & 2) ? 80.0f : 0.0f,
                                    (i & 4) ? 60.0f : 0.0f});
        }
        box.edges = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                     {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        OrbitCamera camera;
        camera.target = {50, 40, 30};
        camera.radius = 400;
        const Mat4 vp = Mat4::perspective(45, 160.0f / 120.0f, 10, 2000) * view::viewMatrix(camera);
        view::drawWireMesh(canvas, box, vp, kBlue);
        CHECK(countPixels(canvas, kBlue) > 200);
        // The target projects to the centre of the canvas.
        Point centre{};
        CHECK(view::projectPoint(vp, camera.target, Rect{0, 0, 160, 120}, centre));
        CHECK_POINT(centre, 80, 60, 1e-2);
        // A point behind the camera cannot be projected.
        const Vec3 eye = view::eyePosition(camera);
        const Vec3 behindEye = view::add(eye, view::subtract(eye, camera.target));
        CHECK(!view::projectPoint(vp, behindEye, Rect{0, 0, 160, 120}, centre));
    }
}

} // namespace

int main() {
    testWindowToViewport();
    testPan();
    testZoomAbout();
    testMatchViewportAspect();
    testScreenToRoom();
    testDrawThroughCamera();
    testMat4Basics();
    testRotationY();
    testMat4Products();
    testLookAt();
    testPerspective();
    testOrthographic();
    testEyePosition();
    testViewMatrix();
    testNearPlaneClip();
    testDrawWireMesh();
    return test::summary("test_view");
}
