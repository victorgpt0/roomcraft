// Tests for xform::Pose and for furniture placed by its pose.

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "xform/mat3.h"
#include "xform/pose.h"

#include "test_helpers.h"

#include <cstddef>
#include <vector>

using raster::Point;
using raster::Polygon;
using roomcraft::Furniture;
using xform::Mat3;
using xform::Pose;

namespace {

// The 5.6 m x 4.0 m demo room (1 px = 1 cm).
const Polygon kRoom = raster::makeRectangle(40, 40, 560, 400);
const raster::Color kGrey{200, 200, 200};

// A 120 x 60 cm desk in LOCAL coordinates, centred on its own origin.
Polygon localDesk() {
    return raster::makeRectangle(-60, -30, 120, 60);
}

Furniture deskAt(Point position, float angleDegrees = 0.0f, float scaleX = 1.0f, float scaleY = 1.0f) {
    Furniture desk{"desk", localDesk(), kGrey, Pose{}};
    desk.pose.position = position;
    desk.pose.angleDegrees = angleDegrees;
    desk.pose.scaleX = scaleX;
    desk.pose.scaleY = scaleY;
    return desk;
}

void checkMatricesNear(const Mat3& a, const Mat3& b, double tolerance) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            CHECK_NEAR(a.m[i][j], b.m[i][j], tolerance);
        }
    }
}

void testPoseMatrix() {
    // The default pose changes nothing.
    checkMatricesNear(xform::poseMatrix(Pose{}), Mat3::identity(), 0);

    // Order: scale, then rotate, then move.
    Pose pose;
    pose.position = {100, 50};
    pose.angleDegrees = 90;
    pose.scaleX = 2;
    pose.scaleY = 1;
    // Local (10, 0): scale x by 2 -> (20, 0); rotate +90 (clockwise, so right
    // becomes down) -> (0, 20); move by (100, 50) -> (100, 70).
    const Point p = xform::transformPoint(xform::poseMatrix(pose), {10, 0});
    CHECK_NEAR(p.x, 100, 1e-4);
    CHECK_NEAR(p.y, 70, 1e-4);
    // Local (0, 10): scale y by 1 -> (0, 10); rotate -> (-10, 0); move -> (90, 50).
    const Point q = xform::transformPoint(xform::poseMatrix(pose), {0, 10});
    CHECK_NEAR(q.x, 90, 1e-4);
    CHECK_NEAR(q.y, 50, 1e-4);

    // The other orders give different answers, so the order is really tested:
    // rotate-then-scale would give (100, 60) for the first point.
    const Mat3 wrongOrder = Mat3::translation(100, 50) * Mat3::scaling(2, 1) * Mat3::rotation(90);
    const Point wrong = xform::transformPoint(wrongOrder, {10, 0});
    CHECK_NEAR(wrong.x, 100, 1e-4);
    CHECK_NEAR(wrong.y, 60, 1e-4);

    // It is exactly the documented product.
    checkMatricesNear(xform::poseMatrix(pose),
                      Mat3::translation(100, 50) * Mat3::rotation(90) * Mat3::scaling(2, 1), 0);

    // The local origin always lands on pose.position, whatever the angle and scale.
    for (int angle = 0; angle < 360; angle += 45) {
        pose.angleDegrees = static_cast<float>(angle);
        const Point origin = xform::transformPoint(xform::poseMatrix(pose), {0, 0});
        CHECK_NEAR(origin.x, 100, 0);
        CHECK_NEAR(origin.y, 50, 0);
    }
}

