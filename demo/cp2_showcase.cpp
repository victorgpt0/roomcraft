// RoomCraft Checkpoint 2, Part 4b: the final integration showcase.
//
// Writes one continuous GIF and a shorter GIF for each scene to
// demo_output/cp2/ (or to the directory given as the first argument):
//
//   showcase.gif             the whole 22-second story, all six scenes
//   showcase_1_door.gif      pivot rotation and hierarchy (xform, scene, view)
//   showcase_2_arrange.gif   keyframes and easing (anim, roomcraft, view)
//   showcase_3_mirror.gif    wall check and reflection (roomcraft, xform)
//   showcase_4_fan_vacuum.gif hierarchy and path animation (scene, anim)
//   showcase_5_zoom.gif      window-to-viewport zoom (view)
//   showcase_6_reveal.gif    extrusion, projection and orbit (scene, view)
//
// The plan is drawn at 1 pixel = 1 cm in ROOM coordinates, then the 2D camera
// maps it into the viewport. Scene 6 lifts the same footprints into 3D with
// scene::extrude and draws them with view::drawWireMesh. Everything is still
// pure C++: the CP1 rasterizer fills the pixels and output::AnimationWriter
// compresses the frames.

#include "anim/easing.h"
#include "anim/path.h"
#include "anim/track.h"
#include "output/animation_writer.h"
#include "output/text.h"
#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "scene/extrude.h"
#include "scene/node.h"
#include "view/camera2d.h"
#include "view/camera3d.h"
#include "view/mat4.h"
#include "xform/mat3.h"
#include "xform/pose.h"
#include "xform/vec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
using raster::Rect;
using roomcraft::Furniture;
using scene::Node;
using view::Camera2D;
using view::Mat4;
using view::OrbitCamera;
using xform::Mat3;
using xform::Pose;
using xform::Vec3;
using xform::WireMesh;

