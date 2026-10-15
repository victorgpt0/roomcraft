// RoomCraft demo: applies every Checkpoint 1 technique to RoomCraft objects
// and writes one image per technique plus a complete 2D floor plan.
//
// Usage:  roomcraft_demo [output-directory]
// Without an argument the images go to ROOMCRAFT_DEMO_OUTPUT_DIR (set by
// CMake to <repo>/demo_output). Each image is written as .ppm and .bmp.
//
// Floor plan units: 1 pixel = 1 cm. The room is 5.6 m x 4.0 m.

#include "raster/canvas.h"
#include "raster/circle.h"
#include "raster/clipping.h"
#include "raster/curves.h"
#include "raster/ellipse.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

#ifndef ROOMCRAFT_DEMO_OUTPUT_DIR
#define ROOMCRAFT_DEMO_OUTPUT_DIR "demo_output"
#endif

using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Polygon;
using roomcraft::Furniture;

namespace {

// ---- Palette ---------------------------------------------------------------
const Color kWhite{255, 255, 255};
const Color kBlack{0, 0, 0};
const Color kOutline{40, 40, 40};
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
const Color kWarning{220, 30, 30};
const Color kSelect{255, 140, 0};

// ---- Room geometry (shared by all images) ----------------------------------
constexpr float kRoomLeft = 40.0f;
constexpr float kRoomTop = 40.0f;
constexpr float kRoomWidth = 560.0f;  // 5.6 m
constexpr float kRoomHeight = 400.0f; // 4.0 m
constexpr int kWallThickness = 8;

// Door in the bottom wall (hinge on the left), window in the top wall.
constexpr int kDoorLeft = 440;
constexpr int kDoorRight = 520;
constexpr int kWindowLeft = 250;
constexpr int kWindowRight = 370;

Polygon roomOutline() {
    return raster::makeRectangle(kRoomLeft, kRoomTop, kRoomWidth, kRoomHeight);
}

raster::Rect roomRect() {
    return raster::Rect{kRoomLeft, kRoomTop, kRoomLeft + kRoomWidth, kRoomTop + kRoomHeight};
}

int failures = 0;

void save(const Canvas& canvas, const std::string& directory, const std::string& name) {
    const std::string base = directory + "/" + name;
    const bool ppm = canvas.savePPM(base + ".ppm");
    const bool bmp = canvas.saveBMP(base + ".bmp");
    if (ppm && bmp) {
        std::printf("  wrote %s.ppm / .bmp\n", base.c_str());
    } else {
        std::printf("  ERROR: could not write %s (does the folder exist?)\n", base.c_str());
        ++failures;
    }
}

Canvas blankCanvas(int width, int height) {
    Canvas canvas(width, height);
    canvas.clear(kWhite.r, kWhite.g, kWhite.b);
    return canvas;
}

void drawCross(Canvas& canvas, Point p, int size, Color color) {
    const int x = raster::roundToPixel(p.x);
    const int y = raster::roundToPixel(p.y);
    raster::drawLineBresenham(canvas, x - size, y - size, x + size, y + size, color);
    raster::drawLineBresenham(canvas, x - size, y + size, x + size, y - size, color);
}

void drawThickOutline(Canvas& canvas, const Polygon& polygon, int thickness, Color color) {
    const std::vector<Point>& v = polygon.vertices;
    for (size_t i = 0; i < v.size(); ++i) {
        const Point& a = v[i];
        const Point& b = v[(i + 1) % v.size()];
        raster::drawThickLine(canvas, raster::roundToPixel(a.x), raster::roundToPixel(a.y),
                              raster::roundToPixel(b.x), raster::roundToPixel(b.y), thickness, color);
    }
}

void drawDashedOutline(Canvas& canvas, const Polygon& polygon, Color color) {
    const std::vector<Point>& v = polygon.vertices;
    for (size_t i = 0; i < v.size(); ++i) {
        const Point& a = v[i];
        const Point& b = v[(i + 1) % v.size()];
        raster::drawDashedLine(canvas, raster::roundToPixel(a.x), raster::roundToPixel(a.y),
                               raster::roundToPixel(b.x), raster::roundToPixel(b.y), 5, 3, color);
    }
}

// ---- Room drawing ----------------------------------------------------------

// Floor (scanline fill) and the 50 cm editor grid. The grid lines are
// generated across the WHOLE canvas and clipped to the room with
// Cohen-Sutherland, so they stop exactly at the walls.
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

// Walls as thick lines, with gaps for the door and the window. The window is
// drawn as three thin lines, the usual floor plan symbol.
void drawWalls(Canvas& canvas) {
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

// Door: the open door leaf (a line) and its swing (a quarter-circle arc).
// Hinge at the left end of the door gap; the door opens into the room.
// Angles are clockwise on screen: 270 = up (door fully open), 360 = along
// the wall to the right (door closed).
void drawDoor(Canvas& canvas) {
    const int hingeX = kDoorLeft;
    const int hingeY = static_cast<int>(kRoomTop + kRoomHeight);
    const int width = kDoorRight - kDoorLeft;
    raster::drawThickLine(canvas, hingeX, hingeY, hingeX, hingeY - width, 3, kWall);
    raster::drawCircleArc(canvas, hingeX, hingeY, width, 270.0f, 360.0f, kDoorArc);
}

// Dimension line under the room: dashed, with end ticks every metre.
void drawDimensionLine(Canvas& canvas) {
    const int y = static_cast<int>(kRoomTop + kRoomHeight) + 22;
    const int left = static_cast<int>(kRoomLeft);
    const int right = static_cast<int>(kRoomLeft + kRoomWidth);
    raster::drawDashedLine(canvas, left, y, right, y, 6, 4, kOutline);
    for (int x = left; x <= right; x += 100) {
        raster::drawLineBresenham(canvas, x, y - 4, x, y + 4, kOutline);
    }
    raster::drawLineBresenham(canvas, right, y - 4, right, y + 4, kOutline);
}

// ---- Furniture -------------------------------------------------------------

Polygon makeEllipseFootprint(Point center, float radiusX, float radiusY) {
    // A circle polygon squashed with scale() about its centre.
    Polygon ellipse = roomcraft::makeCircleFootprint(center, radiusX, 64);
    raster::scale(ellipse, 1.0f, radiusY / radiusX, center);
    return ellipse;
}

// Kidney-shaped coffee table: three cubic Bezier curves joined end to end
// into a closed outline - one for the rounded top edge and two lobes for the
// bottom edge, which meet in a dent in the middle. The sample points become
// the polygon's vertices. Offsets are in cm at s = 1.
Polygon makeKidneyTable(Point c, float s) {
    auto at = [&](float dx, float dy) { return Point{c.x + dx * s, c.y + dy * s}; };
    const std::vector<std::vector<Point>> pieces = {
        raster::sampleBezierCubic(at(-45, 0), at(-45, -38), at(45, -38), at(45, 0), 24),  // top
        raster::sampleBezierCubic(at(45, 0), at(45, 28), at(18, 30), at(0, 16), 16),      // right lobe
        raster::sampleBezierCubic(at(0, 16), at(-18, 30), at(-45, 28), at(-45, 0), 16),   // left lobe
    };
    Polygon kidney;
    for (const std::vector<Point>& piece : pieces) {
        // Each curve starts where the previous one ended: skip that repeated
        // first point (and the last curve ends where the first one started).
        kidney.vertices.insert(kidney.vertices.end(), piece.begin(), piece.end() - 1);
    }
    return kidney;
}

Polygon makeLSofa() {
    Polygon sofa;
    sofa.vertices = {{380, 60}, {580, 60}, {580, 220}, {530, 220}, {530, 110}, {380, 110}};
    return sofa;
}

Polygon makeArmchair() {
    Polygon chair = raster::makeRectangle(275, 75, 70, 70);
    raster::rotate(chair, 25.0f, raster::computeCentroid(chair));
    return chair;
}

const Point kTableCenter{200, 360};
constexpr float kTableRadius = 45;
const Point kChairCenters[4] = {{200, 298}, {262, 360}, {200, 422}, {138, 360}};
constexpr float kChairRadius = 14;
const Point kRugCenter{470, 182};
constexpr float kRugRadiusX = 56;
constexpr float kRugRadiusY = 40;
const Point kPlantCenter{574, 412};

// Everything on the floor plan, bottom (drawn first) to top. Selection
// searches this list from the top.
std::vector<Furniture> makeFurniture() {
    std::vector<Furniture> pieces;
    pieces.push_back({"Rug", makeEllipseFootprint(kRugCenter, kRugRadiusX, kRugRadiusY), kRug});
    pieces.push_back({"Bed", raster::makeRectangle(60, 60, 140, 200), kBed});
    pieces.push_back({"Nightstand", raster::makeRectangle(208, 60, 36, 36), kWood});
    pieces.push_back({"Wardrobe", raster::makeRectangle(48, 290, 60, 140), kWood});
    pieces.push_back({"L-shaped sofa", makeLSofa(), kSofa});
    pieces.push_back({"Armchair", makeArmchair(), kSofa});
    pieces.push_back({"Dining table", roomcraft::makeCircleFootprint(kTableCenter, kTableRadius, 64), kWood});
    for (int i = 0; i < 4; ++i) {
        pieces.push_back({"Chair " + std::to_string(i + 1),
                          roomcraft::makeCircleFootprint(kChairCenters[i], kChairRadius, 32), kChair});
    }
    pieces.push_back({"Coffee table", makeKidneyTable(kRugCenter, 0.7f), kGlassTable});
    pieces.push_back({"Plant", roomcraft::makeCircleFootprint(kPlantCenter, 14, 32), kPlant});
    return pieces;
}

// Draws one piece. Polygon furniture uses scanline fill + outline; round
// furniture is drawn with the midpoint circle/ellipse algorithms so it is
// perfectly round (its polygon footprint is only used for hit testing).
void drawPiece(Canvas& canvas, const Furniture& piece) {
    const std::string& n = piece.name;
    if (n == "Rug") {
        const int cx = raster::roundToPixel(kRugCenter.x);
        const int cy = raster::roundToPixel(kRugCenter.y);
        const int rx = static_cast<int>(kRugRadiusX);
        const int ry = static_cast<int>(kRugRadiusY);
        raster::fillEllipseMidpoint(canvas, cx, cy, rx, ry, kRug);
        raster::fillEllipseMidpoint(canvas, cx, cy, rx - 7, ry - 7, kRugInner);
        raster::drawEllipseMidpoint(canvas, cx, cy, rx - 10, ry - 10, kRug);
    } else if (n == "Dining table" || n.rfind("Chair", 0) == 0 || n == "Plant") {
        const raster::Rect box = raster::computeBoundingBox(piece.footprint);
        const int cx = raster::roundToPixel((box.xmin + box.xmax) / 2.0f);
        const int cy = raster::roundToPixel((box.ymin + box.ymax) / 2.0f);
        const int r = raster::roundToPixel((box.xmax - box.xmin) / 2.0f);
        if (n == "Plant") {
            raster::fillCircleMidpoint(canvas, cx, cy, r, kPot);
            raster::fillCircleMidpoint(canvas, cx, cy, r - 4, kPlant);
            raster::drawCircleMidpoint(canvas, cx, cy, r, kOutline);
        } else {
            raster::fillCircleMidpoint(canvas, cx, cy, r, piece.color);
            raster::drawCircleMidpoint(canvas, cx, cy, r, kOutline);
        }
    } else {
        roomcraft::drawFurniture(canvas, piece);
    }

    if (n == "Bed") {
        // Pillows (ellipses) and a folded-back blanket edge.
        raster::fillEllipseMidpoint(canvas, 98, 82, 28, 13, kPillow);
        raster::drawEllipseMidpoint(canvas, 98, 82, 28, 13, kOutline);
        raster::fillEllipseMidpoint(canvas, 162, 82, 28, 13, kPillow);
        raster::drawEllipseMidpoint(canvas, 162, 82, 28, 13, kOutline);
        raster::drawThickLine(canvas, 61, 120, 199, 120, 2, kOutline);
    }
}

void drawFloorPlan(Canvas& canvas, const std::vector<Furniture>& pieces) {
    drawFloorAndGrid(canvas);
    for (const Furniture& piece : pieces) {
        drawPiece(canvas, piece);
    }
    drawWalls(canvas);
    drawDoor(canvas);
    drawDimensionLine(canvas);
}

// ---- One image per technique ----------------------------------------------

// 1. Lines: walls (thick lines), editor grid clipped to the room
//    (Cohen-Sutherland), dimension line (dashed), DDA vs Bresenham.
void imageLines(const std::string& dir) {
    Canvas canvas = blankCanvas(640, 500);

    // Grid lines across the whole canvas, shown faintly outside the room
    // (what clipping removes) and clipped to the room inside.
    const Color faint{235, 235, 235};
    for (int x = 0; x <= 640; x += 50) {
        raster::drawLineBresenham(canvas, x, 0, x, 499, faint);
    }
    for (int y = 0; y <= 500; y += 50) {
        raster::drawLineBresenham(canvas, 0, y, 639, y, faint);
    }
    drawFloorAndGrid(canvas);
    drawWalls(canvas);
    drawDoor(canvas);
    drawDimensionLine(canvas);

    // The same diagonal "measuring tape" with DDA (red, upper) and
    // Bresenham (blue, 6 px lower): the pixels are the same.
    raster::drawLineDDA(canvas, 80, 380, 400, 120, Color{220, 0, 0});
    raster::drawLineBresenham(canvas, 80, 386, 400, 126, Color{0, 0, 220});

    // A polyline: the outline of a planned kitchen counter.
    raster::drawPolyline(canvas, {{430, 70}, {580, 70}, {580, 200}, {540, 200}, {540, 110}, {430, 110}},
                         Color{0, 120, 0});
    save(canvas, dir, "01_lines");
}

// 2. Polygons: an armchair footprint moved, rotated and scaled about its
//    centroid, with bounding boxes and centroids; area of an L-shaped sofa.
void imagePolygons(const std::string& dir) {
    Canvas canvas = blankCanvas(640, 500);
    drawFloorAndGrid(canvas);

    Polygon chair = raster::makeRectangle(60, 80, 80, 70);
    raster::drawPolygonOutline(canvas, chair, Color{150, 150, 150}); // original

    Polygon moved = chair;
    raster::translate(moved, 110, 0);
    raster::drawPolygonOutline(canvas, moved, kOutline);

    Polygon turned = moved;
    raster::translate(turned, 110, 0);
    raster::rotate(turned, 30.0f, raster::computeCentroid(turned));
    raster::fillPolygonScanline(canvas, turned, kSofa);
    raster::drawPolygonOutline(canvas, turned, kOutline);

    Polygon bigger = turned;
    raster::translate(bigger, 150, 0);
    raster::scale(bigger, 1.3f, 1.3f, raster::computeCentroid(bigger));
    raster::fillPolygonScanline(canvas, bigger, kSofa);
    raster::drawPolygonOutline(canvas, bigger, kOutline);

    for (const Polygon* p : {&chair, &moved, &turned, &bigger}) {
        const raster::Rect box = raster::computeBoundingBox(*p);
        drawDashedOutline(canvas, raster::makeRectangle(box.xmin, box.ymin, box.xmax - box.xmin, box.ymax - box.ymin),
                          Color{0, 120, 220});
        drawCross(canvas, raster::computeCentroid(*p), 4, kWarning);
    }

    Polygon sofa = makeLSofa();
    raster::translate(sofa, -300, 160);
    raster::fillPolygonScanline(canvas, sofa, kSofa);
    raster::drawPolygonOutline(canvas, sofa, kOutline);
    drawCross(canvas, raster::computeCentroid(sofa), 4, kWarning);

    drawWalls(canvas);
    std::printf("  armchair area %.2f m2 before and %.2f m2 after scaling by 1.3 (x1.69)\n",
                raster::computeArea(turned) / 10000.0f, raster::computeArea(bigger) / 10000.0f);
    const Point sofaCentroid = raster::computeCentroid(sofa);
    std::printf("  L-shaped sofa area %.2f m2, centroid (%.1f, %.1f) is %s the sofa%s\n",
                raster::computeArea(sofa) / 10000.0f, sofaCentroid.x, sofaCentroid.y,
                raster::pointInPolygon(sofa, sofaCentroid) ? "inside" : "OUTSIDE",
                raster::pointInPolygon(sofa, sofaCentroid) ? "" : " (concave shape: it lies in the inner corner)");
    save(canvas, dir, "02_polygons");
}

// 3. Clipping and filling: furniture pushed through a wall is clipped to the
//    room (Sutherland-Hodgman) and only the inside part is filled; floor
//    tiles show that the scanline fill leaves no gaps or overlaps; a rug zone
//    painted with boundary fill.
void imageClippingFilling(const std::string& dir) {
    Canvas canvas = blankCanvas(640, 500);
    drawFloorAndGrid(canvas);
    const Polygon room = roomOutline();

    // Tiled area: 8 x 5 tiles of 30 cm, alternating colours, each filled
    // separately. Neighbouring tiles share edges exactly.
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 8; ++col) {
            const Color tile = ((row + col) % 2 == 0) ? Color{200, 210, 220} : Color{160, 175, 190};
            raster::fillPolygonScanline(canvas,
                                        raster::makeRectangle(60.0f + 30.0f * static_cast<float>(col),
                                                              280.0f + 30.0f * static_cast<float>(row), 30, 30),
                                        tile);
        }
    }

