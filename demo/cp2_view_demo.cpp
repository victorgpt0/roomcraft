// RoomCraft Checkpoint 2, Part 3 demo: viewing transforms.
//
// Writes five animations to demo_output/cp2/ (or to the folder given as the
// first argument):
//
//   view_zoom        the floor plan through a 2D camera that pans to the
//                    armchair, zooms in about it (it stays still on screen),
//                    pans across the room and zooms back out; furniture is
//                    clipped cleanly at the viewport edge
//   view_minimap     two viewports on one canvas: a zoomed main view and a
//                    full-room minimap showing the main view's window
//   view_orbit       the room as a wireframe, the orbit camera going once round
//   view_projection  orthographic and perspective side by side, same orbit
//   view_reveal      the camera starting straight above the room (it looks
//                    like the 2D plan) and tilting down into a 3D view
//
// Floor plan units: 1 pixel = 1 cm at zoom 1; the room is 5.6 m x 4.0 m, as
// in the Checkpoint 1 demo. 3D: X = plan x, Y = height, Z = plan y.

#include "output/animation_writer.h"
#include "output/text.h"
#include "raster/canvas.h"
#include "raster/curves.h"
#include "raster/line.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "view/camera2d.h"
#include "view/camera3d.h"
#include "view/mat4.h"
#include "xform/vec.h"

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
using raster::Rect;
using roomcraft::Furniture;
using view::Camera2D;
using view::Mat4;
using view::OrbitCamera;
using xform::Vec3;
using xform::WireMesh;