void testWorldFootprint() {
    // Default pose: the world footprint is exactly the local footprint, so
    // Checkpoint 1 pieces built in room coordinates are unaffected.
    const Furniture cp1{"bed", raster::makeRectangle(60, 60, 140, 200), kGrey, Pose{}};
    const Polygon world = roomcraft::worldFootprint(cp1);
    CHECK(world.vertices.size() == cp1.footprint.vertices.size());
    for (std::size_t i = 0; i < world.vertices.size(); ++i) {
        CHECK(world.vertices[i].x == cp1.footprint.vertices[i].x);
        CHECK(world.vertices[i].y == cp1.footprint.vertices[i].y);
    }

    // Moved desk: corners are the local corners plus the position.
    const Polygon moved = roomcraft::worldFootprint(deskAt({300, 200}));
    CHECK_NEAR(moved.vertices[0].x, 240, 1e-4);
    CHECK_NEAR(moved.vertices[0].y, 170, 1e-4);
    CHECK_NEAR(moved.vertices[2].x, 360, 1e-4);
    CHECK_NEAR(moved.vertices[2].y, 230, 1e-4);

    // Turned by 90 the 120 x 60 desk occupies 60 x 120, still centred.
    const raster::Rect box = raster::computeBoundingBox(roomcraft::worldFootprint(deskAt({300, 200}, 90)));
    CHECK_NEAR(box.xmin, 270, 1e-3);
    CHECK_NEAR(box.xmax, 330, 1e-3);
    CHECK_NEAR(box.ymin, 140, 1e-3);
    CHECK_NEAR(box.ymax, 260, 1e-3);

    // The centroid of the placed piece is its pose position for any angle,
    // because the local shape is centred on its origin.
    for (int angle = 0; angle < 360; angle += 20) {
        const Point c = raster::computeCentroid(roomcraft::worldFootprint(deskAt({300, 200}, static_cast<float>(angle))));
        CHECK_NEAR(c.x, 300, 1e-2);
        CHECK_NEAR(c.y, 200, 1e-2);
    }

    // Pose scale changes the area by scaleX * scaleY; rotation does not.
    CHECK_NEAR(raster::computeArea(roomcraft::worldFootprint(deskAt({300, 200}, 35))), 120.0 * 60.0, 0.5);
    CHECK_NEAR(raster::computeArea(roomcraft::worldFootprint(deskAt({300, 200}, 35, 1.5f, 2.0f))),
               120.0 * 60.0 * 3.0, 1.5);

    // The local footprint itself is never modified by placing the piece.
    const Furniture desk = deskAt({300, 200}, 35, 1.5f, 2.0f);
    (void)roomcraft::worldFootprint(desk);
    CHECK(desk.footprint.vertices[0].x == -60.0f && desk.footprint.vertices[0].y == -30.0f);
}

void testHitTestWithPose() {
    // Desk at (300, 200), not turned: covers x 240..360, y 170..230.
    Furniture desk = deskAt({300, 200});
    CHECK(roomcraft::hitTest(desk, {300, 200}));
    CHECK(roomcraft::hitTest(desk, {350, 180}));
    CHECK(!roomcraft::hitTest(desk, {300, 240}));
    // The local coordinates are NOT where the piece is: a click near the
    // room origin misses.
    CHECK(!roomcraft::hitTest(desk, {0, 0}));
    CHECK(!roomcraft::hitTest(desk, {50, 20}));

    // Turned by 90: now covers x 270..330, y 140..260.
    desk.pose.angleDegrees = 90;
    CHECK(roomcraft::hitTest(desk, {300, 250}));  // was a miss, now a hit
    CHECK(!roomcraft::hitTest(desk, {350, 180})); // was a hit, now a miss
    CHECK(roomcraft::hitTest(desk, {300, 200}));  // the centre stays a hit

    // Turned by 45: the corner of the bounding box is empty floor.
    desk.pose.angleDegrees = 45;
    CHECK(roomcraft::hitTest(desk, {330, 230}));  // along the desk's long axis
    CHECK(!roomcraft::hitTest(desk, {330, 170})); // across it

    // Moving the piece moves the hit area with it.
    desk.pose.angleDegrees = 0;
    desk.pose.position = {500, 100};
    CHECK(!roomcraft::hitTest(desk, {300, 200}));
    CHECK(roomcraft::hitTest(desk, {500, 100}));

    // Scaled to double width: x 380..620 around (500, 100).
    desk.pose.scaleX = 2;
    CHECK(roomcraft::hitTest(desk, {400, 100}));
    desk.pose.scaleX = 1;
    CHECK(!roomcraft::hitTest(desk, {400, 100}));

    // Mirror with a negative scale: an L-shaped sofa whose arm is on the
    // right ends up with its arm on the left.
    Polygon l;
    l.vertices = {{-100, -80}, {100, -80}, {100, 80}, {50, 80}, {50, -30}, {-100, -30}};
    Furniture sofa{"sofa", l, kGrey, Pose{}};
    sofa.pose.position = {300, 200};
    CHECK(roomcraft::hitTest(sofa, {375, 250}));  // the arm, bottom right
    CHECK(!roomcraft::hitTest(sofa, {225, 250})); // empty floor, bottom left
    sofa.pose.scaleX = -1;
    CHECK(!roomcraft::hitTest(sofa, {375, 250}));
    CHECK(roomcraft::hitTest(sofa, {225, 250}));

    // findFurnitureAt: two pieces share one local footprint and differ only
    // by pose; the topmost (last) one under the click wins.
    const std::vector<Furniture> pieces = {deskAt({150, 150}), deskAt({400, 300}), deskAt({420, 310})};
    CHECK(roomcraft::findFurnitureAt(pieces, {150, 150}) == 0);
    CHECK(roomcraft::findFurnitureAt(pieces, {350, 280}) == 1); // only the second covers this
    CHECK(roomcraft::findFurnitureAt(pieces, {410, 305}) == 2); // both cover it: top one wins
    CHECK(roomcraft::findFurnitureAt(pieces, {0, 0}) == -1);    // the shared local origin area
}