namespace {

// ---- Layout and palette ----------------------------------------------------

const Color kBackdrop{32, 37, 48};
const Color kPaper{248, 247, 243};
const Color kCaption{245, 245, 248};
const Color kMuted{178, 186, 200};
const Color kBarBack{62, 70, 86};
const Color kBarFront{245, 166, 66};
const Color kWhite{255, 255, 255};
const Color kOutline{40, 40, 48};
const Color kWall{60, 60, 70};
const Color kFloor{238, 226, 205};
const Color kGrid{214, 198, 172};
const Color kWindow{90, 160, 220};
const Color kDoorArc{168, 164, 154};
const Color kGuide{188, 178, 158};
const Color kWood{181, 136, 99};
const Color kChair{96, 125, 160};
const Color kBed{170, 190, 215};
const Color kSofa{120, 150, 110};
const Color kRug{190, 80, 90};
const Color kGlassTable{150, 110, 80};
const Color kPlant{70, 140, 70};
const Color kPot{170, 100, 60};
const Color kWarning{220, 30, 30};
const Color kWarningFill{235, 95, 95};
const Color kVacuumBody{55, 65, 75};
const Color kVacuumTop{90, 105, 120};
const Color kVacuumLed{70, 210, 120};
const Color kTrail{70, 130, 190};
const Color kBlade{171, 128, 92};
const Color kBladeAlt{154, 112, 78};
const Color kHub{70, 78, 90};
const Color kCap{225, 205, 155};
const Color kBrass{224, 186, 92};

constexpr int kWidth = 400;
constexpr int kHeight = 300;
constexpr int kFps = 12;
constexpr int kSceneCount = 6;
constexpr float kSceneSeconds[kSceneCount] = {3.0f, 4.0f, 3.8f, 4.0f, 3.2f, 4.0f};
constexpr float totalSeconds() {
    float total = 0.0f;
    for (float seconds : kSceneSeconds) {
        total += seconds;
    }
    return total;
}
// Derived from the scene lengths so the story and the clips cannot disagree.
constexpr float kTotalSeconds = totalSeconds();
constexpr Rect kViewport{8.0f, 32.0f, 392.0f, 284.0f};
// "Under 2 MB" read strictly: 2,000,000 bytes, not 2 MiB.
constexpr std::uintmax_t kMaxFileBytes = 2000000U;

// The same 5.6 m x 4.0 m room used by every CP1 and CP2 demo.
constexpr float kRoomLeft = 40.0f;
constexpr float kRoomTop = 40.0f;
constexpr float kRoomRight = 600.0f;
constexpr float kRoomBottom = 440.0f;
constexpr float kWallHalf = 4.0f;
constexpr float kDoorLeft = 440.0f;
constexpr float kDoorRight = 520.0f;
constexpr float kWindowLeft = 250.0f;
constexpr float kWindowRight = 370.0f;
constexpr Point kArmchairCentre{330.0f, 150.0f};

// Scene 3 ends with the L-sofa reflected across the diagonal y = x, which
// carries its top-right corner shape into the bottom-left corner. In pose form
// that reflection is scaleX = -1 followed by a -90 degree turn:
//   (x, y) -> (-x, y) -> (y, x).
// checkDiagonalMirror() proves this against the textbook composite.
const Pose kSofaArrived{{480.0f, 140.0f}, 0.0f, 1.0f, 1.0f};
const Pose kSofaMirrored{{140.0f, 320.0f}, -90.0f, -1.0f, 1.0f};

int failures = 0;

int px(float value) {
    return raster::roundToPixel(value);
}

float clamp01(float value) {
    return std::min(std::max(value, 0.0f), 1.0f);
}

float smooth(float value) {
    return anim::ease(Easing::EaseInOut, clamp01(value));
}

Color lighter(Color color, float amount) {
    auto mix = [amount](uint8_t value) {
        return static_cast<uint8_t>(value + (255 - value) * amount + 0.5f);
    };
    return Color{mix(color.r), mix(color.g), mix(color.b)};
}

Color darker(Color color, float amount) {
    auto mix = [amount](uint8_t value) {
        return static_cast<uint8_t>(value * (1.0f - amount) + 0.5f);
    };
    return Color{mix(color.r), mix(color.g), mix(color.b)};
}

void fillRect(Canvas& canvas, const Rect& rect, Color color) {
    for (int y = static_cast<int>(rect.ymin); y < static_cast<int>(rect.ymax); ++y) {
        for (int x = static_cast<int>(rect.xmin); x < static_cast<int>(rect.xmax); ++x) {
            canvas.setPixel(x, y, color);
        }
    }
}

// ---- Room and furniture data ----------------------------------------------

Polygon roomOutline() {
    return raster::makeRectangle(kRoomLeft, kRoomTop, kRoomRight - kRoomLeft, kRoomBottom - kRoomTop);
}

Polygon centredRect(float width, float height) {
    return raster::makeRectangle(-width / 2.0f, -height / 2.0f, width, height);
}

// An L-shaped sofa has a notch, so a horizontal reflection is easy to see.
Polygon localLSofa() {
    Polygon sofa;
    sofa.vertices = {{-100.0f, -80.0f}, {100.0f, -80.0f}, {100.0f, 80.0f},
                     {50.0f, 80.0f},   {50.0f, -30.0f}, {-100.0f, -30.0f}};
    return sofa;
}

std::vector<Polygon> wallShapes() {
    const float l = kRoomLeft - kWallHalf;
    const float t = kRoomTop - kWallHalf;
    const float b = kRoomBottom + kWallHalf;
    const float w = 2.0f * kWallHalf;
    return {
        raster::makeRectangle(l, t, kWindowLeft - l, w),
        raster::makeRectangle(kWindowRight, t, kRoomRight + kWallHalf - kWindowRight, w),
        raster::makeRectangle(l, kRoomBottom - kWallHalf, kDoorLeft - l, w),
        raster::makeRectangle(kDoorRight, kRoomBottom - kWallHalf, kRoomRight + kWallHalf - kDoorRight, w),
        raster::makeRectangle(l, t, w, b - t),
        raster::makeRectangle(kRoomRight - kWallHalf, t, w, b - t),
    };
}

// One entry per piece: the local shape never changes; the pose places it.
struct FurnitureSpec {
    std::string name;
    Polygon local;
    Color color;
    Pose target;
    float height; // height in the 3D preview, in centimetres
};

// The bottom-left corner is left empty on purpose: scene 3 mirrors the sofa
// into it, and every later scene keeps it there.
const std::vector<FurnitureSpec>& furnitureSpecs() {
    static const std::vector<FurnitureSpec> specs = {
        {"Rug", centredRect(112.0f, 80.0f), kRug, {{470.0f, 182.0f}, 0.0f, 1.0f, 1.0f}, 2.0f},
        // Head against the left wall: the local 140 x 200 bed turned 90 degrees.
        {"Bed", centredRect(140.0f, 200.0f), kBed, {{160.0f, 130.0f}, 90.0f, 1.0f, 1.0f}, 55.0f},
        {"Nightstand", centredRect(36.0f, 36.0f), kWood, {{282.0f, 78.0f}, 0.0f, 1.0f, 1.0f}, 55.0f},
        {"Wardrobe", centredRect(60.0f, 140.0f), kWood, {{562.0f, 300.0f}, 0.0f, 1.0f, 1.0f}, 200.0f},
        {"L-sofa", localLSofa(), kSofa, kSofaArrived, 80.0f},
        {"Armchair", centredRect(70.0f, 70.0f), kSofa, {{kArmchairCentre.x, kArmchairCentre.y}, 25.0f, 1.0f, 1.0f}, 85.0f},
        {"Dining table", roomcraft::makeCircleFootprint({0.0f, 0.0f}, 45.0f, 48), kWood,
         {{330.0f, 360.0f}, 0.0f, 1.0f, 1.0f}, 75.0f},
        {"Plant", roomcraft::makeCircleFootprint({0.0f, 0.0f}, 14.0f, 24), kPot,
         {{574.0f, 412.0f}, 0.0f, 1.0f, 1.0f}, 120.0f},
    };
    return specs;
}

bool isSofa(const FurnitureSpec& spec) {
    return spec.name == "L-sofa";
}

Furniture pieceFrom(const FurnitureSpec& spec, const Pose& pose) {
    return Furniture{spec.name, spec.local, spec.color, pose};
}

// Where each piece stands after scene 3: the arrival targets, except that the
// sofa now sits mirrored in the bottom-left corner. Scenes 4, 5 and 6 all use
// this one layout, so the story stays continuous.
Pose finalPose(const FurnitureSpec& spec) {
    return isSofa(spec) ? kSofaMirrored : spec.target;
}

std::vector<Furniture> finalLayout() {
    std::vector<Furniture> pieces;
    for (const FurnitureSpec& spec : furnitureSpecs()) {
        pieces.push_back(pieceFrom(spec, finalPose(spec)));
    }
    return pieces;
}

// ---- 2D rendering helpers -------------------------------------------------

Camera2D planCamera() {
    const float margin = 16.0f;
    Camera2D camera{Rect{kRoomLeft - margin, kRoomTop - margin, kRoomRight + margin, kRoomBottom + margin},
                    kViewport};
    // A camera window with the viewport's aspect keeps squares square and
    // circles round; matchViewportAspect grows one side rather than squash.
    view::matchViewportAspect(camera);
    return camera;
}

void drawGridThroughCamera(Canvas& canvas, const Camera2D& camera) {
    for (float x = kRoomLeft + 50.0f; x < kRoomRight; x += 50.0f) {
        view::drawLineThroughCamera(canvas, {x, kRoomTop}, {x, kRoomBottom}, camera, kGrid);
    }
    for (float y = kRoomTop + 50.0f; y < kRoomBottom; y += 50.0f) {
        view::drawLineThroughCamera(canvas, {kRoomLeft, y}, {kRoomRight, y}, camera, kGrid);
    }
}

void drawWallsThroughCamera(Canvas& canvas, const Camera2D& camera) {
    for (const Polygon& wall : wallShapes()) {
        view::fillPolygonThroughCamera(canvas, wall, camera, kWall);
    }
    for (float offset : {-3.0f, 0.0f, 3.0f}) {
        view::drawLineThroughCamera(canvas, {kWindowLeft, kRoomTop + offset},
                                    {kWindowRight, kRoomTop + offset}, camera, kWindow);
    }
}

void drawRoomThroughCamera(Canvas& canvas, const Camera2D& camera) {
    view::fillPolygonThroughCamera(canvas, roomOutline(), camera, kFloor);
    drawGridThroughCamera(canvas, camera);
    drawWallsThroughCamera(canvas, camera);
}

void drawPieceThroughCamera(Canvas& canvas, const Furniture& piece, const Camera2D& camera) {
    view::drawFurnitureThroughCamera(canvas, {piece}, camera);
}

void drawPiecesThroughCamera(Canvas& canvas, const std::vector<Furniture>& pieces,
                             const Camera2D& camera) {
    view::drawFurnitureThroughCamera(canvas, pieces, camera);
}

// ---- Scene 1: door on a hinge ---------------------------------------------

void addWallNodes(Node& room) {
    const std::vector<Polygon> walls = wallShapes();
    for (std::size_t i = 0; i < walls.size(); ++i) {
        Node wall;
        wall.name = "wall " + std::to_string(i + 1);
        wall.shape = walls[i];
        wall.color = kWall;
        room.children.push_back(wall);
    }
}

void renderDoorScene(Canvas& canvas, float t) {
    const Camera2D camera = planCamera();

    // The room appears by growing about its centre. This is the same pivot
    // composite used for transformations: T(centre) * S * T(-centre).
    const float appear = 0.15f + 0.85f * smooth(t / 0.9f);
    const Point centre{(kRoomLeft + kRoomRight) / 2.0f, (kRoomTop + kRoomBottom) / 2.0f};
    const Mat3 grow = Mat3::translation(centre.x, centre.y) * Mat3::scaling(appear, appear) *
                      Mat3::translation(-centre.x, -centre.y);

    const Polygon floorWorld = xform::transformPolygon(grow, roomOutline());
    view::fillPolygonThroughCamera(canvas, floorWorld, camera, kFloor);

    for (float x = kRoomLeft + 50.0f; x < kRoomRight; x += 50.0f) {
        const Point a = xform::transformPoint(grow, {x, kRoomTop});
        const Point b = xform::transformPoint(grow, {x, kRoomBottom});
        view::drawLineThroughCamera(canvas, a, b, camera, kGrid);
    }
    for (float y = kRoomTop + 50.0f; y < kRoomBottom; y += 50.0f) {
        const Point a = xform::transformPoint(grow, {kRoomLeft, y});
        const Point b = xform::transformPoint(grow, {kRoomRight, y});
        view::drawLineThroughCamera(canvas, a, b, camera, kGrid);
    }

    // The door opens from 0 to about -82 degrees. Its local origin is the
    // hinge, so the pose rotation is exactly a rotation about the hinge.
    const float open = smooth((t - 0.8f) / 1.8f);
    const float doorAngle = -82.0f * open;

    // The swing arc is drawn under the door leaf.
    const float radius = kDoorRight - kDoorLeft;
    Point previous{kDoorLeft + radius, kRoomBottom};
    for (int i = 1; i <= 24; ++i) {
        const float degrees = 360.0f - 90.0f * static_cast<float>(i) / 24.0f;
        const double radians = degrees * raster::kPi / 180.0;
        const Point next{kDoorLeft + radius * static_cast<float>(std::cos(radians)),
                         kRoomBottom + radius * static_cast<float>(std::sin(radians))};
        view::drawLineThroughCamera(canvas, xform::transformPoint(grow, previous),
                                    xform::transformPoint(grow, next), camera, kDoorArc);
        previous = next;
    }

    Node room;
    room.name = "room";
    addWallNodes(room);

    Node door;
    door.name = "door";
    door.shape = raster::makeRectangle(0.0f, -4.0f, radius, 8.0f);
    door.color = kWood;
    door.pose.position = {kDoorLeft, kRoomBottom};
    door.pose.angleDegrees = doorAngle;

    Node knob;
    knob.name = "knob";
    knob.shape = roomcraft::makeCircleFootprint({0.0f, 0.0f}, 4.0f, 16);
    knob.color = kBrass;
    knob.pose.position = {radius - 12.0f, 0.0f};
    door.children.push_back(knob);

    Node hinge;
    hinge.name = "hinge";
    hinge.shape = roomcraft::makeCircleFootprint({0.0f, 0.0f}, 3.0f, 12);
    hinge.color = kOutline;
    door.children.push_back(hinge);

    room.children.push_back(door);

    // The whole tree is pushed through the camera matrix and the room's
    // appear transform. Every child inherits both for free.
    scene::drawNode(canvas, room, view::windowToViewport(camera) * grow);
}

// ---- Scene 2: keyframed arrangement ---------------------------------------

struct ScheduledPiece {
    const FurnitureSpec* spec = nullptr;
    PoseTrack track;
    float start = 0.0f;
    float finish = 0.0f;
};

std::vector<ScheduledPiece> arrangementSchedule() {
    std::vector<ScheduledPiece> schedule;
    const float doorwayX = (kDoorLeft + kDoorRight) / 2.0f;
    const Point doorway{doorwayX, kRoomBottom - 8.0f};
    const float stagger = 0.32f;
    const float move = 0.78f;

    for (std::size_t i = 0; i < furnitureSpecs().size(); ++i) {
        ScheduledPiece item;
        item.spec = &furnitureSpecs()[i];
        item.start = 0.18f + stagger * static_cast<float>(i);
        item.finish = item.start + move;

        const Pose entry{doorway, 130.0f, 0.2f, 0.2f};
        const Point target = item.spec->target.position;
        const Point mid{anim::lerp(doorway.x, target.x, 0.55f),
                        anim::lerp(doorway.y, target.y, 0.55f) - 24.0f};
        const Pose middle{mid, anim::lerpAngle(130.0f, item.spec->target.angleDegrees, 0.65f),
                          0.78f, 0.78f};

        item.track.addKey(item.start, entry, Easing::Linear);
        item.track.addKey(item.start + move * 0.55f, middle, Easing::EaseIn);
        item.track.addKey(item.finish, item.spec->target, Easing::EaseOut);
        schedule.push_back(item);
    }
    return schedule;
}

void renderArrangeScene(Canvas& canvas, float t) {
    const Camera2D camera = planCamera();
    drawRoomThroughCamera(canvas, camera);

    static const std::vector<ScheduledPiece> schedule = arrangementSchedule();
    for (const ScheduledPiece& item : schedule) {
        if (t < item.start) {
            continue;
        }
        const Pose pose = item.track.sample(t);
        const Furniture piece = pieceFrom(*item.spec, pose);
        if (t < item.finish) {
            // The faint outline is the destination keyframe.
            const Furniture ghost = pieceFrom(*item.spec, item.spec->target);
            view::drawPolygonOutlineThroughCamera(canvas, roomcraft::worldFootprint(ghost), camera,
                                                  kGuide);
        }
        drawPieceThroughCamera(canvas, piece, camera);
    }
}

// ---- Scene 3: collision and mirror ----------------------------------------

// The pose form of the diagonal mirror must equal the classic "reflection
// about a line through the origin" composite R(45) * Rx * R(-45): turn the
// line y = x onto the x axis, reflect across it, turn back. Both must also
// have determinant -1 (a reflection flips orientation; a rotation never does).
bool checkDiagonalMirror() {
    const Mat3 composite = Mat3::rotation(45.0f) * Mat3::reflectionAcrossXAxis() *
                           Mat3::rotation(-45.0f);
    Pose atOrigin = kSofaMirrored;
    atOrigin.position = {0.0f, 0.0f};
    const Mat3 fromPose = xform::poseMatrix(atOrigin);

    bool same = true;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            same = same && std::fabs(composite.m[row][column] - fromPose.m[row][column]) < 1e-5f;
        }
    }
    const bool flips = std::fabs(xform::determinant(fromPose) + 1.0f) < 1e-5f;
    std::printf("  diagonal mirror: pose matrix %s R(45)*Rx*R(-45), det = %.3f\n",
                same ? "==" : "!=", static_cast<double>(xform::determinant(fromPose)));
    return same && flips;
}