namespace {

// ---- Palette (same family as the Checkpoint 1 demo) ------------------------
const Color kWhite{255, 255, 255};
const Color kOutline{40, 40, 40};
const Color kText{30, 30, 40};
const Color kWall{60, 60, 70};
const Color kFloor{238, 226, 205};
const Color kGrid{214, 198, 172};
const Color kWindow{90, 160, 220};
const Color kDoorArc{150, 150, 150};
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
const Color kSelect{255, 140, 0};
const Color kBackdrop{72, 76, 88};     // canvas outside the 2D viewports
const Color kBackdropText{235, 235, 240};
const Color kPanel{232, 234, 238};     // canvas around the 3D viewports

constexpr int kFps = 20;
constexpr double kTwoPi = 2.0 * raster::kPi;

// ---- Room geometry (as in the Checkpoint 1 demo) ----------------------------
constexpr float kRoomLeft = 40.0f;
constexpr float kRoomTop = 40.0f;
constexpr float kRoomRight = 600.0f;
constexpr float kRoomBottom = 440.0f;
constexpr float kWallHalf = 4.0f; // walls are 8 cm thick
constexpr float kDoorLeft = 440.0f;
constexpr float kDoorRight = 520.0f;
constexpr float kWindowLeft = 250.0f;
constexpr float kWindowRight = 370.0f;
constexpr float kWallHeight = 250.0f;

const Rect kRoom{kRoomLeft, kRoomTop, kRoomRight, kRoomBottom};
const Point kRoomCentre{320.0f, 240.0f};
const Point kArmchairCentre{310.0f, 110.0f};
const Point kRugCentre{470.0f, 182.0f};

int failures = 0;

// Ease in and out (smoothstep): starts and ends gently, so camera moves do
// not jerk. (Part 4 owns the real easing library; this demo needs only one.)
float smooth(float t) {
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

Color lighter(Color c, float amount) {
    auto mix = [amount](uint8_t v) {
        return static_cast<uint8_t>(v + (255 - v) * amount + 0.5f);
    };
    return Color{mix(c.r), mix(c.g), mix(c.b)};
}

Color darker(Color c, float amount) {
    auto mix = [amount](uint8_t v) { return static_cast<uint8_t>(v * (1.0f - amount) + 0.5f); };
    return Color{mix(c.r), mix(c.g), mix(c.b)};
}

void clearTo(Canvas& canvas, Color c) {
    canvas.clear(c.r, c.g, c.b);
}

// Paints the pixels of `rect` (half-open rule) in one colour.
void fillRect(Canvas& canvas, const Rect& rect, Color c) {
    for (int y = static_cast<int>(rect.ymin); y < static_cast<int>(rect.ymax); ++y) {
        for (int x = static_cast<int>(rect.xmin); x < static_cast<int>(rect.xmax); ++x) {
            canvas.setPixel(x, y, c);
        }
    }
}

// A 1-pixel frame just OUTSIDE the viewport, so it never covers what the
// camera drew.
void drawViewportFrame(Canvas& canvas, const Rect& v, Color c) {
    raster::drawPolygonOutline(canvas, raster::makeRectangle(v.xmin - 1, v.ymin - 1,
                                                             v.xmax - v.xmin + 1, v.ymax - v.ymin + 1), c);
}

void drawCross(Canvas& canvas, Point p, int size, Color c) {
    const int x = raster::roundToPixel(p.x);
    const int y = raster::roundToPixel(p.y);
    raster::drawLineBresenham(canvas, x - size, y, x + size, y, c);
    raster::drawLineBresenham(canvas, x, y - size, x, y + size, c);
}

class Gif {
public:
    Gif(const std::string& dir, const std::string& name, int width, int height)
        : writer_(dir, name, width, height, kFps) {}

    void add(const Canvas& canvas) {
        if (!writer_.addFrame(canvas)) {
            ok_ = false;
        }
    }

    void finish() {
        if (!writer_.finish() || !ok_) {
            std::printf("  ERROR: could not write %s\n", writer_.path().c_str());
            ++failures;
            return;
        }
        std::printf("  wrote %s (%d frames, %d colours)\n", writer_.path().c_str(),
                    writer_.frameCount(), writer_.colorCount());
    }

private:
    output::AnimationWriter writer_;
    bool ok_ = true;
};

// ---- Furniture (the Checkpoint 1 floor plan) ---------------------------------

Polygon makeEllipseFootprint(Point centre, float radiusX, float radiusY, int sides) {
    Polygon ellipse = roomcraft::makeCircleFootprint(centre, radiusX, sides);
    raster::scale(ellipse, 1.0f, radiusY / radiusX, centre);
    return ellipse;
}

Polygon makeKidneyTable(Point c, float s) {
    auto at = [&](float dx, float dy) { return Point{c.x + dx * s, c.y + dy * s}; };
    const std::vector<std::vector<Point>> pieces = {
        raster::sampleBezierCubic(at(-45, 0), at(-45, -38), at(45, -38), at(45, 0), 24),
        raster::sampleBezierCubic(at(45, 0), at(45, 28), at(18, 30), at(0, 16), 16),
        raster::sampleBezierCubic(at(0, 16), at(-18, 30), at(-45, 28), at(-45, 0), 16),
    };
    Polygon kidney;
    for (const std::vector<Point>& piece : pieces) {
        kidney.vertices.insert(kidney.vertices.end(), piece.begin(), piece.end() - 1);
    }
    return kidney;
}

std::vector<Furniture> makeFurniture() {
    std::vector<Furniture> pieces;
    pieces.push_back({"Rug", makeEllipseFootprint(kRugCentre, 56, 40, 64), kRug, {}});
    pieces.push_back({"Bed", raster::makeRectangle(60, 60, 140, 200), kBed, {}});
    pieces.push_back({"Nightstand", raster::makeRectangle(208, 60, 36, 36), kWood, {}});
    pieces.push_back({"Wardrobe", raster::makeRectangle(48, 290, 60, 140), kWood, {}});
    Polygon sofa;
    sofa.vertices = {{380, 60}, {580, 60}, {580, 220}, {530, 220}, {530, 110}, {380, 110}};
    pieces.push_back({"L-shaped sofa", sofa, kSofa, {}});
    // The armchair is defined centred on (0, 0) and placed by its pose
    // (Part 1): the same place and angle as in the Checkpoint 1 demo.
    Furniture armchair{"Armchair", raster::makeRectangle(-35, -35, 70, 70), kSofa, {}};
    armchair.pose.position = kArmchairCentre;
    armchair.pose.angleDegrees = 25.0f;
    pieces.push_back(armchair);
    pieces.push_back({"Dining table", roomcraft::makeCircleFootprint({200, 360}, 45, 64), kWood, {}});
    const Point chairs[4] = {{200, 298}, {262, 360}, {200, 422}, {138, 360}};
    for (int i = 0; i < 4; ++i) {
        pieces.push_back({"Chair " + std::to_string(i + 1),
                          roomcraft::makeCircleFootprint(chairs[i], 14, 32), kChair, {}});
    }
    pieces.push_back({"Coffee table", makeKidneyTable(kRugCentre, 0.7f), kGlassTable, {}});
    pieces.push_back({"Plant", roomcraft::makeCircleFootprint({574, 412}, 14, 32), kPot, {}});
    return pieces;
}

// The wall pieces as rectangles 8 cm thick, with gaps for the door and the
// window. As polygons they are scaled by the camera like everything else
// (a thick LINE would stay the same number of pixels at every zoom).
std::vector<Polygon> makeWalls() {
    const float l = kRoomLeft - kWallHalf, r = kRoomRight + kWallHalf;
    const float t = kRoomTop - kWallHalf, b = kRoomBottom + kWallHalf;
    const float w = 2 * kWallHalf;
    return {
        raster::makeRectangle(l, t, kWindowLeft - l, w),
        raster::makeRectangle(kWindowRight, t, r - kWindowRight, w),
        raster::makeRectangle(l, kRoomBottom - kWallHalf, kDoorLeft - l, w),
        raster::makeRectangle(kDoorRight, kRoomBottom - kWallHalf, r - kDoorRight, w),
        raster::makeRectangle(l, t, w, b - t),
        raster::makeRectangle(kRoomRight - kWallHalf, t, w, b - t),
    };
}

// The door's swing: a quarter circle about the hinge, as sample points
// (clockwise on screen: 270 = up, 360 = along the wall to the right).
std::vector<Point> doorArc() {
    std::vector<Point> points;
    const float radius = kDoorRight - kDoorLeft;
    for (int i = 0; i <= 24; ++i) {
        const double a = (270.0 + 90.0 * i / 24.0) * raster::kPi / 180.0;
        points.push_back({kDoorLeft + radius * static_cast<float>(std::cos(a)),
                          kRoomBottom + radius * static_cast<float>(std::sin(a))});
    }
    return points;
}

// Draws the whole floor plan through a 2D camera. Every shape is in ROOM
// coordinates; the camera maps it to the viewport and clips it there.
void drawPlan(Canvas& canvas, const Camera2D& camera, const std::vector<Furniture>& pieces) {
    view::fillPolygonThroughCamera(canvas, raster::makeRectangle(kRoomLeft, kRoomTop,
                                                                 kRoomRight - kRoomLeft,
                                                                 kRoomBottom - kRoomTop),
                                   camera, kFloor);
    for (float x = 50; x < kRoomRight; x += 50) {
        view::drawLineThroughCamera(canvas, {x, kRoomTop}, {x, kRoomBottom}, camera, kGrid);
    }
    for (float y = 50; y < kRoomBottom; y += 50) {
        view::drawLineThroughCamera(canvas, {kRoomLeft, y}, {kRoomRight, y}, camera, kGrid);
    }

    // One piece at a time, so each piece's details are drawn before the
    // next piece goes on top (the coffee table lies on the rug).
    for (const Furniture& piece : pieces) {
        view::drawFurnitureThroughCamera(canvas, {piece}, camera);
        if (piece.name == "Rug") {
            view::fillPolygonThroughCamera(canvas, makeEllipseFootprint(kRugCentre, 49, 33, 64), camera,
                                           kRugInner);
            view::drawPolygonOutlineThroughCamera(canvas, makeEllipseFootprint(kRugCentre, 46, 30, 64),
                                                  camera, kRug);
        } else if (piece.name == "Bed") {
            for (float cx : {98.0f, 162.0f}) {
                const Polygon pillow = makeEllipseFootprint({cx, 82}, 28, 13, 40);
                view::fillPolygonThroughCamera(canvas, pillow, camera, kPillow);
                view::drawPolygonOutlineThroughCamera(canvas, pillow, camera, kOutline);
            }
            view::drawLineThroughCamera(canvas, {61, 120}, {199, 120}, camera, kOutline);
        } else if (piece.name == "Plant") {
            view::fillPolygonThroughCamera(canvas, roomcraft::makeCircleFootprint({574, 412}, 10, 32),
                                           camera, kPlant);
        }
    }

    for (const Polygon& wall : makeWalls()) {
        view::fillPolygonThroughCamera(canvas, wall, camera, kWall);
    }
    for (float offset : {-3.0f, 0.0f, 3.0f}) {
        view::drawLineThroughCamera(canvas, {kWindowLeft, kRoomTop + offset},
                                    {kWindowRight, kRoomTop + offset}, camera, kWindow);
    }
    view::fillPolygonThroughCamera(canvas, raster::makeRectangle(kDoorLeft - 1.5f, kRoomBottom - 80, 3, 80),
                                   camera, kWall);
    const std::vector<Point> arc = doorArc();
    for (size_t i = 0; i + 1 < arc.size(); ++i) {
        view::drawLineThroughCamera(canvas, arc[i], arc[i + 1], camera, kDoorArc);
    }
}

// ---- 1. view_zoom ------------------------------------------------------------

void animateZoom(const std::string& dir) {
    const int width = 640, height = 480;
    Canvas canvas(width, height);
    Gif gif(dir, "view_zoom", width, height);
    const std::vector<Furniture> pieces = makeFurniture();

    // The viewport has the room's aspect ratio (560 x 400 px). The starting
    // window is the room plus a 16 cm margin, so the walls (which stand half
    // outside the room's inner outline) are fully visible: zoom 0.95.
    const Rect viewport{40, 56, 600, 456};
    Camera2D camera{Rect{kRoomLeft - 16, kRoomTop - 16, kRoomRight + 16, kRoomBottom + 16}, viewport};
    view::matchViewportAspect(camera);
    const Camera2D start = camera;

    bool showAnchor = false;
    std::string caption;
    auto frame = [&]() {
        clearTo(canvas, kBackdrop);
        drawPlan(canvas, camera, pieces);
        drawViewportFrame(canvas, viewport, kBackdropText);
        if (showAnchor) {
            drawCross(canvas, view::roomToScreen(camera, kArmchairCentre), 7, kSelect);
        }
        char line[96];
        std::snprintf(line, sizeof line, "ZOOM %.1fX   WINDOW X %.0f..%.0f  Y %.0f..%.0f CM",
                      view::scaleX(camera), camera.window.xmin, camera.window.xmax,
                      camera.window.ymin, camera.window.ymax);
        output::drawText(canvas, 40, 14, caption, kBackdropText, 2);
        output::drawText(canvas, 40, 464, line, kBackdropText, 1);
        gif.add(canvas);
    };

    // Moves the camera over `frames` frames: pans by `panTotal` (room cm) and
    // multiplies the zoom by `zoomTotal`, both eased. The zoom is about
    // `anchor`, or about the window's centre if anchor is null. Each frame
    // applies only the CHANGE since the last frame, through view::pan and
    // view::zoomAbout.
    auto move = [&](int frames, Point panTotal, float zoomTotal, const Point* anchor) {
        float done = 0.0f;
        for (int i = 1; i <= frames; ++i) {
            const float now = smooth(static_cast<float>(i) / frames);
            const float step = now - done;
            done = now;
            view::pan(camera, panTotal.x * step, panTotal.y * step);
            const Point centre{(camera.window.xmin + camera.window.xmax) / 2,
                               (camera.window.ymin + camera.window.ymax) / 2};
            view::zoomAbout(camera, std::pow(zoomTotal, step), anchor ? *anchor : centre);
            frame();
        }
    };
    auto hold = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            frame();
        }
    };

