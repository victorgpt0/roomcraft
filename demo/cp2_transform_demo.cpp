// RoomCraft Checkpoint 2, Part 1 demo: 2D transformations as 3x3 matrices.
//
// Writes one still image per technique to demo_output/cp2/ (or to the folder
// given as the first argument):
//
//   transform_basic   translate, rotate, scale, reflect and shear one armchair
//   transform_order   the same two matrices multiplied in both orders
//   transform_pivot   a door turning on its hinge; a desk turned about its
//                     own centre versus about the corner of the room
//   transform_mirror  an L-shaped sofa mirrored into the opposite corner,
//                     and sheared copies used as drop shadows
//   transform_pose    one local footprint placed several times by its Pose,
//                     with pieces that do not fit in the room drawn in red
//
// In every image a DASHED outline is the shape before the transform and a
// SOLID shape is the result. A small cross marks a pivot or a pose position.
// Floor plan units: 1 pixel = 1 cm; the room is 5.6 m x 4.0 m, as in the
// Checkpoint 1 demo.

#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "xform/mat3.h"
#include "xform/pose.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifndef ROOMCRAFT_DEMO_OUTPUT_DIR
#define ROOMCRAFT_DEMO_OUTPUT_DIR "demo_output"
#endif

using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Polygon;
using roomcraft::Furniture;
using xform::Mat3;
using xform::Pose;

