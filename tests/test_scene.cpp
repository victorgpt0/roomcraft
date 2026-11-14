// Tests for the Checkpoint 2 scene graph and extrusion:
//   - scene::Node world matrices (two and three levels, parent rotation,
//     independent siblings, lookup by name);
//   - scene::drawNode actually drawing at the transformed position;
//   - scene::extrude vertex/edge counts and the 2D-to-3D mapping;
//   - scene::extrudedRoom and the oblique (cabinet) projection.

#include "scene/extrude.h"
#include "scene/node.h"
#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"
#include "xform/mat3.h"
#include "xform/pose.h"
#include "xform/vec.h"

#include "test_helpers.h"

#include <cmath>
#include <cstddef>

using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Polygon;
using xform::Mat3;
using xform::Pose;
using xform::Vec3;

namespace {

void checkPointNear(Point actual, Point expected, double tolerance) {
    CHECK_NEAR(actual.x, expected.x, tolerance);
    CHECK_NEAR(actual.y, expected.y, tolerance);
}

void checkMatricesNear(const Mat3& actual, const Mat3& expected, double tolerance) {
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            CHECK_NEAR(actual.m[row][column], expected.m[row][column], tolerance);
        }
    }
}

void testTwoLevelModel() {
    // Root at (100, 50); the child sits 20 cm right and 10 cm down from it.
    // The child's local shape vertex (5, 0) is therefore at (125, 60).
    scene::Node root;
    root.name = "root";
    root.pose.position = {100.0f, 50.0f};

    scene::Node child;
    child.name = "child";
    child.pose.position = {20.0f, 10.0f};
    child.shape = raster::makeRectangle(-5.0f, -5.0f, 10.0f, 10.0f);
    child.color = Color{200, 80, 80};
    root.children.push_back(child);

    const Mat3 rootWorld = scene::worldMatrix(root, "root");
    checkMatricesNear(rootWorld, xform::poseMatrix(root.pose), 1e-6);

    const Mat3 childWorld = scene::worldMatrix(root, "child");
    checkMatricesNear(childWorld, xform::poseMatrix(root.pose) * xform::poseMatrix(child.pose), 1e-6);
    checkPointNear(xform::transformPoint(childWorld, {0.0f, 0.0f}), {120.0f, 60.0f}, 1e-5);
    checkPointNear(xform::transformPoint(childWorld, {5.0f, 0.0f}), {125.0f, 60.0f}, 1e-5);
    checkPointNear(xform::transformPoint(childWorld, {0.0f, -5.0f}), {120.0f, 55.0f}, 1e-5);
}

void testThreeLevelModel() {
    // Hand-computed chain:
    //   root     = translate(10, 20) * rotate(90)   (90 is clockwise on screen)
    //   child    = translate(30, 0)
    //   grandchild = translate(0, 20)
    //
    // Child origin: rotate(90) * (30, 0) = (0, 30), then +(10, 20) = (10, 50).
    // Tip (5, 0) in grandchild: (5, 0) -> (5, 20) -> (35, 20)
    //   -> rotate(90) = (-20, 35) -> +(10, 20) = (-10, 55).
    scene::Node root;
    root.name = "root";
    root.pose.position = {10.0f, 20.0f};
    root.pose.angleDegrees = 90.0f;

    scene::Node child;
    child.name = "child";
    child.pose.position = {30.0f, 0.0f};

    scene::Node grandchild;
    grandchild.name = "tip";
    grandchild.pose.position = {0.0f, 20.0f};
    grandchild.shape = raster::makeRectangle(-2.0f, -2.0f, 4.0f, 4.0f);

    child.children.push_back(grandchild);
    root.children.push_back(child);

    const Mat3 expected = xform::poseMatrix(root.pose) * xform::poseMatrix(child.pose) *
                          xform::poseMatrix(grandchild.pose);
    const Mat3 actual = scene::worldMatrix(root, "tip");
    checkMatricesNear(actual, expected, 1e-5);

    checkPointNear(xform::transformPoint(actual, {0.0f, 0.0f}), {-10.0f, 50.0f}, 1e-4);
    checkPointNear(xform::transformPoint(actual, {5.0f, 0.0f}), {-10.0f, 55.0f}, 1e-4);

    // The middle level is correct too, not just the leaf.
    checkPointNear(xform::transformPoint(scene::worldMatrix(root, "child"), {0.0f, 0.0f}),
                   {10.0f, 50.0f}, 1e-4);
}