    caption = "2D CAMERA: THE WHOLE ROOM";
    hold(12);
    caption = "PAN TO THE ARMCHAIR";
    move(26, {kArmchairCentre.x - kRoomCentre.x, kArmchairCentre.y - kRoomCentre.y}, 1.0f, nullptr);
    caption = "ZOOM ABOUT THE ARMCHAIR (IT STAYS STILL)";
    showAnchor = true;
    move(34, {0, 0}, 3.5f, &kArmchairCentre);
    hold(10);
    showAnchor = false;
    caption = "PAN: PIECES CLIPPED AT THE VIEWPORT";
    move(30, {180, 40}, 1.0f, nullptr);
    move(30, {-330, 160}, 1.0f, nullptr);
    hold(6);
    caption = "ZOOM OUT AND PAN BACK";
    const Point centre{(camera.window.xmin + camera.window.xmax) / 2,
                       (camera.window.ymin + camera.window.ymax) / 2};
    move(36, {kRoomCentre.x - centre.x, kRoomCentre.y - centre.y}, 1.0f / 3.5f, nullptr);
    camera = start; // remove any rounding drift, so the loop joins exactly
    caption = "2D CAMERA: THE WHOLE ROOM";
    hold(4);
    gif.finish();
}

// ---- 2. view_minimap ---------------------------------------------------------