void renderMirrorScene(Canvas& canvas, float t) {
    const Camera2D camera = planCamera();
    drawRoomThroughCamera(canvas, camera);

    // The scene starts exactly where scene 2 ended: every other piece stays
    // put, and the bottom-left corner is the free space the sofa will take.
    std::vector<Furniture> context;
    for (const FurnitureSpec& spec : furnitureSpecs()) {
        if (!isSofa(spec)) {
            context.push_back(pieceFrom(spec, spec.target));
        }
    }
    drawPiecesThroughCamera(canvas, context, camera);

    // Push through the right wall, pull back, then one reflection plus a move
    // carries the sofa into the opposite corner.
    constexpr float kPullBack = 1.95f;
    constexpr float kLanded = 3.3f;
    const Pose pushed{{568.0f, 140.0f}, 0.0f, 1.0f, 1.0f};
    PoseTrack track;
    track.addKey(0.0f, kSofaArrived, Easing::Linear);
    track.addKey(0.35f, kSofaArrived, Easing::Linear);
    track.addKey(1.05f, pushed, Easing::EaseInOut);
    track.addKey(1.45f, pushed, Easing::Linear);
    track.addKey(kPullBack, kSofaArrived, Easing::EaseInOut);
    track.addKey(kLanded, kSofaMirrored, Easing::EaseInOut);

    const FurnitureSpec& sofaSpec = *std::find_if(furnitureSpecs().begin(), furnitureSpecs().end(), isSofa);
    if (t > kPullBack && t < kLanded) {
        // Destination outline, as in scene 2.
        view::drawPolygonOutlineThroughCamera(
            canvas, roomcraft::worldFootprint(pieceFrom(sofaSpec, kSofaMirrored)), camera, kGuide);
    }

    Furniture sofa = pieceFrom(sofaSpec, track.sample(t));
    const bool fits = roomcraft::fitsInRoom(sofa, roomOutline());
    sofa.color = fits ? kSofa : kWarningFill;
    drawPieceThroughCamera(canvas, sofa, camera);
    if (!fits) {
        view::drawPolygonOutlineThroughCamera(canvas, roomcraft::worldFootprint(sofa), camera, kWarning);
    }
}

