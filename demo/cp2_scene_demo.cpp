// RoomCraft Checkpoint 2, Part 2 demo: hierarchical models and 3D geometry.
//
// Writes four animated GIFs to demo_output/cp2/ (or to the directory given as
// the first command-line argument):
//
//   scene_fan.gif      a ceiling fan: a hub with four blade children spinning
//                      while the whole fan sways
//   scene_chair.gif    a swivel chair rolling across the room; the seat,
//                      backrest and armrests turn on the base
//   scene_door.gif     a door hinged on a room, swinging while the whole room
//                      moves, so every child follows the root
//   scene_room_3d.gif  the furnished room extruded into wireframe boxes with
//                      sensible heights and drawn with the oblique projection
//
// The first three use scene::drawNode on the 2D floor plan. The last uses
// scene::extrude and scene::obliqueProject, independently of the Part 3 camera.

#include "output/animation_writer.h"
#include "output/text.h"
#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/clipping.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "scene/extrude.h"
#include "scene/node.h"
#include "xform/mat3.h"
#include "xform/pose.h"
#include "xform/vec.h"

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

using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Polygon;
using scene::Node;
using xform::Mat3;
using xform::Vec3;

namespace {

// ---- Palette and room geometry (matching the Checkpoint 1 demo) ------------
const Color kWhite{255, 255, 255};
const Color kOutline{40, 40, 40};
const Color kWall{60, 60, 70};
const Color kFloor{238, 226, 205};
const Color kFloorEdge{205, 188, 160};
const Color kGrid{214, 198, 172};
const Color kWindow{90, 160, 220};
const Color kWood{181, 136, 99};
const Color kChair{96, 125, 160};
const Color kChairDark{66, 90, 120};
const Color kBed{170, 190, 215};
const Color kSofa{120, 150, 110};
const Color kRug{190, 80, 90};
const Color kGlassTable{150, 110, 80};
const Color kPlant{70, 140, 70};
const Color kPot{170, 100, 60};
const Color kBlade{171, 128, 92};
const Color kBladeAlt{154, 112, 78};
const Color kHub{70, 78, 90};
const Color kCap{225, 205, 155};
const Color kBase{70, 78, 90};
const Color kMarker{240, 170, 60};
const Color kBrass{224, 186, 92};
const Color kGuide{185, 175, 155};

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

Polygon circle(float radius, int sides = 32) {
    return roomcraft::makeCircleFootprint({0.0f, 0.0f}, radius, sides);
}

Polygon centredRect(float width, float height) {
    return raster::makeRectangle(-width / 2.0f, -height / 2.0f, width, height);
}

void reportGif(bool ok, const output::AnimationWriter& writer) {
    if (ok) {
        std::error_code error;
        const auto bytes = std::filesystem::file_size(writer.path(), error);
        std::printf("  wrote %-40s %3d frames, %.2f s/frame, %3d colours, %6.1f KB\n",
                    writer.path().c_str(), writer.frameCount(),
                    writer.delayCentiseconds() / 100.0, writer.colorCount(),
                    error ? 0.0 : static_cast<double>(bytes) / 1024.0);
    } else {
        std::printf("  ERROR: could not write %s\n", writer.path().c_str());
        ++failures;
    }
}

void drawFloorAndGrid(Canvas& canvas) {
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

void drawWallsAndWindow(Canvas& canvas) {
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
}

void drawHeaderAndFooter(Canvas& canvas, const std::string& title, int frame, int totalFrames,
                         const std::string& footer) {
    output::drawText(canvas, 30, 12, title, kOutline, 2);
    const std::string counter = "frame " + std::to_string(frame + 1) + "/" + std::to_string(totalFrames);
    output::drawText(canvas, canvas.width() - 30 - output::textWidth(counter, 1), 12, counter, kChair, 1);
    if (!footer.empty()) {
        output::drawText(canvas, 30, canvas.height() - 20, footer, kOutline, 1);
    }
}

float smoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ---- 1. Ceiling fan -------------------------------------------------------

void makeFanGif(const std::string& directory) {
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr int kFrames = 80;

    output::AnimationWriter gif(directory, "scene_fan", kWidth, kHeight, 20);
    bool ok = true;

    for (int frame = 0; frame < kFrames; ++frame) {
        const float t = static_cast<float>(frame) / static_cast<float>(kFrames);
        const double phase = 2.0 * raster::kPi * t;

        Canvas canvas(kWidth, kHeight);
        canvas.clear(kWhite.r, kWhite.g, kWhite.b);
        drawFloorAndGrid(canvas);
        drawWallsAndWindow(canvas);

        // Root group: no shape of its own, just the sway. Its position and
        // angle feed every descendant.
        Node fan;
        fan.name = "fan";
        fan.pose.position = {320.0f + 24.0f * static_cast<float>(std::sin(phase)),
                             235.0f + 14.0f * static_cast<float>(std::sin(2.0 * phase))};
        fan.pose.angleDegrees = 5.0f * static_cast<float>(std::sin(phase));

        // The hub spins. The blades are its children, so they inherit the
        // spin without being animated themselves.
        Node hub;
        hub.name = "hub";
        hub.shape = circle(26.0f);
        hub.color = kHub;
        hub.pose.angleDegrees = static_cast<float>(720.0 * t);

        for (int i = 0; i < 4; ++i) {
            Node blade;
            blade.name = "blade " + std::to_string(i + 1);
            blade.shape.vertices = {{28.0f, -13.0f}, {130.0f, -8.0f}, {144.0f, 0.0f},
                                    {130.0f, 8.0f},   {28.0f, 13.0f},  {18.0f, 0.0f}};
            blade.color = (i % 2 == 0) ? kBlade : kBladeAlt;
            blade.pose.angleDegrees = 90.0f * static_cast<float>(i);
            hub.children.push_back(blade);
        }

        Node cap;
        cap.name = "cap";
        cap.shape = circle(9.0f, 24);
        cap.color = kCap;
        hub.children.push_back(cap);

        fan.children.push_back(hub);
        scene::drawNode(canvas, fan, Mat3::identity());

        drawHeaderAndFooter(canvas, "Ceiling fan: fan -> hub -> 4 blades", frame, kFrames,
                            "The blades inherit the hub's spin; the hub inherits the fan's sway.");
        if (!gif.addFrame(canvas)) {
            ok = false;
        }
    }

    ok = gif.finish() && ok;
    reportGif(ok, gif);
}

// ---- 2. Swivel chair ------------------------------------------------------

Polygon makeStarBase() {
    // A five-legged office-chair base: alternating outer and inner radii.
    Polygon star;
    constexpr int kPoints = 5;
    for (int i = 0; i < kPoints * 2; ++i) {
        const double angle = -raster::kPi / 2.0 + i * raster::kPi / kPoints;
        const float radius = (i % 2 == 0) ? 40.0f : 14.0f;
        star.vertices.push_back(Point{radius * static_cast<float>(std::cos(angle)),
                                      radius * static_cast<float>(std::sin(angle))});
    }
    return star;
}

Point chairPosition(float t) {
    return Point{150.0f + 330.0f * t,
                 360.0f - 220.0f * t + 22.0f * static_cast<float>(std::sin(2.0 * raster::kPi * t))};
}

void makeChairGif(const std::string& directory) {
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr int kFrames = 100;

    output::AnimationWriter gif(directory, "scene_chair", kWidth, kHeight, 20);
    bool ok = true;

    for (int frame = 0; frame < kFrames; ++frame) {
        const float t = static_cast<float>(frame) / static_cast<float>(kFrames);
        const double phase = 2.0 * raster::kPi * t;

        Canvas canvas(kWidth, kHeight);
        canvas.clear(kWhite.r, kWhite.g, kWhite.b);
        drawFloorAndGrid(canvas);
        drawWallsAndWindow(canvas);

        // Dashed trail of where the root has rolled.
        Point previous = chairPosition(0.0f);
        for (int i = 1; i <= 24; ++i) {
            const Point next = chairPosition(t * static_cast<float>(i) / 24.0f);
            raster::drawDashedLine(canvas, px(previous.x), px(previous.y), px(next.x), px(next.y),
                                   5, 5, kGuide);
            previous = next;
        }

        Node chair;
        chair.name = "chair";
        chair.pose.position = chairPosition(t);
        chair.pose.angleDegrees = 8.0f * static_cast<float>(std::sin(phase));

        // The base belongs to the root, not to the seat, so it does NOT
        // swivel with the seat.
        Node base;
        base.name = "base";
        base.shape = makeStarBase();
        base.color = kBase;

        Node marker;
        marker.name = "base marker";
        marker.pose.position = {0.0f, 30.0f};
        marker.shape = centredRect(10.0f, 20.0f);
        marker.color = kMarker;
        base.children.push_back(marker);
        chair.children.push_back(base);

        // The seat turns on the base. Backrest and armrests are children of
        // the seat, so they follow the swivel too.
        Node seat;
        seat.name = "seat";
        seat.shape = centredRect(72.0f, 68.0f);
        seat.color = kChair;
        seat.pose.angleDegrees = 220.0f * static_cast<float>(std::sin(2.0 * raster::kPi * 1.3 * t));

        Node backrest;
        backrest.name = "backrest";
        backrest.pose.position = {0.0f, -34.0f};
        backrest.shape = centredRect(80.0f, 22.0f);
        backrest.color = kChairDark;
        seat.children.push_back(backrest);

        for (int side = -1; side <= 1; side += 2) {
            Node armrest;
            armrest.name = (side < 0) ? "left armrest" : "right armrest";
            armrest.pose.position = {42.0f * static_cast<float>(side), 0.0f};
            armrest.shape = centredRect(14.0f, 50.0f);
            armrest.color = kChairDark;
            seat.children.push_back(armrest);
        }

        chair.children.push_back(seat);
        scene::drawNode(canvas, chair, Mat3::identity());

        drawHeaderAndFooter(canvas, "Swivel chair: root rolls, seat turns", frame, kFrames,
                            "Backrest and armrests follow the seat; the star base stays on the root.");
        if (!gif.addFrame(canvas)) {
            ok = false;
        }
    }

    ok = gif.finish() && ok;
    reportGif(ok, gif);
}

// ---- 3. Door hinged on a moving room --------------------------------------

void addWall(Node& room, const std::string& name, float x, float y, float width, float height) {
    Node wall;
    wall.name = name;
    wall.shape = raster::makeRectangle(x, y, width, height);
    wall.color = kWall;
    room.children.push_back(wall);
}

void makeDoorGif(const std::string& directory) {
    constexpr int kWidth = 720;
    constexpr int kHeight = 560;
    constexpr int kFrames = 90;

    output::AnimationWriter gif(directory, "scene_door", kWidth, kHeight, 20);
    bool ok = true;

    for (int frame = 0; frame < kFrames; ++frame) {
        const float t = static_cast<float>(frame) / static_cast<float>(kFrames);
        const double phase = 2.0 * raster::kPi * t;

        Canvas canvas(kWidth, kHeight);
        canvas.clear(kWhite.r, kWhite.g, kWhite.b);

        // Room local coordinates are centred on (0, 0), so the room swings
        // about its own centre instead of about a corner.
        Node room;
        room.name = "room";
        room.shape = raster::makeRectangle(-280.0f, -200.0f, 560.0f, 400.0f);
        room.color = kFloor;
        room.pose.position = {360.0f + 18.0f * static_cast<float>(std::sin(phase)),
                              280.0f + 10.0f * static_cast<float>(std::sin(2.0 * phase))};
        room.pose.angleDegrees = 5.0f * static_cast<float>(std::sin(phase));

        // Walls are children too: they move with the room. The bottom wall
        // has a gap for the door (local x 120..200).
        addWall(room, "top wall left", -288.0f, -204.0f, 218.0f, 8.0f);
        addWall(room, "top wall right", 50.0f, -204.0f, 238.0f, 8.0f);
        addWall(room, "left wall", -288.0f, -204.0f, 8.0f, 408.0f);
        addWall(room, "right wall", 280.0f, -204.0f, 8.0f, 408.0f);
        addWall(room, "bottom wall left", -288.0f, 196.0f, 408.0f, 8.0f);
        addWall(room, "bottom wall right", 200.0f, 196.0f, 88.0f, 8.0f);

        Node rug;
        rug.name = "rug";
        rug.shape = circle(70.0f, 40);
        rug.color = kRug;
        rug.pose.position = {-100.0f, 40.0f};
        rug.pose.scaleX = 1.4f;
        rug.pose.scaleY = 0.8f;
        room.children.push_back(rug);

        Node cabinet;
        cabinet.name = "cabinet";
        cabinet.shape = centredRect(80.0f, 60.0f);
        cabinet.color = kWood;
        cabinet.pose.position = {-150.0f, -110.0f};
        room.children.push_back(cabinet);

        // The door is a child of the room: the hinge position is in ROOM
        // coordinates. The door leaf extends along local +x when closed.
        Node door;
        door.name = "door";
        door.shape = raster::makeRectangle(0.0f, -4.0f, 80.0f, 8.0f);
        door.color = kWood;
        door.pose.position = {120.0f, 200.0f};
        door.pose.angleDegrees = -85.0f * (0.5f - 0.5f * static_cast<float>(std::cos(phase)));

        Node knob;
        knob.name = "knob";
        knob.shape = circle(4.0f, 16);
        knob.color = kBrass;
        knob.pose.position = {68.0f, 0.0f};
        door.children.push_back(knob);

        Node hinge;
        hinge.name = "hinge";
        hinge.shape = circle(3.0f, 12);
        hinge.color = kOutline;
        door.children.push_back(hinge);

        room.children.push_back(door);
        scene::drawNode(canvas, room, Mat3::identity());

        drawHeaderAndFooter(canvas, "Door hinged on a moving room", frame, kFrames,
                            "Door, rug, cabinet and walls all inherit the room root's motion.");
        if (!gif.addFrame(canvas)) {
            ok = false;
        }
    }

    ok = gif.finish() && ok;
    reportGif(ok, gif);
}

// ---- 4. Extruded furnished room -------------------------------------------

struct BoxPiece {
    std::string name;
    Polygon footprint;
    Color color;
    float height;
};

Polygon ellipseFootprint(Point centre, float radiusX, float radiusY, int sides = 40) {
    Polygon polygon = roomcraft::makeCircleFootprint(centre, radiusX, sides);
    raster::scale(polygon, 1.0f, radiusY / radiusX, centre);
    return polygon;
}

std::vector<BoxPiece> furnishedRoom() {
    std::vector<BoxPiece> pieces;
    pieces.push_back({"Rug", ellipseFootprint({470.0f, 182.0f}, 56.0f, 40.0f), kRug, 2.0f});
    pieces.push_back({"Bed", raster::makeRectangle(60.0f, 60.0f, 140.0f, 200.0f), kBed, 55.0f});
    pieces.push_back({"Nightstand", raster::makeRectangle(208.0f, 60.0f, 36.0f, 36.0f), kWood, 55.0f});
    pieces.push_back({"Wardrobe", raster::makeRectangle(48.0f, 290.0f, 60.0f, 140.0f), kWood, 200.0f});

    Polygon sofa;
    sofa.vertices = {{380.0f, 60.0f}, {580.0f, 60.0f}, {580.0f, 220.0f},
                    {530.0f, 220.0f}, {530.0f, 110.0f}, {380.0f, 110.0f}};
    pieces.push_back({"L-shaped sofa", sofa, kSofa, 80.0f});

    Polygon armchair = raster::makeRectangle(275.0f, 75.0f, 70.0f, 70.0f);
    raster::rotate(armchair, 25.0f, raster::computeCentroid(armchair));
    pieces.push_back({"Armchair", armchair, kSofa, 85.0f});

    pieces.push_back({"Dining table", roomcraft::makeCircleFootprint({200.0f, 360.0f}, 45.0f, 48),
                      kWood, 75.0f});

    const Point chairCentres[4] = {{200.0f, 298.0f}, {262.0f, 360.0f}, {200.0f, 422.0f}, {138.0f, 360.0f}};
    for (int i = 0; i < 4; ++i) {
        pieces.push_back({"Chair " + std::to_string(i + 1),
                          roomcraft::makeCircleFootprint(chairCentres[i], 14.0f, 24), kChair, 90.0f});
    }

    pieces.push_back({"Coffee table", ellipseFootprint({470.0f, 182.0f}, 50.0f, 30.0f), kGlassTable, 45.0f});
    pieces.push_back({"Plant", roomcraft::makeCircleFootprint({574.0f, 412.0f}, 14.0f, 24), kPlant, 120.0f});
    return pieces;
}

void drawMesh(Canvas& canvas, const xform::WireMesh& mesh, Color color, Point offset) {
    for (const std::pair<int, int>& edge : mesh.edges) {
        const Vec3& a = mesh.vertices[static_cast<std::size_t>(edge.first)];
        const Vec3& b = mesh.vertices[static_cast<std::size_t>(edge.second)];
        const Point sa = scene::obliqueProject(a, offset);
        const Point sb = scene::obliqueProject(b, offset);
        raster::drawLineBresenham(canvas, px(sa.x), px(sa.y), px(sb.x), px(sb.y), color);
    }
}

void makeRoom3DGif(const std::string& directory) {
    constexpr int kWidth = 720;
    constexpr int kHeight = 540;
    constexpr int kFrames = 80;
    const Point offset{-40.0f, 380.0f};
    const std::vector<BoxPiece> pieces = furnishedRoom();

    output::AnimationWriter gif(directory, "scene_room_3d", kWidth, kHeight, 15);
    bool ok = true;

    for (int frame = 0; frame < kFrames; ++frame) {
        const float t = static_cast<float>(frame) / static_cast<float>(kFrames - 1);

        Canvas canvas(kWidth, kHeight);
        canvas.clear(kWhite.r, kWhite.g, kWhite.b);

        // Project the floor polygon itself, so the room reads as a solid
        // surface under the wireframes.
        const Polygon room = roomOutline();
        Polygon floor;
        floor.vertices.reserve(room.vertices.size());
        for (const Point& p : room.vertices) {
            floor.vertices.push_back(scene::obliqueProject(Vec3{p.x, 0.0f, p.y}, offset));
        }
        raster::fillPolygonScanline(canvas, floor, kFloor);
        raster::drawPolygonOutline(canvas, floor, kFloorEdge);

        // Faint floor grid in world X and Z directions.
        const int left = static_cast<int>(kRoomLeft);
        const int right = static_cast<int>(kRoomLeft + kRoomWidth);
        const int top = static_cast<int>(kRoomTop);
        const int bottom = static_cast<int>(kRoomTop + kRoomHeight);
        for (int x = left; x <= right; x += 50) {
            const Point a = scene::obliqueProject(Vec3{static_cast<float>(x), 0.0f, static_cast<float>(top)}, offset);
            const Point b = scene::obliqueProject(Vec3{static_cast<float>(x), 0.0f, static_cast<float>(bottom)}, offset);
            raster::drawLineBresenham(canvas, px(a.x), px(a.y), px(b.x), px(b.y), kGrid);
        }
        for (int z = top; z <= bottom; z += 50) {
            const Point a = scene::obliqueProject(Vec3{static_cast<float>(left), 0.0f, static_cast<float>(z)}, offset);
            const Point b = scene::obliqueProject(Vec3{static_cast<float>(right), 0.0f, static_cast<float>(z)}, offset);
            raster::drawLineBresenham(canvas, px(a.x), px(a.y), px(b.x), px(b.y), kGrid);
        }

        // Walls rise first, then each piece rises from its flat footprint.
        const float wallHeight = 250.0f * smoothStep(std::min(1.0f, t / 0.3f));
        drawMesh(canvas, scene::extrudedRoom(room, wallHeight), kWall, offset);

        for (std::size_t i = 0; i < pieces.size(); ++i) {
            const float start = 0.08f + 0.035f * static_cast<float>(i);
            const float progress = smoothStep((t - start) / 0.4f);
            const float height = pieces[i].height * progress;
            drawMesh(canvas, scene::extrude(pieces[i].footprint, height), pieces[i].color, offset);
        }

        output::drawText(canvas, 20, 16, "3D room: extruded footprints, oblique view", kOutline, 2);
        output::drawText(canvas, 20, kHeight - 20,
                         "plan (x, y) at height h -> X = x, Y = h, Z = y; pieces rise to their real heights.",
                         kOutline, 1);
        if (!gif.addFrame(canvas)) {
            ok = false;
        }
    }

    ok = gif.finish() && ok;
    reportGif(ok, gif);
}

} // namespace

int main(int argc, char** argv) {
    const std::string directory = (argc > 1) ? argv[1] : ROOMCRAFT_DEMO_OUTPUT_DIR "/cp2";
    std::printf("Writing Part 2 scene GIFs to %s\n", directory.c_str());

    makeFanGif(directory);
    makeChairGif(directory);
    makeDoorGif(directory);
    makeRoom3DGif(directory);

    if (failures == 0) {
        std::printf("cp2_scene_demo: all GIFs written successfully\n");
        return 0;
    }
    std::printf("cp2_scene_demo: %d failure(s)\n", failures);
    return 1;
}