void testParentRotationMovesChildOnCircle() {
    scene::Node root;
    root.name = "root";
    root.pose.position = {200.0f, 150.0f};

    scene::Node child;
    child.name = "child";
    child.pose.position = {50.0f, 0.0f};
    root.children.push_back(child);

    for (int angle = 0; angle < 360; angle += 15) {
        root.pose.angleDegrees = static_cast<float>(angle);
        const Point world = xform::transformPoint(scene::worldMatrix(root, "child"), {0.0f, 0.0f});

        // A point on a circle of radius 50 around (200, 150), at the same
        // angle. The radius must not change as the parent turns.
        const double radians = angle * raster::kPi / 180.0;
        const double expectedX = 200.0 + 50.0 * std::cos(radians);
        const double expectedY = 150.0 + 50.0 * std::sin(radians);
        CHECK_NEAR(world.x, expectedX, 1e-3);
        CHECK_NEAR(world.y, expectedY, 1e-3);

        const double dx = world.x - 200.0;
        const double dy = world.y - 150.0;
        CHECK_NEAR(std::sqrt(dx * dx + dy * dy), 50.0, 1e-3);
    }

    // The four cardinal angles, written out so a sign error cannot hide.
    root.pose.angleDegrees = 0.0f;
    checkPointNear(xform::transformPoint(scene::worldMatrix(root, "child"), {0, 0}),
                   {250.0f, 150.0f}, 1e-4);
    root.pose.angleDegrees = 90.0f;
    checkPointNear(xform::transformPoint(scene::worldMatrix(root, "child"), {0, 0}),
                   {200.0f, 200.0f}, 1e-4);
    root.pose.angleDegrees = 180.0f;
    checkPointNear(xform::transformPoint(scene::worldMatrix(root, "child"), {0, 0}),
                   {150.0f, 150.0f}, 1e-4);
    root.pose.angleDegrees = 270.0f;
    checkPointNear(xform::transformPoint(scene::worldMatrix(root, "child"), {0, 0}),
                   {200.0f, 100.0f}, 1e-4);
}

void testChangingChildLeavesParentAndSiblingAlone() {
    scene::Node root;
    root.name = "root";
    root.pose.position = {100.0f, 100.0f};
    root.pose.angleDegrees = 30.0f;

    scene::Node first;
    first.name = "first";
    first.pose.position = {10.0f, 0.0f};

    scene::Node second;
    second.name = "second";
    second.pose.position = {0.0f, 10.0f};

    root.children.push_back(first);
    root.children.push_back(second);

    const Mat3 rootBefore = scene::worldMatrix(root, "root");
    const Mat3 firstBefore = scene::worldMatrix(root, "first");
    const Mat3 secondBefore = scene::worldMatrix(root, "second");

    // Move and turn only the first child.
    root.children[0].pose.position = {60.0f, 60.0f};
    root.children[0].pose.angleDegrees = 45.0f;

    const Mat3 rootAfter = scene::worldMatrix(root, "root");
    const Mat3 firstAfter = scene::worldMatrix(root, "first");
    const Mat3 secondAfter = scene::worldMatrix(root, "second");

    checkMatricesNear(rootAfter, rootBefore, 0.0);
    checkMatricesNear(secondAfter, secondBefore, 0.0);
    CHECK(std::fabs(firstAfter.m[0][2] - firstBefore.m[0][2]) > 1.0f ||
          std::fabs(firstAfter.m[1][2] - firstBefore.m[1][2]) > 1.0f ||
          std::fabs(firstAfter.m[0][0] - firstBefore.m[0][0]) > 1e-4f);
}

