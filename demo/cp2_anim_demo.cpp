// RoomCraft Checkpoint 2, Part 4 demo: the animation engine.
//
// Writes four animated GIFs to demo_output/cp2/ (or to the directory given as
// the first command-line argument):
//
//   anim_easing.gif     four identical armchairs sliding across the room, one
//                       per easing curve (Linear, EaseIn, EaseOut, EaseInOut),
//                       labelled with output::drawText
//   anim_arrange.gif    an empty room where each piece of furniture slides in,
//                       turns and scales into place from keyframes, one after
//                       another
//   anim_collision.gif  a piece pushed through the walls, turning red on every
//                       frame where roomcraft::fitsInRoom fails and returning
//                       to its normal colour when back inside
//   anim_vacuum.gif     a round robot vacuum following a Catmull-Rom spline
//                       between the furniture at constant speed, facing its
//                       direction of travel and leaving a dashed trail

#include "anim/easing.h"
#include "anim/path.h"
#include "anim/track.h"
#include "output/animation_writer.h"
#include "output/text.h"
#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/clipping.h"
#include "raster/curves.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "xform/mat3.h"
#include "xform/pose.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifndef ROOMCRAFT_DEMO_OUTPUT_DIR
#define ROOMCRAFT_DEMO_OUTPUT_DIR "demo_output"
#endif

using anim::Easing;
using anim::FixedTimestep;
using anim::PoseTrack;
using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Polygon;
using roomcraft::Furniture;
using xform::Mat3;
using xform::Pose;

namespace {

// ---- Palette and room geometry (matching demo/roomcraft_demo.cpp) -----------
const Color kWhite{255, 255, 255};
const Color kOutline{40, 40, 40};
const Color kWall{60, 60, 70};
const Color kFloor{238, 226, 205};
const Color kGrid{214, 198, 172};
const Color kWindow{90, 160, 220};
const Color kDoorArc{150, 150, 150};
const Color kGhost{165, 155, 140};
const Color kGuide{180, 170, 150};
const Color kWood{181, 136, 99};
const Color kChair{96, 125, 160};
const Color kBed{170, 190, 215};
const Color kPillow{250, 250, 245};
const Color kSofa{120, 150, 110};
const Color kRug{190, 80, 90};
const Color kRugInner{225, 150, 150};
const Color kGlassTable{150, 110, 80};
const Color kPlant{70, 140, 70};
const Color kPot{170, 100, 60};
const Color kWarning{220, 30, 30};
const Color kWarningFill{235, 95, 95};
const Color kOkGreen{40, 130, 60};
const Color kVacuumBody{55, 65, 75};
const Color kVacuumTop{90, 105, 120};
const Color kVacuumLed{70, 210, 120};
const Color kTrail{70, 130, 190};

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 480;
constexpr float kRoomLeft = 40.0f;
constexpr float kRoomTop = 40.0f;
constexpr float kRoomWidth = 560.0f;  // 5.6 m
constexpr float kRoomHeight = 400.0f; // 4.0 m
constexpr int kWallThickness = 8;

constexpr int kDoorLeft = 440;
constexpr int kDoorRight = 520;
constexpr int kWindowLeft = 250;
constexpr int kWindowRight = 370;

int failures = 0;

int px(float v) {
    return raster::roundToPixel(v);
}

Polygon roomOutline() {
    return raster::makeRectangle(kRoomLeft, kRoomTop, kRoomWidth, kRoomHeight);
}

raster::Rect roomRect() {
    return raster::Rect{kRoomLeft, kRoomTop, kRoomLeft + kRoomWidth, kRoomTop + kRoomHeight};
}

void reportGif(bool ok, const output::AnimationWriter& writer) {
    if (ok) {
        std::error_code ec;
        const auto bytes = std::filesystem::file_size(writer.path(), ec);
        std::printf("  wrote %-38s %3d frames, %.2f s/frame, %3d colours, %6.1f KB\n",
                    writer.path().c_str(), writer.frameCount(),
                    writer.delayCentiseconds() / 100.0, writer.colorCount(),
                    ec ? 0.0 : static_cast<double>(bytes) / 1024.0);
    } else {
        std::printf("  ERROR: could not write %s\n", writer.path().c_str());
        ++failures;
    }
}

void drawDashedPolygon(Canvas& canvas, const Polygon& poly, Color color) {
    const std::vector<Point>& v = poly.vertices;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const Point a = v[i];
        const Point b = v[(i + 1) % v.size()];
        raster::drawDashedLine(canvas, px(a.x), px(a.y), px(b.x), px(b.y), 5, 4, color);
    }
}