    // A bookshelf pushed through the right wall and a rotated desk poking
    // through the top wall: dashed red = full footprint, filled = clipped part.
    const Polygon shelf = raster::makeRectangle(540, 180, 100, 40);
    Polygon desk = raster::makeRectangle(300, 50, 160, 70);
    raster::rotate(desk, -20.0f, raster::computeCentroid(desk));
    for (const Polygon* p : std::initializer_list<const Polygon*>{&shelf, &desk}) {
        const Polygon inside = raster::clipPolygonSutherlandHodgman(*p, room);
        raster::fillPolygonScanline(canvas, inside, kWood);
        raster::drawPolygonOutline(canvas, inside, kOutline);
        drawDashedOutline(canvas, *p, kWarning);
        std::printf("  clipped footprint: %.0f%% of the piece is inside the room\n",
                    100.0f * raster::computeArea(inside) / raster::computeArea(*p));
    }

    // Boundary fill: an irregular "play mat" zone drawn as an outline, then
    // painted from a seed point inside it.
    Polygon zone;
    zone.vertices = {{380, 290}, {470, 270}, {560, 300}, {575, 380}, {500, 420}, {420, 410}, {400, 350}};
    const Color zoneLine{60, 90, 160};
    raster::drawPolygonOutline(canvas, zone, zoneLine);
    const int painted = raster::boundaryFill(canvas, 480, 340, Color{180, 200, 240}, zoneLine);
    std::printf("  boundary fill painted %d pixels inside the play-mat outline\n", painted);