void testWorldMatrixLookup() {
    scene::Node room;
    room.name = "room";
    room.pose.position = {320.0f, 240.0f};
    room.pose.angleDegrees = 10.0f;

    scene::Node door;
    door.name = "door";
    door.pose.position = {120.0f, 180.0f};
    door.pose.angleDegrees = -35.0f;

    scene::Node handle;
    handle.name = "handle";
    handle.pose.position = {100.0f, 0.0f};

    door.children.push_back(handle);
    room.children.push_back(door);

    bool found = false;
    checkMatricesNear(scene::worldMatrix(room, "room", &found), xform::poseMatrix(room.pose), 1e-6);
    CHECK(found);
    checkMatricesNear(scene::worldMatrix(room, "door", &found),
                      xform::poseMatrix(room.pose) * xform::poseMatrix(door.pose), 1e-5);
    CHECK(found);
    checkMatricesNear(scene::worldMatrix(room, "handle", &found),
                      xform::poseMatrix(room.pose) * xform::poseMatrix(door.pose) *
                          xform::poseMatrix(handle.pose),
                      1e-5);
    CHECK(found);

    found = true;
    const Mat3 missing = scene::worldMatrix(room, "missing", &found);
    CHECK(!found);
    checkMatricesNear(missing, Mat3::identity(), 0.0);

    // Not even the root name is found when it does not match.
    found = true;
    (void)scene::worldMatrix(room, "Room", &found);
    CHECK(!found);
}

void testDrawNodePlacesChildShape() {
    Canvas canvas(200, 200);
    canvas.clear(255, 255, 255);

    scene::Node root;
    root.name = "root";
    root.pose.position = {100.0f, 100.0f};

    scene::Node marker;
    marker.name = "marker";
    marker.shape = raster::makeRectangle(-6.0f, -6.0f, 12.0f, 12.0f);
    marker.color = Color{220, 30, 30};
    root.children.push_back(marker);

    scene::drawNode(canvas, root, Mat3::identity());

    const Color centre = canvas.getPixel(100, 100);
    CHECK(centre.r == 220 && centre.g == 30 && centre.b == 30);
    const Color outside = canvas.getPixel(20, 20);
    CHECK(outside.r == 255 && outside.g == 255 && outside.b == 255);
}

void testExtrude() {
    const Polygon footprint = raster::makeRectangle(10.0f, 20.0f, 30.0f, 40.0f);
    const float height = 100.0f;
    const xform::WireMesh mesh = scene::extrude(footprint, height);

    const std::size_t n = footprint.vertices.size(); // 4
    CHECK(n == 4);
    CHECK(mesh.vertices.size() == 2 * n);
    CHECK(mesh.edges.size() == 3 * n);

    // Bottom ring at Y = 0, top ring at Y = height; X and Z come straight
    // from the plan point (x, y) -> (X = x, Z = y).
    for (std::size_t i = 0; i < n; ++i) {
        const Point& plan = footprint.vertices[i];
        CHECK_NEAR(mesh.vertices[i].x, plan.x, 0.0);
        CHECK_NEAR(mesh.vertices[i].y, 0.0, 0.0);
        CHECK_NEAR(mesh.vertices[i].z, plan.y, 0.0);

        CHECK_NEAR(mesh.vertices[n + i].x, plan.x, 0.0);
        CHECK_NEAR(mesh.vertices[n + i].y, height, 0.0);
        CHECK_NEAR(mesh.vertices[n + i].z, plan.y, 0.0);
    }

    checkPointNear({mesh.vertices[0].x, mesh.vertices[0].z}, {10.0f, 20.0f}, 0.0);
    checkPointNear({mesh.vertices[2].x, mesh.vertices[2].z}, {40.0f, 60.0f}, 0.0);

    // Every expected bottom edge, top edge and upright is present.
    const auto hasEdge = [&mesh](int a, int b) {
        for (const auto& edge : mesh.edges) {
            if ((edge.first == a && edge.second == b) || (edge.first == b && edge.second == a)) {
                return true;
            }
        }
        return false;
    };
    for (std::size_t i = 0; i < n; ++i) {
        const int next = static_cast<int>((i + 1) % n);
        const int bottom = static_cast<int>(i);
        const int top = static_cast<int>(n + i);
        CHECK(hasEdge(bottom, next));
        CHECK(hasEdge(top, static_cast<int>(n) + next));
        CHECK(hasEdge(bottom, top));
    }

    // A triangle with arbitrary vertices follows the same mapping.
    Polygon triangle;
    triangle.vertices = {{0.0f, 0.0f}, {100.0f, 0.0f}, {0.0f, 50.0f}};
    const xform::WireMesh prism = scene::extrude(triangle, 42.0f);
    CHECK(prism.vertices.size() == 6);
    CHECK(prism.edges.size() == 9);
    CHECK_NEAR(prism.vertices[1].x, 100.0, 0.0);
    CHECK_NEAR(prism.vertices[1].y, 0.0, 0.0);
    CHECK_NEAR(prism.vertices[1].z, 0.0, 0.0);
    CHECK_NEAR(prism.vertices[4].x, 100.0, 0.0);
    CHECK_NEAR(prism.vertices[4].y, 42.0, 0.0);
    CHECK_NEAR(prism.vertices[5].x, 0.0, 0.0);
    CHECK_NEAR(prism.vertices[5].z, 50.0, 0.0);
}