void drawCross(Canvas& canvas, Point p, int size, Color color) {
    const int x = px(p.x);
    const int y = px(p.y);
    raster::drawLineBresenham(canvas, x - size, y - size, x + size, y + size, color);
    raster::drawLineBresenham(canvas, x - size, y + size, x + size, y - size, color);
}

// ---- CP1 room drawing -------------------------------------------------------

void drawFloorAndGrid(Canvas& canvas) {
    canvas.clear(kWhite.r, kWhite.g, kWhite.b);
    raster::fillPolygonScanline(canvas, roomOutline(), kFloor);
    const raster::Rect room = roomRect();
    for (int x = 0; x <= canvas.width(); x += 50) {
        raster::drawLineClipped(canvas, Point{static_cast<float>(x), 0.0f},
                                Point{static_cast<float>(x), static_cast<float>(canvas.height())}, room, kGrid);
    }
    for (int y = 0; y <= canvas.height(); y += 50) {
        raster::drawLineClipped(canvas, Point{0.0f, static_cast<float>(y)},
                                Point{static_cast<float>(canvas.width()), static_cast<float>(y)}, room, kGrid);
    }
}

void drawWallsAndDoor(Canvas& canvas, bool includeDoorSwing = true) {
    const int left = static_cast<int>(kRoomLeft);
    const int top = static_cast<int>(kRoomTop);
    const int right = static_cast<int>(kRoomLeft + kRoomWidth);
    const int bottom = static_cast<int>(kRoomTop + kRoomHeight);
    const int half = kWallThickness / 2;

    raster::drawThickLine(canvas, left - half, top, kWindowLeft, top, kWallThickness, kWall);
    raster::drawThickLine(canvas, kWindowRight, top, right + half, top, kWallThickness, kWall);
    raster::drawThickLine(canvas, left - half, bottom, kDoorLeft, bottom, kWallThickness, kWall);
    raster::drawThickLine(canvas, kDoorRight, bottom, right + half, bottom, kWallThickness, kWall);
    raster::drawThickLine(canvas, left, top - half, left, bottom + half, kWallThickness, kWall);
    raster::drawThickLine(canvas, right, top - half, right, bottom + half, kWallThickness, kWall);

    for (int offset : {-3, 0, 3}) {
        raster::drawLineBresenham(canvas, kWindowLeft, top + offset, kWindowRight, top + offset, kWindow);
    }

    if (includeDoorSwing) {
        const int hingeX = kDoorLeft;
        const int hingeY = bottom;
        const int width = kDoorRight - kDoorLeft;
        raster::drawThickLine(canvas, hingeX, hingeY, hingeX, hingeY - width, 3, kWall);
        raster::drawCircleArc(canvas, hingeX, hingeY, width, 270.0f, 360.0f, kDoorArc);
    }
}

void drawHeaderAndFooter(Canvas& canvas, const std::string& title, int frame, int totalFrames,
                         const std::string& footer) {
    output::drawText(canvas, 40, 12, title, kOutline, 2);
    const std::string counter = "frame " + std::to_string(frame + 1) + "/" + std::to_string(totalFrames);
    output::drawText(canvas, kCanvasWidth - 40 - output::textWidth(counter, 2), 12, counter, kChair, 2);
    if (!footer.empty()) {
        output::drawText(canvas, 40, 454, footer, kOutline, 1);
    }
}

// ---- Local furniture footprints (centred on (0, 0)) -------------------------

Polygon localArmchair(float halfSize = 30.0f) {
    const float s = halfSize;
    const float inner = halfSize * 0.62f;
    const float seatBack = -halfSize * 0.35f;
    Polygon chair;
    chair.vertices = {{-s, -s}, {s, -s}, {s, s}, {inner, s}, {inner, seatBack}, {-inner, seatBack}, {-inner, s}, {-s, s}};
    return chair;
}

Polygon localEllipse(float rx, float ry, int sides = 48) {
    Polygon poly = roomcraft::makeCircleFootprint({0.0f, 0.0f}, rx, sides);
    raster::scale(poly, 1.0f, ry / rx, {0.0f, 0.0f});
    return poly;
}