void animateMinimap(const std::string& dir) {
    const int width = 800, height = 480;
    Canvas canvas(width, height);
    Gif gif(dir, "view_minimap", width, height);
    const std::vector<Furniture> pieces = makeFurniture();

    const Rect mainViewport{20, 50, 580, 450};
    Camera2D mainCamera{kRoom, mainViewport};
    view::matchViewportAspect(mainCamera);

    // The minimap: the whole room (plus the wall thickness) in a small
    // viewport. matchViewportAspect keeps the room undistorted.
    const Rect miniViewport{600, 50, 780, 180};
    Camera2D minimap{Rect{kRoomLeft - 10, kRoomTop - 10, kRoomRight + 10, kRoomBottom + 10}, miniViewport};
    view::matchViewportAspect(minimap);
    const float miniPixel = 1.0f / view::scaleX(minimap); // one minimap pixel in cm

    const int frames = 140;
    float zoom = 1.0f;
    Point centre = kRoomCentre;
    for (int i = 0; i < frames; ++i) {
        // The main camera tours the room on a figure of eight, zooming
        // between 2x and 3x. Each frame pans to the new centre and zooms
        // about it, using the camera helpers.
        const double t = static_cast<double>(i) / frames;
        const Point next{320.0f + 175.0f * static_cast<float>(std::sin(kTwoPi * t)),
                         240.0f + 105.0f * static_cast<float>(std::sin(2.0 * kTwoPi * t))};
        const float nextZoom = 2.5f + 0.5f * static_cast<float>(std::cos(kTwoPi * t));
        view::pan(mainCamera, next.x - centre.x, next.y - centre.y);
        view::zoomAbout(mainCamera, nextZoom / zoom, next);
        centre = next;
        zoom = nextZoom;

        clearTo(canvas, kBackdrop);
        drawPlan(canvas, mainCamera, pieces);
        drawPlan(canvas, minimap, pieces);
        drawViewportFrame(canvas, mainViewport, kBackdropText);
        drawViewportFrame(canvas, miniViewport, kBackdropText);

        // The main view's window, drawn on the minimap: its four corners are
        // room points, so the minimap camera maps them like any other shape.
        // Each edge is clipped to the minimap (Cohen-Sutherland).
        const Rect& w = mainCamera.window;
        for (float grow : {0.0f, miniPixel}) {
            view::drawPolygonOutlineThroughCamera(
                canvas, raster::makeRectangle(w.xmin - grow, w.ymin - grow, w.xmax - w.xmin + 2 * grow,
                                              w.ymax - w.ymin + 2 * grow),
                minimap, kSelect);
        }

        // The pixel at the centre of the main view, mapped back to the room
        // through the INVERSE camera matrix (what a mouse click does), and
        // that room point shown on the minimap.
        const Point screenCentre{(mainViewport.xmin + mainViewport.xmax) / 2,
                                 (mainViewport.ymin + mainViewport.ymax) / 2};
        Point roomPoint{};
        view::screenToRoom(mainCamera, screenCentre, roomPoint);
        drawCross(canvas, screenCentre, 8, kSelect);
        drawCross(canvas, view::roomToScreen(minimap, roomPoint), 3, kOutline);

        char line[80];
        output::drawText(canvas, 20, 18, "MAIN VIEW", kBackdropText, 2);
        std::snprintf(line, sizeof line, "ZOOM %.1fX", zoom);
        output::drawText(canvas, 580 - output::textWidth(line, 2), 18, line, kBackdropText, 2);
        output::drawText(canvas, 600, 18, "MINIMAP", kBackdropText, 2);
        output::drawText(canvas, 600, 196, "ORANGE = MAIN", kBackdropText, 1);
        output::drawText(canvas, 600, 208, "VIEW'S WINDOW", kBackdropText, 1);
        output::drawText(canvas, 600, 232, "CENTRE PIXEL", kBackdropText, 1);
        output::drawText(canvas, 600, 244, "MAPPED BACK:", kBackdropText, 1);
        std::snprintf(line, sizeof line, "(%.0f, %.0f) CM", roomPoint.x, roomPoint.y);
        output::drawText(canvas, 600, 256, line, kBackdropText, 1);
        gif.add(canvas);
    }
    gif.finish();
}