void testFitsInRoomWithPose() {
    // In the top-left corner, 5 cm from each wall: x 45..165, y 45..105.
    CHECK(roomcraft::fitsInRoom(deskAt({105, 75}), kRoom));
    // Touching both walls exactly still fits.
    CHECK(roomcraft::fitsInRoom(deskAt({100, 70}), kRoom));

    // Turning it by 30 on the spot pushes a corner through the wall (the
    // same situation as the Checkpoint 1 test, now done with the pose).
    CHECK(!roomcraft::fitsInRoom(deskAt({105, 75}, 30), kRoom));
    // In the middle of the room it fits at every angle.
    for (int angle = 0; angle < 360; angle += 15) {
        CHECK(roomcraft::fitsInRoom(deskAt({320, 240}, static_cast<float>(angle)), kRoom));
    }

    // Pushed 40 cm through the right wall (x 520..640, wall at 600):
    // 80 of its 120 cm are inside.
    const Furniture through = deskAt({580, 200});
    CHECK(!roomcraft::fitsInRoom(through, kRoom));
    CHECK_NEAR(roomcraft::areaInsideRoom(through, kRoom), 80.0 * 60.0, 1e-1);

    // Turned by 90 at the same position it is only 60 wide (x 550..610)...
    CHECK(!roomcraft::fitsInRoom(deskAt({580, 200}, 90), kRoom));
    // ...and 15 cm further left it fits (x 535..595).
    CHECK(roomcraft::fitsInRoom(deskAt({565, 200}, 90), kRoom));

    // Completely outside.
    CHECK(!roomcraft::fitsInRoom(deskAt({800, 200}), kRoom));
    CHECK(roomcraft::areaInsideRoom(deskAt({800, 200}), kRoom) == 0.0f);
    // A piece left at the default pose with a local, origin-centred footprint
    // sits at the canvas corner, outside the room.
    CHECK(!roomcraft::fitsInRoom(Furniture{"unplaced", localDesk(), kGrey, Pose{}}, kRoom));

    // Scaling up by the pose can make a piece too big for the room.
    CHECK(roomcraft::fitsInRoom(deskAt({320, 240}, 0, 4.0f, 1.0f), kRoom));  // 480 wide
    CHECK(!roomcraft::fitsInRoom(deskAt({320, 240}, 0, 5.0f, 1.0f), kRoom)); // 600 wide > 560

    // A mirrored piece (negative scale reverses the vertex order) is still
    // measured correctly.
    CHECK(roomcraft::fitsInRoom(deskAt({320, 240}, 20, -1.0f, 1.0f), kRoom));
    // Scale 0 collapses the footprint to a line: not a real piece.
    CHECK(!roomcraft::fitsInRoom(deskAt({320, 240}, 0, 0.0f, 1.0f), kRoom));
}

void testDrawFurnitureWithPose() {
    raster::Canvas canvas(640, 480);
    canvas.clear(255, 255, 255);
    Furniture desk = deskAt({300, 200});
    desk.color = raster::Color{10, 120, 200};
    roomcraft::drawFurniture(canvas, desk);

    const auto isDeskColour = [&](int x, int y) {
        const raster::Color c = canvas.getPixel(x, y);
        return c.r == 10 && c.g == 120 && c.b == 200;
    };
    const auto isWhite = [&](int x, int y) {
        const raster::Color c = canvas.getPixel(x, y);
        return c.r == 255 && c.g == 255 && c.b == 255;
    };
    CHECK(isDeskColour(300, 200)); // drawn where the pose puts it
    CHECK(isDeskColour(250, 180));
    CHECK(isWhite(20, 10));        // nothing drawn at the local coordinates
    CHECK(isWhite(300, 250));      // just below the desk

    // Every filled pixel passes the hit test and every pixel well outside
    // fails it: drawing and selection agree on where the piece is.
    desk.pose.angleDegrees = 30;
    canvas.clear(255, 255, 255);
    roomcraft::drawFurniture(canvas, desk);
    int mismatches = 0;
    int filled = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            if (isDeskColour(x, y)) {
                ++filled;
                if (!roomcraft::hitTest(desk, {static_cast<float>(x), static_cast<float>(y)})) {
                    ++mismatches;
                }
            }
        }
    }
    CHECK(mismatches == 0);
    // About 120 * 60 pixels, minus the outline drawn over the border.
    CHECK_NEAR(filled, 120.0 * 60.0, 400.0);
}

} // namespace

int main() {
    testPoseMatrix();
    testWorldFootprint();
    testHitTestWithPose();
    testFitsInRoomWithPose();
    testDrawFurnitureWithPose();
    return test::summary("test_pose");
}