Polygon localKidneyTable(float s) {
    auto at = [&](float dx, float dy) { return Point{dx * s, dy * s}; };
    const std::vector<std::vector<Point>> pieces = {
        raster::sampleBezierCubic(at(-45, 0), at(-45, -38), at(45, -38), at(45, 0), 20),
        raster::sampleBezierCubic(at(45, 0), at(45, 28), at(18, 30), at(0, 16), 14),
        raster::sampleBezierCubic(at(0, 16), at(-18, 30), at(-45, 28), at(-45, 0), 14),
    };
    Polygon kidney;
    for (const std::vector<Point>& piece : pieces) {
        kidney.vertices.insert(kidney.vertices.end(), piece.begin(), piece.end() - 1);
    }
    return kidney;
}

// L-shaped sofa centred around (480, 140) in room coordinates -> shifted so
// (0, 0) is its local reference point.
Polygon localLSofa() {
    Polygon sofa;
    sofa.vertices = {{-100, -80}, {100, -80}, {100, 80}, {50, 80}, {50, -30}, {-100, -30}};
    return sofa;
}

// Draws a piece with decorative inner details (rug border, bed pillows, plant
// foliage) transformed by the piece's current Pose so details follow rotation
// and scaling during animations.
void drawDetailedFurniture(Canvas& canvas, const Furniture& piece) {
    roomcraft::drawFurniture(canvas, piece);
    const Mat3 m = xform::poseMatrix(piece.pose);

    if (piece.name == "Rug") {
        const Polygon inner = xform::transformPolygon(m, localEllipse(49.0f, 33.0f, 40));
        raster::fillPolygonScanline(canvas, inner, kRugInner);
        raster::drawPolygonOutline(canvas, inner, kRug);
    } else if (piece.name == "Bed") {
        const Polygon leftPillow = xform::transformPolygon(
            m * Mat3::translation(-32.0f, -78.0f), localEllipse(26.0f, 12.0f, 24));
        const Polygon rightPillow = xform::transformPolygon(
            m * Mat3::translation(32.0f, -78.0f), localEllipse(26.0f, 12.0f, 24));
        raster::fillPolygonScanline(canvas, leftPillow, kPillow);
        raster::drawPolygonOutline(canvas, leftPillow, kOutline);
        raster::fillPolygonScanline(canvas, rightPillow, kPillow);
        raster::drawPolygonOutline(canvas, rightPillow, kOutline);
        const Point a = xform::transformPoint(m, {-68.0f, -40.0f});
        const Point b = xform::transformPoint(m, {68.0f, -40.0f});
        raster::drawThickLine(canvas, px(a.x), px(a.y), px(b.x), px(b.y), 2, kOutline);
    } else if (piece.name == "Plant") {
        const Polygon leaves = xform::transformPolygon(
            m, roomcraft::makeCircleFootprint({0.0f, 0.0f}, 10.0f, 24));
        raster::fillPolygonScanline(canvas, leaves, kPlant);
    }
}