// ---- 3D scene: boxes built by hand -----------------------------------------

// A box from four plan corners (in order round the footprint) between two
// heights: 8 vertices (0-3 the bottom, 4-7 the top) and 12 edges (4 round
// the bottom, 4 round the top, 4 upright). World X = plan x, Y = height,
// Z = plan y.
WireMesh makeBox(const Point (&corners)[4], float bottom, float top) {
    WireMesh box;
    for (float h : {bottom, top}) {
        for (const Point& c : corners) {
            box.vertices.push_back(Vec3{c.x, h, c.y});
        }
    }
    for (int i = 0; i < 4; ++i) {
        box.edges.push_back({i, (i + 1) % 4});         // bottom
        box.edges.push_back({4 + i, 4 + (i + 1) % 4}); // top
        box.edges.push_back({i, 4 + i});               // upright
    }
    return box;
}

WireMesh makeBox(float x0, float y0, float x1, float y1, float bottom, float top) {
    const Point corners[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    return makeBox(corners, bottom, top);
}

struct Solid {
    WireMesh mesh;
    Color color;
};

struct Scene3D {
    std::vector<Solid> solids;
    WireMesh grid;                 // floor grid, drawn faintly
    std::vector<Furniture> plan;   // footprints painted on the floor
};

Scene3D makeScene() {
    Scene3D scene;
    scene.plan = makeFurniture();

    scene.solids.push_back({makeBox(kRoomLeft, kRoomTop, kRoomRight, kRoomBottom, 0, kWallHeight), kWall});
    scene.solids.push_back({makeBox(60, 60, 200, 260, 0, 50), darker(kBed, 0.45f)});
    scene.solids.push_back({makeBox(60, 60, 200, 72, 0, 100), darker(kBed, 0.45f)}); // headboard
    scene.solids.push_back({makeBox(208, 60, 244, 96, 0, 55), darker(kWood, 0.3f)});
    scene.solids.push_back({makeBox(48, 290, 108, 430, 0, 200), darker(kWood, 0.3f)});
    scene.solids.push_back({makeBox(380, 60, 580, 110, 0, 80), darker(kSofa, 0.3f)});
    scene.solids.push_back({makeBox(530, 110, 580, 220, 0, 80), darker(kSofa, 0.3f)});
    scene.solids.push_back({makeBox(155, 315, 245, 405, 0, 75), darker(kWood, 0.3f)});

    // The armchair box, turned by the same pose as its footprint: its four
    // corners come from the piece's world footprint.
    for (const Furniture& piece : scene.plan) {
        if (piece.name == "Armchair") {
            const Polygon world = roomcraft::worldFootprint(piece);
            const Point corners[4] = {world.vertices[0], world.vertices[1], world.vertices[2],
                                      world.vertices[3]};
            scene.solids.push_back({makeBox(corners, 0, 85), darker(kSofa, 0.3f)});
        }
    }

    // Door frame in the front wall and window frame in the back wall.
    WireMesh door;
    door.vertices = {{kDoorLeft, 0, kRoomBottom}, {kDoorLeft, 205, kRoomBottom},
                     {kDoorRight, 205, kRoomBottom}, {kDoorRight, 0, kRoomBottom}};
    door.edges = {{0, 1}, {1, 2}, {2, 3}};
    scene.solids.push_back({door, kWall});
    WireMesh window;
    window.vertices = {{kWindowLeft, 90, kRoomTop}, {kWindowRight, 90, kRoomTop},
                       {kWindowRight, 190, kRoomTop}, {kWindowLeft, 190, kRoomTop}};
    window.edges = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    scene.solids.push_back({window, kWindow});

    for (float x = 50; x < kRoomRight; x += 50) {
        const int i = static_cast<int>(scene.grid.vertices.size());
        scene.grid.vertices.push_back({x, 0, kRoomTop});
        scene.grid.vertices.push_back({x, 0, kRoomBottom});
        scene.grid.edges.push_back({i, i + 1});
    }
    for (float z = 50; z < kRoomBottom; z += 50) {
        const int i = static_cast<int>(scene.grid.vertices.size());
        scene.grid.vertices.push_back({kRoomLeft, 0, z});
        scene.grid.vertices.push_back({kRoomRight, 0, z});
        scene.grid.edges.push_back({i, i + 1});
    }
    return scene;
}

// The room's 3D camera settings shared by the 3D animations.
constexpr float kFovY = 50.0f;
constexpr float kNear = 10.0f;
constexpr float kFar = 5000.0f;
constexpr float kOrbitRadius = 850.0f;
const Vec3 kTarget{320.0f, 60.0f, 240.0f};

// Draws the 3D scene into `viewport`: the floor and the furniture footprints
// filled on the floor (Y = 0), the floor grid, then every wireframe box.
void drawScene(Canvas& canvas, const Scene3D& scene, const Mat4& viewProjection, const Rect& viewport) {
    fillRect(canvas, viewport, kWhite);
    view::fillPlanPolygon(canvas, raster::makeRectangle(kRoomLeft, kRoomTop, kRoomRight - kRoomLeft,
                                                        kRoomBottom - kRoomTop),
                          0.0f, viewProjection, kFloor, viewport);
    for (const Furniture& piece : scene.plan) {
        view::fillPlanPolygon(canvas, roomcraft::worldFootprint(piece), 0.0f, viewProjection,
                              lighter(piece.color, 0.35f), viewport);
    }
    view::drawWireMesh(canvas, scene.grid, viewProjection, kGrid, viewport);
    for (const Solid& solid : scene.solids) {
        view::drawWireMesh(canvas, solid.mesh, viewProjection, solid.color, viewport);
    }
}

Mat4 perspectiveFor(const Rect& viewport, float fovY = kFovY) {
    const float aspect = (viewport.xmax - viewport.xmin) / (viewport.ymax - viewport.ymin);
    return Mat4::perspective(fovY, aspect, kNear, kFar);
}

// An orthographic box that shows the plane through the target at the same
// size as the perspective view does: at distance d the perspective view is
// 2 d tan(fov / 2) high.
Mat4 orthographicFor(const Rect& viewport, float distance) {
    const float aspect = (viewport.xmax - viewport.xmin) / (viewport.ymax - viewport.ymin);
    const float halfHeight = distance * static_cast<float>(std::tan(kFovY * raster::kPi / 360.0));
    const float halfWidth = halfHeight * aspect;
    return Mat4::orthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, kNear, kFar);
}