    drawWalls(canvas);
    save(canvas, dir, "03_clipping_filling");
}

// 4. Circles and ellipses: round dining table with chairs, oval rug,
//    pillows, plant pots and door swings (arcs).
void imageCirclesEllipses(const std::string& dir) {
    Canvas canvas = blankCanvas(640, 500);
    drawFloorAndGrid(canvas);

    // Dining set.
    raster::fillCircleMidpoint(canvas, 200, 200, 70, kWood);
    raster::drawCircleMidpoint(canvas, 200, 200, 70, kOutline);
    for (int i = 0; i < 6; ++i) {
        const double a = i * 60.0 * raster::kPi / 180.0;
        const int x = raster::roundToPixel(200 + 92 * std::cos(a));
        const int y = raster::roundToPixel(200 + 92 * std::sin(a));
        raster::fillCircleMidpoint(canvas, x, y, 16, kChair);
        raster::drawCircleMidpoint(canvas, x, y, 16, kOutline);
    }

    // Oval rug with border and pillows.
    raster::fillEllipseMidpoint(canvas, 450, 170, 110, 65, kRug);
    raster::fillEllipseMidpoint(canvas, 450, 170, 98, 53, kRugInner);
    raster::drawEllipseMidpoint(canvas, 450, 170, 92, 47, kRug);
    for (int i = 0; i < 3; ++i) {
        const int x = 390 + 60 * i;
        raster::fillEllipseMidpoint(canvas, x, 330, 24, 14, kPillow);
        raster::drawEllipseMidpoint(canvas, x, 330, 24, 14, kOutline);
    }

    // Plant pots of different sizes.
    for (int i = 0; i < 4; ++i) {
        const int r = 8 + 4 * i;
        const int x = 160 + 50 * i;
        raster::fillCircleMidpoint(canvas, x, 395, r, kPot);
        raster::fillCircleMidpoint(canvas, x, 395, r - 4, kPlant);
        raster::drawCircleMidpoint(canvas, x, 395, r, kOutline);
    }

    drawWalls(canvas);
    drawDoor(canvas);
    // A second door in the left wall, hinged at the top, opening into the
    // room: door closed = 90 degrees (down along the wall), open = 0 (right).
    raster::fillPolygonScanline(canvas, raster::makeRectangle(36, 330, 9, 70), kFloor); // door gap
    raster::drawThickLine(canvas, 40, 330, 110, 330, 3, kWall);
    raster::drawCircleArc(canvas, 40, 330, 70, 0.0f, 90.0f, kDoorArc);
    save(canvas, dir, "04_circles_ellipses");
}