void testExtrudedRoom() {
    const Polygon room = raster::makeRectangle(40.0f, 40.0f, 560.0f, 400.0f);
    const xform::WireMesh walls = scene::extrudedRoom(room, 250.0f);

    CHECK(walls.vertices.size() == 2 * room.vertices.size());
    CHECK(walls.edges.size() == 3 * room.vertices.size());
    for (std::size_t i = 0; i < room.vertices.size(); ++i) {
        CHECK_NEAR(walls.vertices[i].y, 0.0, 0.0);       // floor perimeter
        CHECK_NEAR(walls.vertices[4 + i].y, 250.0, 0.0); // wall tops
    }
    CHECK_NEAR(walls.vertices[0].x, 40.0, 0.0);
    CHECK_NEAR(walls.vertices[0].z, 40.0, 0.0);
    CHECK_NEAR(walls.vertices[2].x, 600.0, 0.0);
    CHECK_NEAR(walls.vertices[2].z, 440.0, 0.0);
}

void testObliqueProjection() {
    const double k = 0.5 * std::cos(45.0 * raster::kPi / 180.0); // 0.35355339

    checkPointNear(scene::obliqueProject(Vec3{0, 0, 0}), {0.0f, 0.0f}, 1e-5);
    checkPointNear(scene::obliqueProject(Vec3{10, 0, 0}), {10.0f, 0.0f}, 1e-5);
    // World Y is up, screen y is down.
    checkPointNear(scene::obliqueProject(Vec3{0, 10, 0}), {0.0f, -10.0f}, 1e-5);
    // Depth Z is sheared equally into +x and +y at 45 degrees.
    checkPointNear(scene::obliqueProject(Vec3{0, 0, 10}), {static_cast<float>(10.0 * k), static_cast<float>(10.0 * k)}, 1e-5);

    const Point offset = scene::obliqueProject(Vec3{12, 34, 56}, {100.0f, 50.0f});
    CHECK_NEAR(offset.x, 100.0 + 12.0 + 56.0 * k, 1e-4);
    CHECK_NEAR(offset.y, 50.0 - 34.0 + 56.0 * k, 1e-4);

    // A pure Z step changes both screen coordinates by the same amount: the
    // projection really is a 45-degree shear.
    const Point a = scene::obliqueProject(Vec3{5, 5, 0});
    const Point b = scene::obliqueProject(Vec3{5, 5, 20});
    CHECK_NEAR(b.x - a.x, 20.0 * k, 1e-4);
    CHECK_NEAR(b.y - a.y, 20.0 * k, 1e-4);
}

} // namespace

int main() {
    testTwoLevelModel();
    testThreeLevelModel();
    testParentRotationMovesChildOnCircle();
    testChangingChildLeavesParentAndSiblingAlone();
    testWorldMatrixLookup();
    testDrawNodePlacesChildShape();
    testExtrude();
    testExtrudedRoom();
    testObliqueProjection();
    return test::summary("test_scene");
}