// ---- Scene 4: fan and vacuum ----------------------------------------------

void renderFanVacuumScene(Canvas& canvas, float t) {
    const Camera2D camera = planCamera();
    drawRoomThroughCamera(canvas, camera);

    drawPiecesThroughCamera(canvas, finalLayout(), camera);

    // From the doorway, round the dining table, into the crook of the mirrored
    // sofa, along the foot of the bed and back across the rug. Every waypoint
    // keeps the 16 cm vacuum clear of the furniture.
    const std::vector<Point> path = {
        {480.0f, 410.0f}, {450.0f, 305.0f}, {330.0f, 268.0f}, {160.0f, 320.0f},
        {160.0f, 250.0f}, {260.0f, 235.0f}, {400.0f, 250.0f}, {500.0f, 300.0f},
    };
    const float progress = clamp01(t / kSceneSeconds[3]);

    // Dashed trail: draw every second short chord so the dash length is in
    // centimetres and does not shimmer as the vacuum advances.
    constexpr int kTrailChords = 90;
    const int reach = std::max(1, static_cast<int>(std::lround(progress * kTrailChords)));
    for (int i = 0; i < reach; i += 2) {
        const float s0 = static_cast<float>(i) / kTrailChords;
        const float s1 = std::min(progress, static_cast<float>(i + 1) / kTrailChords);
        if (s1 <= s0) {
            continue;
        }
        const Point a = anim::poseOnPathConstantSpeed(path, s0, 512).position;
        const Point b = anim::poseOnPathConstantSpeed(path, s1, 512).position;
        view::drawLineThroughCamera(canvas, a, b, camera, kTrail);
    }

    const Pose vacuumPose = anim::poseOnPathConstantSpeed(path, progress, 512);
    Furniture vacuum{"Robot vacuum", roomcraft::makeCircleFootprint({0.0f, 0.0f}, 16.0f, 32),
                     kVacuumBody, vacuumPose};
    drawPieceThroughCamera(canvas, vacuum, camera);

    const Mat3 vacuumWorld = xform::poseMatrix(vacuumPose);
    const Polygon vacuumCap = xform::transformPolygon(
        vacuumWorld, roomcraft::makeCircleFootprint({0.0f, 0.0f}, 11.0f, 24));
    view::fillPolygonThroughCamera(canvas, vacuumCap, camera, kVacuumTop);

    Polygon arrow;
    arrow.vertices = {{15.0f, 0.0f}, {-4.0f, -8.0f}, {0.0f, 0.0f}, {-4.0f, 8.0f}};
    view::fillPolygonThroughCamera(canvas, xform::transformPolygon(vacuumWorld, arrow), camera,
                                   kVacuumLed);

    // The ceiling fan is a scene graph: fan -> hub -> four blades + cap.
    // Spinning the hub moves all blades; swaying the fan moves the whole tree.
    Node fan;
    fan.name = "fan";
    const float phase = 2.0f * static_cast<float>(raster::kPi) * t / kSceneSeconds[3];
    fan.pose.position = {320.0f + 16.0f * static_cast<float>(std::sin(phase)),
                         215.0f + 9.0f * static_cast<float>(std::sin(2.0f * phase))};
    fan.pose.angleDegrees = 4.0f * static_cast<float>(std::sin(phase));

    Node hub;
    hub.name = "hub";
    hub.shape = roomcraft::makeCircleFootprint({0.0f, 0.0f}, 18.0f, 24);
    hub.color = kHub;
    hub.pose.angleDegrees = 720.0f * progress;

    for (int i = 0; i < 4; ++i) {
        Node blade;
        blade.name = "blade " + std::to_string(i + 1);
        blade.shape.vertices = {{20.0f, -9.0f}, {78.0f, -5.0f}, {88.0f, 0.0f},
                                {78.0f, 5.0f},  {20.0f, 9.0f},  {12.0f, 0.0f}};
        blade.color = (i % 2 == 0) ? kBlade : kBladeAlt;
        blade.pose.angleDegrees = 90.0f * static_cast<float>(i);
        hub.children.push_back(blade);
    }

    Node cap;
    cap.name = "cap";
    cap.shape = roomcraft::makeCircleFootprint({0.0f, 0.0f}, 7.0f, 16);
    cap.color = kCap;
    hub.children.push_back(cap);
    fan.children.push_back(hub);

    scene::drawNode(canvas, fan, view::windowToViewport(camera));
}