void drawAngles(Canvas& canvas, int x, int y, const OrbitCamera& camera) {
    char line[80];
    std::snprintf(line, sizeof line, "AZIMUTH %3.0f  ELEVATION %2.0f", camera.azimuthDegrees,
                  camera.elevationDegrees);
    output::drawText(canvas, x, y, line, kText, 1);
}

// ---- 3. view_orbit -------------------------------------------------------------

void animateOrbit(const std::string& dir, const Scene3D& scene) {
    const int width = 640, height = 480;
    Canvas canvas(width, height);
    Gif gif(dir, "view_orbit", width, height);
    const Rect viewport{0, 0, static_cast<float>(width), static_cast<float>(height)};
    const Mat4 projection = perspectiveFor(viewport);

    OrbitCamera camera;
    camera.target = kTarget;
    camera.radius = kOrbitRadius;
    camera.elevationDegrees = 30.0f;
    const int frames = 90; // 4 degrees per frame
    for (int i = 0; i < frames; ++i) {
        camera.azimuthDegrees = 360.0f * i / frames;
        drawScene(canvas, scene, projection * view::viewMatrix(camera), viewport);
        output::drawText(canvas, 12, 12, "ORBIT CAMERA (PERSPECTIVE)", kText, 2);
        drawAngles(canvas, 12, 34, camera);
        gif.add(canvas);
    }
    gif.finish();
}