namespace {

// ---- Palette (same family as the Checkpoint 1 demo) ------------------------
const Color kWhite{255, 255, 255};
const Color kOutline{40, 40, 40};
const Color kWall{60, 60, 70};
const Color kFloor{238, 226, 205};
const Color kGrid{214, 198, 172};
const Color kGhost{120, 120, 120};
const Color kGuide{150, 150, 150};
const Color kShadow{196, 184, 164};
const Color kWood{181, 136, 99};
const Color kSofa{120, 150, 110};
const Color kBlue{96, 125, 160};
const Color kOrange{230, 150, 60};
const Color kPurple{150, 110, 170};
const Color kTeal{80, 160, 150};
const Color kRose{200, 110, 120};
const Color kWarning{220, 30, 30};
const Color kPivot{200, 30, 30};

// ---- Room geometry (as in demo/roomcraft_demo.cpp) --------------------------
constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 480;
constexpr float kRoomLeft = 40.0f;
constexpr float kRoomTop = 40.0f;
constexpr float kRoomWidth = 560.0f;
constexpr float kRoomHeight = 400.0f;
constexpr int kWallThickness = 8;

Polygon roomOutline() {
    return raster::makeRectangle(kRoomLeft, kRoomTop, kRoomWidth, kRoomHeight);
}

int failures = 0;

void save(const Canvas& canvas, const std::string& directory, const std::string& name) {
    const std::string path = directory + "/" + name + ".bmp";
    if (canvas.saveBMP(path)) {
        std::printf("  wrote %s\n", path.c_str());
    } else {
        std::printf("  ERROR: could not write %s\n", path.c_str());
        ++failures;
    }
}

// ---- Small drawing helpers ---------------------------------------------------

int px(float v) {
    return raster::roundToPixel(v);
}

void drawSegment(Canvas& canvas, Point a, Point b, Color color) {
    raster::drawLineBresenham(canvas, px(a.x), px(a.y), px(b.x), px(b.y), color);
}

void drawDashedSegment(Canvas& canvas, Point a, Point b, Color color) {
    raster::drawDashedLine(canvas, px(a.x), px(a.y), px(b.x), px(b.y), 5, 4, color);
}

void drawCross(Canvas& canvas, Point p, Color color) {
    const int x = px(p.x);
    const int y = px(p.y);
    for (int offset = -1; offset <= 1; ++offset) { // three lines thick so it stays visible
        raster::drawLineBresenham(canvas, x - 5 + offset, y - 5, x + 5 + offset, y + 5, color);
        raster::drawLineBresenham(canvas, x - 5 + offset, y + 5, x + 5 + offset, y - 5, color);
    }
}

// The shape BEFORE a transform.
void drawGhost(Canvas& canvas, const Polygon& polygon, Color color = kGhost) {
    const std::vector<Point>& v = polygon.vertices;
    for (std::size_t i = 0; i < v.size(); ++i) {
        drawDashedSegment(canvas, v[i], v[(i + 1) % v.size()], color);
    }
}

// The shape AFTER a transform.
void drawSolid(Canvas& canvas, const Polygon& polygon, Color color) {
    raster::fillPolygonScanline(canvas, polygon, color);
    raster::drawPolygonOutline(canvas, polygon, kOutline);
}

// Mixes a colour with white: amount 0 = the colour, 1 = white. Used to draw
// the earlier steps of a movement paler than the last one.
Color fade(Color c, float amount) {
    const auto mix = [amount](uint8_t value) {
        return static_cast<uint8_t>(value + (255 - value) * amount);
    };
    return Color{mix(c.r), mix(c.g), mix(c.b)};
}

// Floor, 50 cm grid (clipped to the room with Cohen-Sutherland) and walls.
Canvas roomCanvas() {
    Canvas canvas(kCanvasWidth, kCanvasHeight);
    canvas.clear(kWhite.r, kWhite.g, kWhite.b);
    raster::fillPolygonScanline(canvas, roomOutline(), kFloor);

    const raster::Rect room{kRoomLeft, kRoomTop, kRoomLeft + kRoomWidth, kRoomTop + kRoomHeight};
    for (float x = kRoomLeft; x <= kRoomLeft + kRoomWidth; x += 50.0f) {
        raster::drawLineClipped(canvas, {x, 0}, {x, static_cast<float>(kCanvasHeight)}, room, kGrid);
    }
    for (float y = kRoomTop; y <= kRoomTop + kRoomHeight; y += 50.0f) {
        raster::drawLineClipped(canvas, {0, y}, {static_cast<float>(kCanvasWidth), y}, room, kGrid);
    }

    const Polygon outline = roomOutline();
    for (std::size_t i = 0; i < outline.vertices.size(); ++i) {
        const Point a = outline.vertices[i];
        const Point b = outline.vertices[(i + 1) % outline.vertices.size()];
        raster::drawThickLine(canvas, px(a.x), px(a.y), px(b.x), px(b.y), kWallThickness, kWall);
    }
    return canvas;
}

// ---- Furniture in LOCAL coordinates (centred on its own origin) -------------

// Armchair seen from above: a back along the top and two arms, the left arm
// wider than the right. It is deliberately NOT symmetric, so that a rotation
// and a mirror image can be told apart.
Polygon localArmchair() {
    Polygon chair;
    chair.vertices = {{-35, -35}, {35, -35}, {35, 35}, {22, 35}, {22, -12}, {-10, -12}, {-10, 35}, {-35, 35}};
    return chair;
}

Polygon localDesk() {
    return raster::makeRectangle(-50, -25, 100, 50);
}

// ---- 1. The basic transforms -------------------------------------------------
//
// Six cells. Each cell has its own origin c. The armchair is defined once in
// local coordinates; in each cell it is drawn with the matrix
//     M = T(c) * X
// where X is the transform being shown. Read right to left: X is applied to
// the local shape first (so it acts about the chair's own centre), then T(c)
// carries the result to the cell.
void imageBasic(const std::string& dir) {
    Canvas canvas = roomCanvas();
    const Polygon chair = localArmchair();

    struct Cell {
        const char* name;
        Point centre;
        Mat3 transform;
        Color color;
    };
    const Cell cells[] = {
        {"identity (the original)", {133, 140}, Mat3::identity(), kBlue},
        {"translation(45, 30)", {320, 140}, Mat3::translation(45, 30), kOrange},
        {"rotation(40) - clockwise", {507, 140}, Mat3::rotation(40), kSofa},
        {"scaling(1.5, 0.7)", {133, 340}, Mat3::scaling(1.5f, 0.7f), kPurple},
        {"reflectionAcrossYAxis()", {320, 340}, Mat3::reflectionAcrossYAxis(), kTeal},
        {"shear(0.6, 0)", {507, 340}, Mat3::shear(0.6f, 0), kRose},
    };

    // Cell borders.
    drawDashedSegment(canvas, {227, 44}, {227, 436}, kGuide);
    drawDashedSegment(canvas, {413, 44}, {413, 436}, kGuide);
    drawDashedSegment(canvas, {44, 240}, {596, 240}, kGuide);

    std::printf("  cells, left to right, top row then bottom row:\n");
    for (const Cell& cell : cells) {
        const Mat3 toCell = Mat3::translation(cell.centre.x, cell.centre.y);
        const Polygon before = xform::transformPolygon(toCell, chair);
        const Polygon after = xform::transformPolygon(toCell * cell.transform, chair);
        drawSolid(canvas, after, cell.color);
        drawGhost(canvas, before);
        std::printf("    %-26s determinant %5.2f, area %6.0f -> %6.0f cm^2\n", cell.name,
                    xform::determinant(cell.transform), raster::computeArea(before), raster::computeArea(after));
    }

    // The mirror line of the reflection cell: the chair's own y axis.
    drawDashedSegment(canvas, {320, 275}, {320, 405}, kPivot);

    save(canvas, dir, "transform_basic");
}

// ---- 2. Order matters --------------------------------------------------------
//
// Top left: the same translation T and rotation R, multiplied both ways.
//   T * R (blue):   rotate on the spot, then slide right.
//   R * T (orange): slide right, then rotate about the origin O - the slide
//                   itself gets swung round, so the chair ends up elsewhere.
// Right: the same scaling S and rotation R on a desk.
//   R * S (green):  stretch, then turn - still a rectangle.
//   S * R (purple): turn, then stretch along the ROOM's x axis - the desk is
//                   skewed into a parallelogram.
void imageOrder(const std::string& dir) {
    Canvas canvas = roomCanvas();

    // -- translation and rotation --
    const Polygon chair = localArmchair();
    const Point origin{130, 120};
    const Mat3 toOrigin = Mat3::translation(origin.x, origin.y);
    const Mat3 t = Mat3::translation(170, 0);
    const Mat3 r = Mat3::rotation(50);

    const Mat3 rotateThenMove = toOrigin * t * r;
    const Mat3 moveThenRotate = toOrigin * r * t;

    // Paths of the chair's centre.
    const Point centreA = xform::transformPoint(rotateThenMove, {0, 0});
    const Point centreB = xform::transformPoint(moveThenRotate, {0, 0});
    drawSegment(canvas, origin, centreA, kBlue);
    raster::drawCircleArc(canvas, px(origin.x), px(origin.y), 170, 0.0f, 50.0f, kOrange);
    drawDashedSegment(canvas, origin, centreB, kOrange);

    drawSolid(canvas, xform::transformPolygon(rotateThenMove, chair), kBlue);
    drawSolid(canvas, xform::transformPolygon(moveThenRotate, chair), kOrange);
    drawGhost(canvas, xform::transformPolygon(toOrigin, chair));     // start
    drawGhost(canvas, xform::transformPolygon(toOrigin * t, chair)); // after T only
    drawCross(canvas, origin, kPivot);

    std::printf("  T = translation(170, 0), R = rotation(50), about the cross at (%.0f, %.0f):\n", origin.x, origin.y);
    std::printf("    T * R (blue):   chair centre ends at (%.0f, %.0f)\n", centreA.x, centreA.y);
    std::printf("    R * T (orange): chair centre ends at (%.0f, %.0f)\n", centreB.x, centreB.y);

    // -- scaling and rotation --
    const Polygon desk = localDesk();
    const Mat3 s = Mat3::scaling(1.8f, 1.0f);
    const Mat3 r2 = Mat3::rotation(35);
    const Mat3 toTop = Mat3::translation(480, 130);
    const Mat3 toBottom = Mat3::translation(480, 340);

    const Polygon stretchThenTurn = xform::transformPolygon(toTop * r2 * s, desk);
    const Polygon turnThenStretch = xform::transformPolygon(toBottom * s * r2, desk);
    drawSolid(canvas, stretchThenTurn, kSofa);
    drawSolid(canvas, turnThenStretch, kPurple);
    drawGhost(canvas, xform::transformPolygon(toTop, desk));
    drawGhost(canvas, xform::transformPolygon(toBottom, desk));

    std::printf("  S = scaling(1.8, 1), R = rotation(35) on a desk:\n");
    std::printf("    R * S (green):  a turned rectangle\n");
    std::printf("    S * R (purple): skewed into a parallelogram\n");

    save(canvas, dir, "transform_order");
}

// ---- 3. Rotation about a pivot ----------------------------------------------
void imagePivot(const std::string& dir) {
    Canvas canvas = roomCanvas();

    // -- Door: rotationAbout(angle, hinge) --
    // The door is 80 cm wide, hinged at the left end of the gap in the bottom
    // wall, and lies along the wall when closed. Opening it into the room is
    // a NEGATIVE angle (counter-clockwise on screen).
    const Point hinge{440, 440};
    const Polygon closedDoor = raster::makeRectangle(440, 437, 80, 6);
    raster::drawThickLine(canvas, 440, 440, 520, 440, kWallThickness, kFloor); // the door gap
    raster::drawCircleArc(canvas, 440, 440, 80, 270.0f, 360.0f, kGuide);       // swing
    const float doorAngles[] = {0.0f, -22.5f, -45.0f, -67.5f, -90.0f};
    for (int i = 0; i < 5; ++i) {
        const Mat3 m = Mat3::rotationAbout(doorAngles[i], hinge);
        drawSolid(canvas, xform::transformPolygon(m, closedDoor), fade(kWood, 0.7f - 0.175f * static_cast<float>(i)));
    }
    drawCross(canvas, hinge, kPivot);

    // -- Desk turned about the CORNER OF THE ROOM --
    // The pivot is far from the desk, so the desk travels along a circle
    // around the corner while it turns.
    const Point corner{kRoomLeft, kRoomTop};
    const Polygon desk = xform::transformPolygon(Mat3::translation(260, 110), localDesk());
    const Point deskCentre{260, 110};
    raster::drawCircleArc(canvas, px(corner.x), px(corner.y), 231, 17.6f, 72.6f, kGuide);
    const float cornerAngles[] = {20.0f, 40.0f, 55.0f};
    for (int i = 0; i < 3; ++i) {
        const Mat3 m = Mat3::rotationAbout(cornerAngles[i], corner);
        drawDashedSegment(canvas, corner, xform::transformPoint(m, deskCentre), kGuide);
        drawSolid(canvas, xform::transformPolygon(m, desk), fade(kOrange, 0.5f - 0.25f * static_cast<float>(i)));
    }
    drawDashedSegment(canvas, corner, deskCentre, kGuide);
    drawGhost(canvas, desk);
    drawCross(canvas, corner, kPivot);

    // -- The same desk shape turned about ITS OWN CENTRE --
    // The pivot is the centre, so the desk turns on the spot.
    const Point ownCentre{470, 150};
    const Polygon desk2 = xform::transformPolygon(Mat3::translation(ownCentre.x, ownCentre.y), localDesk());
    drawSolid(canvas, xform::transformPolygon(Mat3::rotationAbout(55.0f, ownCentre), desk2), kBlue);
    drawGhost(canvas, desk2);
    drawCross(canvas, ownCentre, kPivot);

    const Point movedCentre = xform::transformPoint(Mat3::rotationAbout(55.0f, corner), deskCentre);
    std::printf("  door: rotationAbout(angle, hinge) for 0, -22.5, -45, -67.5, -90 degrees\n");
    std::printf("  desk about the room corner by 55: centre (260, 110) -> (%.0f, %.0f)\n", movedCentre.x, movedCentre.y);
    std::printf("  desk about its own centre by 55:  centre stays at (%.0f, %.0f)\n", ownCentre.x, ownCentre.y);

    save(canvas, dir, "transform_pivot");
}

// ---- 4. Reflection and shear -------------------------------------------------
void imageMirror(const std::string& dir) {
    Canvas canvas = roomCanvas();
    const Polygon room = roomOutline();

    // The L-shaped sofa from Checkpoint 1, in the top-right corner.
    Polygon sofa;
    sofa.vertices = {{380, 60}, {580, 60}, {580, 220}, {530, 220}, {530, 110}, {380, 110}};

    // Mirror in the vertical line through the middle of the room, x = 320.
    // reflectionAcrossYAxis() mirrors in the line x = 0, so (like a pivot
    // rotation) move the mirror line onto the y axis, reflect, move back:
    //   M = T(320, 0) * reflectY * T(-320, 0)
    const float mirrorX = kRoomLeft + kRoomWidth / 2.0f;
    const Mat3 mirror = Mat3::translation(mirrorX, 0) * Mat3::reflectionAcrossYAxis() * Mat3::translation(-mirrorX, 0);
    const Polygon mirroredSofa = xform::transformPolygon(mirror, sofa);

    // Other pieces, to show the shadow on different shapes.
    const Polygon wardrobe = raster::makeRectangle(70, 280, 60, 130);
    const Polygon table = roomcraft::makeCircleFootprint({330, 330}, 45, 48);
    const Polygon armchair = xform::transformPolygon(Mat3::translation(500, 340) * Mat3::rotation(-20), localArmchair());

    // Drop shadow = a sheared, shifted copy of the footprint, drawn first.
    // The shear is taken about the top-left corner p of the piece's bounding
    // box, so the top edge stays under the piece and the shadow leans out to
    // the right further down:
    //   S = T(6, 10) * T(p) * shear(0.18, 0) * T(-p)
    const auto shadowOf = [](const Polygon& piece) {
        const raster::Rect box = raster::computeBoundingBox(piece);
        const Mat3 m = Mat3::translation(6, 10) * Mat3::translation(box.xmin, box.ymin) *
                       Mat3::shear(0.18f, 0) * Mat3::translation(-box.xmin, -box.ymin);
        return xform::transformPolygon(m, piece);
    };
    const Polygon* pieces[] = {&sofa, &mirroredSofa, &wardrobe, &table, &armchair};
    const Color colors[] = {kSofa, kTeal, kWood, kWood, kBlue};
    for (const Polygon* piece : pieces) {
        raster::fillPolygonScanline(canvas, shadowOf(*piece), kShadow);
    }
    for (int i = 0; i < 5; ++i) {
        drawSolid(canvas, *pieces[i], colors[i]);
    }

    // The mirror line.
    drawDashedSegment(canvas, {mirrorX, kRoomTop + 6}, {mirrorX, kRoomTop + kRoomHeight - 6}, kPivot);

    const Furniture original{"L-sofa", sofa, kSofa, Pose{}};
    const Furniture mirrored{"mirrored L-sofa", mirroredSofa, kTeal, Pose{}};
    std::printf("  mirror line x = %.0f, determinant of the mirror matrix %.0f\n", mirrorX, xform::determinant(mirror));
    std::printf("    original sofa: area %.0f cm^2, signed %.0f, fits in room: %s\n", raster::computeArea(sofa),
                raster::computeSignedArea(sofa), roomcraft::fitsInRoom(original, room) ? "yes" : "no");
    std::printf("    mirrored sofa: area %.0f cm^2, signed %.0f, fits in room: %s\n", raster::computeArea(mirroredSofa),
                raster::computeSignedArea(mirroredSofa), roomcraft::fitsInRoom(mirrored, room) ? "yes" : "no");

    save(canvas, dir, "transform_mirror");
}

// ---- 5. Pose -----------------------------------------------------------------
//
// ONE local footprint, never modified, placed five times only by changing the
// numbers in Furniture::pose. hitTest / fitsInRoom / drawFurniture all work
// on worldFootprint(piece) = poseMatrix(pose) * footprint.
void imagePose(const std::string& dir) {
    Canvas canvas = roomCanvas();
    const Polygon room = roomOutline();
    const Polygon chair = localArmchair();

    struct Placement {
        Point position;
        float angle;
        float scaleX;
        float scaleY;
        Color color;
    };
    const Placement placements[] = {
        {{130, 130}, 0.0f, 1.0f, 1.0f, kBlue},      // as designed
        {{320, 250}, 35.0f, 1.6f, 1.6f, kSofa},     // bigger and turned
        {{150, 350}, 180.0f, 1.0f, 1.0f, kPurple},  // facing the other way
        {{480, 360}, 0.0f, -1.0f, 1.0f, kTeal},     // mirrored: negative scaleX
        {{585, 140}, -25.0f, 1.0f, 1.0f, kOrange},  // pushed through the right wall
        {{330, 62}, 90.0f, 1.0f, 1.0f, kRose},      // turned into the top wall
    };

    std::printf("  one local armchair footprint, placed by pose:\n");
    std::printf("    position      angle  scale        fits in room\n");
    for (const Placement& p : placements) {
        Furniture piece{"armchair", chair, p.color, Pose{}};
        piece.pose.position = p.position;
        piece.pose.angleDegrees = p.angle;
        piece.pose.scaleX = p.scaleX;
        piece.pose.scaleY = p.scaleY;

        const bool fits = roomcraft::fitsInRoom(piece, room);
        if (!fits) {
            piece.color = kWarning;
        }
        roomcraft::drawFurniture(canvas, piece);
        drawCross(canvas, p.position, fits ? kOutline : kWhite);
        std::printf("    (%3.0f, %3.0f)  %6.1f  (%4.1f, %3.1f)  %s\n", p.position.x, p.position.y, p.angle,
                    p.scaleX, p.scaleY, fits ? "yes" : "NO - drawn in red");
    }

    // A click is tested against the PLACED footprint, not the local one.
    Furniture big{"armchair", chair, kSofa, Pose{}};
    big.pose.position = {320, 250};
    big.pose.angleDegrees = 35.0f;
    big.pose.scaleX = big.pose.scaleY = 1.6f;
    // (343, 217) is on the back of the big chair; (320, 250) is its pose
    // position, which lies on the open seat between the arms (the footprint
    // is concave), so it is a miss; (20, -30) is on the back of the LOCAL
    // footprint, where nothing is drawn.
    const auto result = [&big](Point click) { return roomcraft::hitTest(big, click) ? "hit" : "miss"; };
    std::printf("  clicks on the big green chair: back (343, 217) %s; open seat (320, 250) %s; "
                "local coordinates (20, -30) %s\n",
                result({343, 217}), result({320, 250}), result({20, -30}));

    save(canvas, dir, "transform_pose");
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : std::string(ROOMCRAFT_DEMO_OUTPUT_DIR) + "/cp2";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    std::printf("RoomCraft Checkpoint 2 transform demo - writing images to %s\n", dir.c_str());

    std::printf("1. Basic transforms\n");
    imageBasic(dir);
    std::printf("2. Order matters\n");
    imageOrder(dir);
    std::printf("3. Rotation about a pivot\n");
    imagePivot(dir);
    std::printf("4. Reflection and shear\n");
    imageMirror(dir);
    std::printf("5. Pose\n");
    imagePose(dir);

    if (failures > 0) {
        std::printf("%d image(s) could not be written\n", failures);
        return 1;
    }
    std::printf("Done.\n");
    return 0;
}