// ---- Scene 5: 2D camera zoom ----------------------------------------------

void renderZoomScene(Canvas& canvas, float t) {
    Camera2D camera = planCamera();

    // Zoom in, hold, zoom back out. zoomAbout keeps the armchair under the
    // same pixel, which is the key property of a camera zoom.
    const float local = clamp01(t / kSceneSeconds[4]);
    float amount = 0.0f;
    if (local < 0.45f) {
        amount = smooth(local / 0.45f);
    } else if (local < 0.60f) {
        amount = 1.0f;
    } else {
        amount = 1.0f - smooth((local - 0.60f) / 0.40f);
    }
    const float factor = 1.0f + 2.3f * amount;
    view::zoomAbout(camera, factor, kArmchairCentre);

    drawRoomThroughCamera(canvas, camera);
    drawPiecesThroughCamera(canvas, finalLayout(), camera);

    const Point anchor = view::roomToScreen(camera, kArmchairCentre);
    raster::drawLineBresenham(canvas, px(anchor.x) - 5, px(anchor.y), px(anchor.x) + 5,
                              px(anchor.y), kBarFront);
    raster::drawLineBresenham(canvas, px(anchor.x), px(anchor.y) - 5, px(anchor.x),
                              px(anchor.y) + 5, kBarFront);
}