// ---- 4. view_projection ------------------------------------------------------

void animateProjection(const std::string& dir, const Scene3D& scene) {
    const int width = 960, height = 420;
    Canvas canvas(width, height);
    Gif gif(dir, "view_projection", width, height);
    const Rect left{16, 44, 472, 404};
    const Rect right{488, 44, 944, 404};
    const Mat4 ortho = orthographicFor(left, kOrbitRadius);
    const Mat4 persp = perspectiveFor(right);

    OrbitCamera camera;
    camera.target = kTarget;
    camera.radius = kOrbitRadius;
    camera.elevationDegrees = 25.0f;
    const int frames = 72; // 5 degrees per frame
    for (int i = 0; i < frames; ++i) {
        camera.azimuthDegrees = 360.0f * i / frames;
        const Mat4 viewM = view::viewMatrix(camera); // ONE view matrix for both
        clearTo(canvas, kPanel);
        drawScene(canvas, scene, ortho * viewM, left);
        drawScene(canvas, scene, persp * viewM, right);
        drawViewportFrame(canvas, left, kDoorArc);
        drawViewportFrame(canvas, right, kDoorArc);
        output::drawText(canvas, 16, 14, "ORTHOGRAPHIC", kText, 2);
        output::drawText(canvas, 488, 14, "PERSPECTIVE", kText, 2);
        output::drawText(canvas, 16, 408, "PARALLEL LINES STAY PARALLEL, SIZE IGNORES DISTANCE", kText, 1);
        output::drawText(canvas, 488, 408, "FAR THINGS SMALLER: X AND Y DIVIDED BY W", kText, 1);
        drawAngles(canvas, 472 - output::textWidth("AZIMUTH 000  ELEVATION 00"), 18, camera);
        gif.add(canvas);
    }
    gif.finish();
}