// 5. Curves: a curved sofa and a kidney coffee table (Bezier), a curtain
//    track through clicked points (Catmull-Rom), and the same points as a
//    smoother B-spline.
void imageCurves(const std::string& dir) {
    Canvas canvas = blankCanvas(640, 500);
    drawFloorAndGrid(canvas);

    // Curved sofa: outer and inner Bezier edges joined into one closed
    // band, then scanline filled.
    std::vector<Point> outer = raster::sampleBezierCubic({70, 260}, {80, 80}, {300, 80}, {310, 260}, 40);
    std::vector<Point> inner = raster::sampleBezierCubic({120, 260}, {130, 135}, {250, 135}, {260, 260}, 40);
    Polygon band;
    band.vertices = outer;
    band.vertices.insert(band.vertices.end(), inner.rbegin(), inner.rend());
    raster::fillPolygonScanline(canvas, band, kSofa);
    raster::drawPolygonOutline(canvas, band, kOutline);

    // Control polygon of the outer edge, to show how Bezier control points
    // pull the curve.
    const Color control{0, 120, 220};
    raster::drawDashedLine(canvas, 70, 260, 80, 80, 4, 3, control);
    raster::drawDashedLine(canvas, 80, 80, 300, 80, 4, 3, control);
    raster::drawDashedLine(canvas, 300, 80, 310, 260, 4, 3, control);
    for (Point p : {Point{70, 260}, Point{80, 80}, Point{300, 80}, Point{310, 260}}) {
        drawCross(canvas, p, 3, control);
    }

    // Kidney coffee table in front of it.
    const Polygon kidney = makeKidneyTable({190, 330}, 1.0f);
    raster::fillPolygonScanline(canvas, kidney, kGlassTable);
    raster::drawPolygonOutline(canvas, kidney, kOutline);

    // Curtain track: Catmull-Rom through the clicked points (passes through
    // all of them), B-spline of the same points (smoother, does not).
    const std::vector<Point> clicks = {{360, 420}, {400, 300}, {470, 360}, {520, 200}, {580, 260}, {560, 110}};
    raster::drawCatmullRomSpline(canvas, clicks, 24, Color{150, 0, 150});
    raster::drawPolyline(canvas, raster::uniformBSpline(clicks, 24), Color{0, 150, 0});
    for (const Point& p : clicks) {
        drawCross(canvas, p, 4, kBlack);
    }

    drawWalls(canvas);
    save(canvas, dir, "05_curves");
}