// ---- Scene 6: 2D to 3D reveal ---------------------------------------------

struct Solid3D {
    WireMesh mesh;
    Color color;
};

WireMesh floorGridMesh() {
    WireMesh grid;
    for (float x = kRoomLeft + 50.0f; x < kRoomRight; x += 50.0f) {
        const int i = static_cast<int>(grid.vertices.size());
        grid.vertices.push_back({x, 0.0f, kRoomTop});
        grid.vertices.push_back({x, 0.0f, kRoomBottom});
        grid.edges.push_back({i, i + 1});
    }
    for (float y = kRoomTop + 50.0f; y < kRoomBottom; y += 50.0f) {
        const int i = static_cast<int>(grid.vertices.size());
        grid.vertices.push_back({kRoomLeft, 0.0f, y});
        grid.vertices.push_back({kRoomRight, 0.0f, y});
        grid.edges.push_back({i, i + 1});
    }
    return grid;
}

const std::vector<Solid3D>& solids3D() {
    static const std::vector<Solid3D> solids = [] {
        std::vector<Solid3D> built;
        built.push_back({scene::extrudedRoom(roomOutline(), 240.0f), kWall});
        for (const FurnitureSpec& spec : furnitureSpecs()) {
            const Furniture piece = pieceFrom(spec, finalPose(spec));
            built.push_back(
                {scene::extrude(roomcraft::worldFootprint(piece), spec.height),
                 darker(spec.color, 0.25f)});
        }
        return built;
    }();
    return solids;
}