// ---- 1. Easing comparison (anim_easing.gif) ---------------------------------
//
// Four identical armchairs slide across the room in parallel horizontal lanes.
// All four share the exact same start keyframe (t = 0.2 s, x = 215) and finish
// keyframe (t = 2.4 s, x = 545), differing ONLY by the Easing curve on the
// second keyframe. A vertical reference line moves at the Linear speed so the
// viewer can see EaseIn lag behind at the start and EaseOut pull ahead.
void demoEasing(const std::string& dir) {
    const FixedTimestep step(20, 3.0f);
    output::AnimationWriter writer(dir, "anim_easing", kCanvasWidth, kCanvasHeight, step.framesPerSecond);
    Canvas canvas(kCanvasWidth, kCanvasHeight);

    struct Lane {
        const char* name;
        const char* formula;
        Easing easing;
        float y;
        Color color;
    };
    const Lane lanes[] = {
        {"Linear",    "f(t) = t",            Easing::Linear,    100.0f, kChair},
        {"EaseIn",    "f(t) = t^2",          Easing::EaseIn,    190.0f, kWood},
        {"EaseOut",   "f(t) = 1-(1-t)^2",    Easing::EaseOut,   280.0f, kSofa},
        {"EaseInOut", "f(t) = 3t^2-2t^3",    Easing::EaseInOut, 370.0f, kRug},
    };

    constexpr float kStartX = 220.0f;
    constexpr float kEndX = 545.0f;
    constexpr float kMoveStart = 0.20f;
    constexpr float kMoveEnd = 2.40f;

    const Polygon chairShape = localArmchair(26.0f);
    std::vector<PoseTrack> tracks(4);
    for (int i = 0; i < 4; ++i) {
        tracks[static_cast<std::size_t>(i)].addKey(
            kMoveStart, Pose{{kStartX, lanes[i].y}, 90.0f, 1.0f, 1.0f}, Easing::Linear);
        tracks[static_cast<std::size_t>(i)].addKey(
            kMoveEnd, Pose{{kEndX, lanes[i].y}, 90.0f, 1.0f, 1.0f}, lanes[i].easing);
    }

    bool ok = true;
    for (int frame = 0; frame < step.frameCount(); ++frame) {
        const float t = step.frameTime(frame);
        drawFloorAndGrid(canvas);

        // Start and finish lines across all four lanes.
        raster::drawDashedLine(canvas, px(kStartX), 60, px(kStartX), 418, 4, 4, kGuide);
        raster::drawDashedLine(canvas, px(kEndX), 60, px(kEndX), 418, 4, 4, kGuide);
        output::drawText(canvas, px(kStartX) - 15, 50, "t=0", kOutline, 1);
        output::drawText(canvas, px(kEndX) - 15, 50, "t=1", kOutline, 1);

        // Moving vertical guide at the Linear position so relative lead/lag is obvious.
        const float linearX = tracks[0].sample(t).position.x;
        raster::drawDashedLine(canvas, px(linearX), 65, px(linearX), 415, 3, 5, kChair);

        for (int i = 0; i < 4; ++i) {
            const Lane& lane = lanes[i];
            const int y = px(lane.y);

            // Labels on the left side of the lane.
            output::drawText(canvas, 54, y - 14, lane.name, kOutline, 2);
            output::drawText(canvas, 54, y + 6, lane.formula, kOutline, 1);

            // Track line and ghost at destination.
            raster::drawDashedLine(canvas, px(kStartX), y, px(kEndX), y, 6, 4, kGuide);
            Furniture ghost{"ghost", chairShape, kGhost, Pose{{kEndX, lane.y}, 90.0f, 1.0f, 1.0f}};
            drawDashedPolygon(canvas, roomcraft::worldFootprint(ghost), kGhost);

            // Animated armchair at frame time t.
            Furniture chair{lane.name, chairShape, lane.color, tracks[static_cast<std::size_t>(i)].sample(t)};
            roomcraft::drawFurniture(canvas, chair);
        }

        drawWallsAndDoor(canvas, false);
        const float normT = std::min(std::max((t - kMoveStart) / (kMoveEnd - kMoveStart), 0.0f), 1.0f);
        char footer[128];
        std::snprintf(footer, sizeof(footer),
                      "Fixed timestep: 20 fps, t = %.2f s (normalised u = %.2f). All four start and finish together.",
                      t, normT);
        drawHeaderAndFooter(canvas, "Easing curves compared", frame, step.frameCount(), footer);

        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;
    reportGif(ok, writer);
}

// ---- 2. Furniture arranging itself (anim_arrange.gif) -----------------------
//
// Starts with an empty room. Each piece of furniture has its own PoseTrack
// that slides it in through the doorway, turns it (including across the 360/0
// angle seam via lerpAngle) and scales it from 0.25x up to 1.0x into its final
// Checkpoint 1 floor-plan pose, one piece after another.
void demoArrange(const std::string& dir) {
    const FixedTimestep step(20, 4.0f);
    output::AnimationWriter writer(dir, "anim_arrange", kCanvasWidth, kCanvasHeight, step.framesPerSecond);
    Canvas canvas(kCanvasWidth, kCanvasHeight);

    struct ScheduledPiece {
        Furniture piece;
        PoseTrack track;
        float startTime = 0.0f;
        float endTime = 0.0f;
    };

    struct Spec {
        const char* name;
        Polygon localFootprint;
        Color color;
        Pose targetPose;
        float startAngle;
    };

    const std::vector<Spec> specs = {
        {"Rug",          localEllipse(56.0f, 40.0f, 48),                       kRug,        Pose{{470, 182},   0.0f, 1.0f, 1.0f}, 270.0f},
        {"Bed",          raster::makeRectangle(-70, -100, 140, 200),           kBed,        Pose{{130, 160},   0.0f, 1.0f, 1.0f}, 340.0f},
        {"Nightstand",   raster::makeRectangle(-18, -18, 36, 36),              kWood,       Pose{{226,  78},   0.0f, 1.0f, 1.0f}, 180.0f},
        {"Wardrobe",     raster::makeRectangle(-30, -70, 60, 140),             kWood,       Pose{{ 78, 360},   0.0f, 1.0f, 1.0f},  90.0f},
        {"L-sofa",       localLSofa(),                                         kSofa,       Pose{{480, 140},   0.0f, 1.0f, 1.0f}, 350.0f},
        {"Armchair",     localArmchair(35.0f),                                 kSofa,       Pose{{310, 110},  25.0f, 1.0f, 1.0f}, 345.0f},
        {"Coffee table", localKidneyTable(0.7f),                               kGlassTable, Pose{{470, 182},   0.0f, 1.0f, 1.0f}, 160.0f},
        {"Dining table", roomcraft::makeCircleFootprint({0, 0}, 45.0f, 48),    kWood,       Pose{{200, 360},   0.0f, 1.0f, 1.0f}, 180.0f},
        {"Chair 1",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24),    kChair,      Pose{{200, 298},   0.0f, 1.0f, 1.0f}, 270.0f},
        {"Chair 2",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24),    kChair,      Pose{{262, 360},   0.0f, 1.0f, 1.0f}, 350.0f},
        {"Chair 3",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24),    kChair,      Pose{{200, 422},   0.0f, 1.0f, 1.0f},  90.0f},
        {"Chair 4",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24),    kChair,      Pose{{138, 360},   0.0f, 1.0f, 1.0f}, 180.0f},
        {"Plant",        roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24),    kPot,        Pose{{574, 412},   0.0f, 1.0f, 1.0f}, 355.0f},
    };

    const Point doorway{480.0f, 425.0f};
    const float stagger = 0.22f;
    const float moveDuration = 0.65f;

    std::vector<ScheduledPiece> items;
    items.reserve(specs.size());
    for (std::size_t i = 0; i < specs.size(); ++i) {
        const Spec& s = specs[i];
        ScheduledPiece item;
        item.piece = Furniture{s.name, s.localFootprint, s.color, s.targetPose};
        item.startTime = 0.15f + static_cast<float>(i) * stagger;
        item.endTime = item.startTime + moveDuration;

        // Three keyframes per piece:
        //   1. At the doorway, small (0.25x) and rotated to startAngle.
        //   2. Mid-flight waypoint inside the room, 0.75x scale, EaseIn.
        //   3. Final room pose at 1.0x scale, settling smoothly with EaseOut.
        const Pose entryPose{doorway, s.startAngle, 0.25f, 0.25f};
        const Point midPt{
            anim::lerp(doorway.x, s.targetPose.position.x, 0.55f),
            anim::lerp(doorway.y, s.targetPose.position.y, 0.55f) - 20.0f,
        };
        const Pose midPose{
            midPt,
            anim::lerpAngle(s.startAngle, s.targetPose.angleDegrees, 0.6f),
            0.75f,
            0.75f,
        };

        item.track.addKey(item.startTime, entryPose, Easing::Linear);
        item.track.addKey(item.startTime + moveDuration * 0.45f, midPose, Easing::EaseIn);
        item.track.addKey(item.endTime, s.targetPose, Easing::EaseOut);
        items.push_back(item);
    }

    bool ok = true;
    for (int frame = 0; frame < step.frameCount(); ++frame) {
        const float t = step.frameTime(frame);
        drawFloorAndGrid(canvas);

        std::string activeName = "Room complete";
        int placedCount = 0;
        for (ScheduledPiece& item : items) {
            if (t < item.startTime) {
                continue; // Not yet entered the room.
            }
            item.piece.pose = item.track.sample(t);
            if (t < item.endTime) {
                activeName = "Arranging: " + item.piece.name;
                // Faint guide line from the piece to its target position.
                const Point target = item.track.keys().back().pose.position;
                raster::drawDashedLine(canvas, px(item.piece.pose.position.x), px(item.piece.pose.position.y),
                                       px(target.x), px(target.y), 4, 4, kGuide);
            } else {
                ++placedCount;
            }
            drawDetailedFurniture(canvas, item.piece);
        }

        drawWallsAndDoor(canvas, true);
        const std::string footer = activeName + "  (" + std::to_string(placedCount) + "/" +
                                   std::to_string(items.size()) + " settled, t = " +
                                   std::to_string(static_cast<int>(t * 10.0f) / 10) + "." +
                                   std::to_string(static_cast<int>(t * 10.0f) % 10) + " s)";
        drawHeaderAndFooter(canvas, "Keyframe room arrangement", frame, step.frameCount(), footer);

        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;
    reportGif(ok, writer);
}