// 6. The complete floor plan.
void imageFloorPlan(const std::string& dir, const std::vector<Furniture>& pieces) {
    Canvas canvas = blankCanvas(640, 500);
    drawFloorPlan(canvas, pieces);
    save(canvas, dir, "06_floor_plan");
}

// 7. Editor logic: simulated clicks select furniture (hit test), and a
//    bookshelf pushed through the wall is flagged by fitsInRoom.
void imageSelection(const std::string& dir, std::vector<Furniture> pieces) {
    pieces.push_back({"Bookshelf", raster::makeRectangle(560, 260, 80, 36), kWood});
    const Polygon room = roomOutline();

    Canvas canvas = blankCanvas(640, 500);
    drawFloorAndGrid(canvas);
    for (const Furniture& piece : pieces) {
        if (roomcraft::fitsInRoom(piece, room)) {
            drawPiece(canvas, piece);
        } else {
            // Draw only the part inside the room, and the full outline in red.
            Furniture inside = piece;
            inside.footprint = raster::clipPolygonSutherlandHodgman(piece.footprint, room);
            roomcraft::drawFurniture(canvas, inside);
            drawThickOutline(canvas, piece.footprint, 2, kWarning);
        }
    }
    drawWalls(canvas);
    drawDoor(canvas);

    std::printf("  fit check (room %.1f m2):\n", raster::computeArea(room) / 10000.0f);
    for (const Furniture& piece : pieces) {
        std::printf("    %-14s %5.2f m2  %s\n", piece.name.c_str(), raster::computeArea(piece.footprint) / 10000.0f,
                    roomcraft::fitsInRoom(piece, room) ? "fits" : "DOES NOT FIT (sticks through a wall)");
    }

    const std::vector<Point> clicks = {{120, 170}, {470, 182}, {515, 185}, {555, 160}, {200, 360},
                                       {300, 260}, {20, 20}};
    std::printf("  simulated clicks:\n");
    for (const Point& click : clicks) {
        const int index = roomcraft::findFurnitureAt(pieces, click);
        if (index >= 0) {
            drawThickOutline(canvas, pieces[static_cast<size_t>(index)].footprint, 3, kSelect);
        }
        drawCross(canvas, click, 5, kBlack);
        std::printf("    click (%3.0f, %3.0f) -> %s\n", click.x, click.y,
                    index >= 0 ? pieces[static_cast<size_t>(index)].name.c_str() : "nothing (empty floor)");
    }
    save(canvas, dir, "07_selection");
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ROOMCRAFT_DEMO_OUTPUT_DIR;
    std::printf("RoomCraft Checkpoint 1 demo - writing images to %s\n", dir.c_str());

    std::printf("1. Lines\n");
    imageLines(dir);
    std::printf("2. Polygons\n");
    imagePolygons(dir);
    std::printf("3. Clipping and filling\n");
    imageClippingFilling(dir);
    std::printf("4. Circles and ellipses\n");
    imageCirclesEllipses(dir);
    std::printf("5. Curves\n");
    imageCurves(dir);

    const std::vector<Furniture> pieces = makeFurniture();
    std::printf("6. Floor plan (%zu pieces of furniture)\n", pieces.size());
    imageFloorPlan(dir, pieces);
    std::printf("7. Selection and fit check\n");
    imageSelection(dir, pieces);

    if (failures > 0) {
        std::printf("%d image(s) could not be written\n", failures);
        return 1;
    }
    std::printf("Done.\n");
    return 0;
}