void renderRevealScene(Canvas& canvas, float t) {
    const float local = clamp01(t / kSceneSeconds[5]);
    const float aspect = (kViewport.xmax - kViewport.xmin) / (kViewport.ymax - kViewport.ymin);
    const Mat4 projection = Mat4::perspective(45.0f, aspect, 10.0f, 5000.0f);

    // Hold the top view very briefly, then tilt from 90 to 28 degrees while
    // the azimuth makes exactly one turn round the room.
    const float orbit = smooth((local - 0.12f) / 0.88f);
    OrbitCamera camera;
    camera.target = Vec3{320.0f, 70.0f, 240.0f};
    camera.radius = 1000.0f;
    camera.azimuthDegrees = 360.0f * orbit;
    camera.elevationDegrees = 90.0f - 62.0f * orbit;
    const Mat4 viewProjection = projection * view::viewMatrix(camera);

    fillRect(canvas, kViewport, kWhite);

    // The filled floor plan stays under the wireframes, so the top view really
    // looks like the 2D plan and the 3D view keeps the colour coding.
    view::fillPlanPolygon(canvas, roomOutline(), 0.0f, viewProjection, kFloor, kViewport);
    for (const FurnitureSpec& spec : furnitureSpecs()) {
        const Furniture piece = pieceFrom(spec, finalPose(spec));
        view::fillPlanPolygon(canvas, roomcraft::worldFootprint(piece), 0.0f, viewProjection,
                              lighter(spec.color, 0.25f), kViewport);
    }
    view::drawWireMesh(canvas, floorGridMesh(), viewProjection, kGrid, kViewport);

    for (const Solid3D& solid : solids3D()) {
        view::drawWireMesh(canvas, solid.mesh, viewProjection, solid.color, kViewport);
    }
}

// ---- Captions, frame assembly and GIF output ------------------------------

struct Caption {
    const char* title;
    const char* line;
};

const Caption kCaptions[kSceneCount] = {
    {"1/6  PIVOT ROTATION + HIERARCHY",
     "Door is a child of the room; its local origin is the hinge."},
    {"2/6  KEYFRAMES + EASING",
     "PoseTrack samples pose keys; easing and lerpPose fill the gaps."},
    {"3/6  WALL CHECK + REFLECTION",
     "fitsInRoom turns it red; a diagonal mirror fits the far corner."},
    {"4/6  HIERARCHY + PATH ANIMATION",
     "Blades inherit the hub; vacuum follows a constant-speed path."},
    {"5/6  2D CAMERA: WINDOW TO VIEWPORT",
     "zoomAbout keeps the armchair fixed while the window shrinks."},
    {"6/6  2D TO 3D: EXTRUDE + ORBIT",
     "scene::extrude + view::drawWireMesh; perspective orbit into 3D."},
};