// ---- 3. Wall collision detection (anim_collision.gif) -----------------------
//
// A desk is animated along a PoseTrack that pushes it through the right wall,
// pulls it back into the room, then slides and turns it so a corner pokes
// through the top wall before returning to safety. On every frame where
// roomcraft::fitsInRoom(desk, room) returns false, the piece turns bright red;
// when it returns inside, its normal wood colour is restored.
void demoCollision(const std::string& dir) {
    const FixedTimestep step(20, 3.5f);
    output::AnimationWriter writer(dir, "anim_collision", kCanvasWidth, kCanvasHeight, step.framesPerSecond);
    Canvas canvas(kCanvasWidth, kCanvasHeight);

    const Polygon room = roomOutline();

    // Context furniture on the left side of the room.
    std::vector<Furniture> staticPieces = {
        {"Bed",        raster::makeRectangle(-70, -100, 140, 200), kBed,  Pose{{130, 160}, 0.0f, 1.0f, 1.0f}},
        {"Nightstand", raster::makeRectangle(-18, -18, 36, 36),    kWood, Pose{{226,  78}, 0.0f, 1.0f, 1.0f}},
        {"Wardrobe",   raster::makeRectangle(-30, -70, 60, 140),   kWood, Pose{{ 78, 360}, 0.0f, 1.0f, 1.0f}},
    };

    // Moving desk (130 cm x 64 cm in local coordinates).
    Furniture desk{"Desk", raster::makeRectangle(-65, -32, 130, 64), kWood, Pose{}};
    PoseTrack deskTrack;
    // 0.0 s: safely inside the room at (410, 260).
    deskTrack.addKey(0.0f, Pose{{410.0f, 260.0f},   0.0f, 1.0f, 1.0f}, Easing::EaseInOut);
    // 0.9 s: pushed through the right wall (x = 600; right edge reaches 645).
    deskTrack.addKey(0.9f, Pose{{580.0f, 260.0f},   0.0f, 1.0f, 1.0f}, Easing::EaseInOut);
    // 1.3 s: held outside briefly while turned slightly.
    deskTrack.addKey(1.3f, Pose{{585.0f, 260.0f},  15.0f, 1.0f, 1.0f}, Easing::EaseInOut);
    // 2.0 s: pulled back safely inside the room -> turns back to wood colour.
    deskTrack.addKey(2.0f, Pose{{420.0f, 140.0f},   0.0f, 1.0f, 1.0f}, Easing::EaseInOut);
    // 2.6 s: moved near the top wall (y = 40) and rotated 38 deg so a corner pokes out.
    deskTrack.addKey(2.6f, Pose{{420.0f,  72.0f},  38.0f, 1.0f, 1.0f}, Easing::EaseInOut);
    // 3.3 s: rotated level and moved back inside.
    deskTrack.addKey(3.3f, Pose{{410.0f, 260.0f},   0.0f, 1.0f, 1.0f}, Easing::EaseInOut);

    int collisionFrames = 0;
    bool ok = true;
    for (int frame = 0; frame < step.frameCount(); ++frame) {
        const float t = step.frameTime(frame);
        drawFloorAndGrid(canvas);

        for (const Furniture& p : staticPieces) {
            drawDetailedFurniture(canvas, p);
        }

        desk.pose = deskTrack.sample(t);
        const bool fits = roomcraft::fitsInRoom(desk, room);
        const float totalArea = raster::computeArea(roomcraft::worldFootprint(desk));
        const float insideArea = roomcraft::areaInsideRoom(desk, room);
        const int pctInside = totalArea > 0.0f
                                  ? static_cast<int>(std::lround(100.0 * insideArea / totalArea))
                                  : 0;

        if (fits) {
            desk.color = kWood;
            roomcraft::drawFurniture(canvas, desk);
        } else {
            ++collisionFrames;
            // Fill the whole offending piece in red and also draw the clipped
            // boundary and protruding dashed outline.
            desk.color = kWarningFill;
            roomcraft::drawFurniture(canvas, desk);
            drawDashedPolygon(canvas, roomcraft::worldFootprint(desk), kWarning);
        }
        drawCross(canvas, desk.pose.position, 4, fits ? kOutline : kWhite);

        drawWallsAndDoor(canvas, true);

        // Status banner inside the top of the room.
        const std::string status = fits
            ? "fitsInRoom: YES (100% inside)"
            : "fitsInRoom: NO - WALL COLLISION (" + std::to_string(pctInside) + "% inside)";
        output::drawText(canvas, 280, 56, status, fits ? kOkGreen : kWarning, 2);

        drawHeaderAndFooter(canvas, "Wall collision check (fitsInRoom)", frame, step.frameCount(),
                            "Piece turns red whenever Sutherland-Hodgman clipped area < footprint area.");

        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;
    reportGif(ok, writer);
    std::printf("    collision detected on %d / %d frames\n", collisionFrames, step.frameCount());
}

// ---- 4. Robot vacuum on a constant-speed path (anim_vacuum.gif) -------------
//
// A round robot vacuum cleans the furnished room along a smooth Catmull-Rom
// spline through 8 waypoints between the furniture. Its pose is sampled with
// anim::poseOnPathConstantSpeed() so it moves at constant cm/s even though the
// waypoints are unevenly spaced, turns to face its direction of travel, and
// leaves a dashed trail behind it.
void demoVacuum(const std::string& dir) {
    const FixedTimestep step(20, 4.0f);
    output::AnimationWriter writer(dir, "anim_vacuum", kCanvasWidth, kCanvasHeight, step.framesPerSecond);
    Canvas canvas(kCanvasWidth, kCanvasHeight);

    const std::vector<Furniture> pieces = {
        {"Rug",          localEllipse(56.0f, 40.0f, 48),                    kRug,        Pose{{470, 182},  0.0f, 1.0f, 1.0f}},
        {"Bed",          raster::makeRectangle(-70, -100, 140, 200),        kBed,        Pose{{130, 160},  0.0f, 1.0f, 1.0f}},
        {"Nightstand",   raster::makeRectangle(-18, -18, 36, 36),           kWood,       Pose{{226,  78},  0.0f, 1.0f, 1.0f}},
        {"Wardrobe",     raster::makeRectangle(-30, -70, 60, 140),          kWood,       Pose{{ 78, 360},  0.0f, 1.0f, 1.0f}},
        {"L-sofa",       localLSofa(),                                      kSofa,       Pose{{480, 140},  0.0f, 1.0f, 1.0f}},
        {"Armchair",     localArmchair(35.0f),                              kSofa,       Pose{{310, 110}, 25.0f, 1.0f, 1.0f}},
        {"Dining table", roomcraft::makeCircleFootprint({0, 0}, 45.0f, 48), kWood,       Pose{{200, 360},  0.0f, 1.0f, 1.0f}},
        {"Chair 1",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24), kChair,      Pose{{200, 298},  0.0f, 1.0f, 1.0f}},
        {"Chair 2",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24), kChair,      Pose{{262, 360},  0.0f, 1.0f, 1.0f}},
        {"Chair 3",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24), kChair,      Pose{{200, 422},  0.0f, 1.0f, 1.0f}},
        {"Chair 4",      roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24), kChair,      Pose{{138, 360},  0.0f, 1.0f, 1.0f}},
        {"Coffee table", localKidneyTable(0.7f),                            kGlassTable, Pose{{470, 182},  0.0f, 1.0f, 1.0f}},
        {"Plant",        roomcraft::makeCircleFootprint({0, 0}, 14.0f, 24), kPot,        Pose{{574, 412},  0.0f, 1.0f, 1.0f}},
    };

    // Waypoints threading between the furniture with deliberately varied
    // spacing (short bend around the armchair, long sweep across the floor).
    const std::vector<Point> waypoints = {
        {480.0f, 405.0f}, // 0: start near doorway
        {350.0f, 390.0f}, // 1: bottom corridor past dining chairs
        {325.0f, 275.0f}, // 2: middle of the room
        {235.0f, 245.0f}, // 3: tight bend below the bed
        {245.0f, 165.0f}, // 4: narrow gap between bed and armchair
        {365.0f, 185.0f}, // 5: past the armchair toward the rug
        {400.0f, 290.0f}, // 6: sweeping below the coffee table
        {525.0f, 325.0f}, // 7: open floor on the right
    };

    const float totalCm = anim::pathLength(waypoints, 1024);
    const float speedCmPerSec = totalCm / step.durationSeconds;
    const Polygon vacuumFootprint = roomcraft::makeCircleFootprint({0.0f, 0.0f}, 16.0f, 32);
    const Polygon vacuumInner = roomcraft::makeCircleFootprint({0.0f, 0.0f}, 11.0f, 24);

    // Heading arrow in the vacuum's local coordinates (0 deg = +x right).
    Polygon noseArrow;
    noseArrow.vertices = {{15.0f, 0.0f}, {-4.0f, -8.0f}, {0.0f, 0.0f}, {-4.0f, 8.0f}};

    bool ok = true;
    for (int frame = 0; frame < step.frameCount(); ++frame) {
        const float s = step.progress(frame);
        drawFloorAndGrid(canvas);

        for (const Furniture& p : pieces) {
            drawDetailedFurniture(canvas, p);
        }

        // Waypoint crosses.
        for (const Point& wp : waypoints) {
            drawCross(canvas, wp, 3, kGuide);
        }

        // Dashed trail from the start of the path up to the vacuum's current
        // constant-speed distance fraction s. Every dash has the same length in
        // centimetres so the trail doesn't shimmer as the vacuum advances.
        const int trailSteps = std::max(1, static_cast<int>(std::lround(s * 90.0f)));
        for (int i = 0; i < trailSteps; ++i) {
            if (i % 2 != 0) {
                continue; // Every second chord is a gap -> clean dashed trail.
            }
            const float s0 = static_cast<float>(i) / 90.0f;
            const float s1 = std::min(s, static_cast<float>(i + 1) / 90.0f);
            if (s1 <= s0) {
                continue;
            }
            const Point p0 = anim::poseOnPathConstantSpeed(waypoints, s0, 512).position;
            const Point p1 = anim::poseOnPathConstantSpeed(waypoints, s1, 512).position;
            raster::drawThickLine(canvas, px(p0.x), px(p0.y), px(p1.x), px(p1.y), 2, kTrail);
        }

        // Robot vacuum placed and oriented by poseOnPathConstantSpeed.
        const Pose vacPose = anim::poseOnPathConstantSpeed(waypoints, s, 512);
        const Mat3 vacMat = xform::poseMatrix(vacPose);

        Furniture vacuum{"Robot vacuum", vacuumFootprint, kVacuumBody, vacPose};
        roomcraft::drawFurniture(canvas, vacuum);

        const Polygon topCap = xform::transformPolygon(vacMat, vacuumInner);
        raster::fillPolygonScanline(canvas, topCap, kVacuumTop);

        const Polygon arrowWorld = xform::transformPolygon(vacMat, noseArrow);
        raster::fillPolygonScanline(canvas, arrowWorld, kVacuumLed);
        raster::drawPolygonOutline(canvas, arrowWorld, kOutline);

        drawWallsAndDoor(canvas, true);

        char footer[128];
        std::snprintf(footer, sizeof(footer),
                      "Catmull-Rom path: %.0f cm total, constant speed %.0f cm/s, heading = %+.0f deg",
                      totalCm, speedCmPerSec, vacPose.angleDegrees);
        drawHeaderAndFooter(canvas, "Constant-speed Catmull-Rom path", frame, step.frameCount(), footer);

        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;
    reportGif(ok, writer);
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : std::string(ROOMCRAFT_DEMO_OUTPUT_DIR) + "/cp2";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    std::printf("RoomCraft Checkpoint 2 animation demo - writing GIFs to %s\n", dir.c_str());

    std::printf("1. Easing curves comparison (anim_easing.gif)\n");
    demoEasing(dir);
    std::printf("2. Keyframed room arrangement (anim_arrange.gif)\n");
    demoArrange(dir);
    std::printf("3. Wall collision check (anim_collision.gif)\n");
    demoCollision(dir);
    std::printf("4. Constant-speed robot vacuum path (anim_vacuum.gif)\n");
    demoVacuum(dir);

    if (failures > 0) {
        std::printf("%d animation(s) could not be written\n", failures);
        return 1;
    }
    std::printf("Done.\n");
    return 0;
}