// ---- 5. view_reveal ------------------------------------------------------------

void animateReveal(const std::string& dir, const Scene3D& scene) {
    const int width = 640, height = 480;
    Canvas canvas(width, height);
    Gif gif(dir, "view_reveal", width, height);
    const Rect viewport{0, 0, static_cast<float>(width), static_cast<float>(height)};
    // A long lens (narrow field of view, camera further away): the wall tops
    // are then only 1500 / 1250 = 1.2x nearer than the floor in the top view,
    // so the walls flare out less and the picture is closer to the flat plan.
    const Mat4 projection = perspectiveFor(viewport, 25.0f);

    OrbitCamera camera;
    camera.target = Vec3{kTarget.x, 100.0f, kTarget.z}; // a metre up: frames the 3D view lower
    camera.radius = 1500.0f;
    camera.azimuthDegrees = 0.0f;
    camera.elevationDegrees = 90.0f;

    auto frame = [&](const char* caption) {
        drawScene(canvas, scene, projection * view::viewMatrix(camera), viewport);
        output::drawText(canvas, 12, 12, caption, kText, 2);
        drawAngles(canvas, 12, 34, camera);
        gif.add(canvas);
    };

    // Top view: the floor is parallel to the image plane, so perspective only
    // SCALES it: it is exactly the 2D floor plan (only the box tops, which
    // are nearer to the camera, look a little bigger).
    for (int i = 0; i < 14; ++i) {
        frame("ELEVATION 90: THE 2D PLAN");
    }
    const int tilt = 60;
    for (int i = 1; i <= tilt; ++i) {
        const float t = smooth(static_cast<float>(i) / tilt);
        camera.elevationDegrees = 90.0f - 62.0f * t;
        camera.azimuthDegrees = 30.0f * t;
        frame("TILTING DOWN INTO 3D");
    }
    for (int i = 0; i < 14; ++i) {
        frame("ELEVATION 28: THE 3D VIEW");
    }
    const int back = 40;
    for (int i = 1; i <= back; ++i) {
        const float t = smooth(static_cast<float>(i) / back);
        camera.elevationDegrees = 28.0f + 62.0f * t;
        camera.azimuthDegrees = 30.0f * (1.0f - t);
        frame("BACK TO THE PLAN");
    }
    gif.finish();
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : std::string(ROOMCRAFT_DEMO_OUTPUT_DIR) + "/cp2";
    std::error_code error;
    std::filesystem::create_directories(dir, error);

    std::printf("RoomCraft CP2 Part 3: viewing transforms -> %s\n", dir.c_str());
    animateZoom(dir);
    animateMinimap(dir);
    const Scene3D scene = makeScene();
    animateOrbit(dir, scene);
    animateProjection(dir, scene);
    animateReveal(dir, scene);

    if (failures > 0) {
        std::printf("%d animation(s) could not be written\n", failures);
        return 1;
    }
    std::printf("done\n");
    return 0;
}