void drawChrome(Canvas& canvas, int scene, float sceneTime) {
    const float progress = clamp01(sceneTime / kSceneSeconds[scene]);
    output::drawText(canvas, 8, 4, kCaptions[scene].title, kCaption, 1);
    output::drawText(canvas, 8, 16, kCaptions[scene].line, kMuted, 1);
    output::drawText(canvas, 8, 288, "xform | output | scene | view | anim", kMuted, 1);

    const int barLeft = 8;
    const int barRight = kWidth - 8;
    const int barY = 29;
    raster::drawLineBresenham(canvas, barLeft, barY, barRight, barY, kBarBack);
    const int filled = barLeft + static_cast<int>(std::lround((barRight - barLeft) * progress));
    if (filled > barLeft) {
        raster::drawLineBresenham(canvas, barLeft, barY, filled, barY, kBarFront);
    }
}

void renderFrame(Canvas& canvas, int scene, float sceneTime) {
    canvas.clear(kBackdrop.r, kBackdrop.g, kBackdrop.b);
    fillRect(canvas, kViewport, kPaper);
    switch (scene) {
    case 0:
        renderDoorScene(canvas, sceneTime);
        break;
    case 1:
        renderArrangeScene(canvas, sceneTime);
        break;
    case 2:
        renderMirrorScene(canvas, sceneTime);
        break;
    case 3:
        renderFanVacuumScene(canvas, sceneTime);
        break;
    case 4:
        renderZoomScene(canvas, sceneTime);
        break;
    default:
        renderRevealScene(canvas, sceneTime);
        break;
    }
    raster::drawPolygonOutline(canvas, raster::makeRectangle(7.0f, 31.0f, 386.0f, 254.0f), kBarBack);
    drawChrome(canvas, scene, sceneTime);
}

void reportGif(bool ok, const output::AnimationWriter& writer) {
    if (!ok) {
        std::printf("  ERROR: could not write %s\n", writer.path().c_str());
        ++failures;
        return;
    }
    std::error_code error;
    const std::uintmax_t bytes = std::filesystem::file_size(writer.path(), error);
    std::printf("  wrote %-34s %3d frames, %6.1f KB\n", writer.path().c_str(),
                writer.frameCount(), error ? 0.0 : static_cast<double>(bytes) / 1024.0);
    if (!error && bytes >= kMaxFileBytes) {
        std::printf("    ERROR: %s is %ju bytes; keep committed files under 2 MB\n",
                    writer.path().c_str(), bytes);
        ++failures;
    }
}

bool writeGif(const std::string& directory, const std::string& name, int scene, float duration) {
    const FixedTimestep step(kFps, duration);
    output::AnimationWriter writer(directory, name, kWidth, kHeight, kFps);
    Canvas canvas(kWidth, kHeight);
    bool ok = true;
    for (int frame = 0; frame < step.frameCount(); ++frame) {
        renderFrame(canvas, scene, step.frameTime(frame));
        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;
    reportGif(ok, writer);
    return ok;
}

int sceneAt(float timeSeconds, float& sceneTime) {
    float start = 0.0f;
    for (int i = 0; i < kSceneCount; ++i) {
        const float end = start + kSceneSeconds[i];
        if (timeSeconds < end || i == kSceneCount - 1) {
            sceneTime = timeSeconds - start;
            return i;
        }
        start = end;
    }
    sceneTime = 0.0f;
    return kSceneCount - 1;
}

bool writeContinuousGif(const std::string& directory) {
    const FixedTimestep step(kFps, kTotalSeconds);
    output::AnimationWriter writer(directory, "showcase", kWidth, kHeight, kFps);
    Canvas canvas(kWidth, kHeight);
    bool ok = true;
    for (int frame = 0; frame < step.frameCount(); ++frame) {
        float sceneTime = 0.0f;
        const int scene = sceneAt(step.frameTime(frame), sceneTime);
        renderFrame(canvas, scene, sceneTime);
        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;
    reportGif(ok, writer);
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    const std::string directory =
        argc > 1 ? argv[1] : std::string(ROOMCRAFT_DEMO_OUTPUT_DIR) + "/cp2";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    std::printf("RoomCraft CP2 Part 4b: integration showcase -> %s\n", directory.c_str());

    if (!checkDiagonalMirror()) {
        std::printf("  ERROR: the scene 3 mirror pose is not a reflection across y = x\n");
        ++failures;
    }

    // Write the six short clips first, then the continuous story. The short
    // clips are the ones copied to docs/images/cp2/ for the README.
    const char* names[kSceneCount] = {
        "showcase_1_door", "showcase_2_arrange", "showcase_3_mirror",
        "showcase_4_fan_vacuum", "showcase_5_zoom", "showcase_6_reveal",
    };
    for (int i = 0; i < kSceneCount; ++i) {
        writeGif(directory, names[i], i, kSceneSeconds[i]);
    }
    writeContinuousGif(directory);

    if (failures > 0) {
        std::printf("%d GIF(s) failed or exceeded the 2 MB limit\n", failures);
        return 1;
    }
    std::printf("Done.\n");
    return 0;
}
