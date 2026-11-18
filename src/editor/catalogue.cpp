#include "editor/catalogue.h"

#include "raster/curves.h"
#include "xform/mat3.h"

#include <algorithm>
#include <cmath>

namespace editor {

raster::Polygon centredRectangle(float width, float depth) {
    return raster::makeRectangle(-width / 2.0f, -depth / 2.0f, width, depth);
}

raster::Polygon rectangleFromTo(float x0, float y0, float x1, float y1) {
    return raster::makeRectangle(std::min(x0, x1), std::min(y0, y1), std::fabs(x1 - x0), std::fabs(y1 - y0));
}

raster::Polygon circlePolygon(raster::Point centre, float radius, int sides) {
    return ellipsePolygon(centre, radius, radius, sides);
}

raster::Polygon ellipsePolygon(raster::Point centre, float radiusX, float radiusY, int sides) {
    sides = std::max(sides, 3);
    raster::Polygon polygon;
    for (int i = 0; i < sides; ++i) {
        const double angle = 2.0 * raster::kPi * i / sides;
        polygon.vertices.push_back({static_cast<float>(centre.x + radiusX * std::cos(angle)),
                                    static_cast<float>(centre.y + radiusY * std::sin(angle))});
    }
    return polygon;
}

raster::Color partColor(const Part& part, raster::Color itemColor) {
    if (!part.useItemColor) {
        return part.color;
    }
    const auto shade = [&part](uint8_t value) {
        const float v = static_cast<float>(value) * part.shade;
        return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
    };
    return raster::Color{shade(itemColor.r), shade(itemColor.g), shade(itemColor.b)};
}

namespace {

const raster::Color kWhiteLinen{238, 236, 228};
const raster::Color kDarkWood{92, 66, 48};
const raster::Color kMetal{70, 72, 78};
const raster::Color kScreen{28, 30, 36};
const raster::Color kLampShade{250, 232, 180};
const raster::Color kTerracotta{176, 98, 62};

Part block(raster::Polygon footprint, float bottom, float top, float shade = 1.0f) {
    Part part;
    part.footprint = std::move(footprint);
    part.bottom = bottom;
    part.top = top;
    part.shade = shade;
    return part;
}

Part fixedBlock(raster::Polygon footprint, float bottom, float top, raster::Color color) {
    Part part = block(std::move(footprint), bottom, top);
    part.useItemColor = false;
    part.color = color;
    return part;
}

// Four square legs of side `size`, inset `inset` from the corners of a
// width x depth rectangle centred on (0, 0).
std::vector<Part> legs(float width, float depth, float size, float inset, float height, raster::Color color) {
    std::vector<Part> parts;
    for (const float sx : {-1.0f, 1.0f}) {
        for (const float sy : {-1.0f, 1.0f}) {
            const float cx = sx * (width / 2.0f - inset - size / 2.0f);
            const float cy = sy * (depth / 2.0f - inset - size / 2.0f);
            parts.push_back(fixedBlock(rectangleFromTo(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2), 0.0f,
                                       height, color));
        }
    }
    return parts;
}

// Kidney-shaped table top made of three cubic Bezier curves (as in the
// Checkpoint 1 demo), centred on (0, 0) and scaled by s.
raster::Polygon kidney(float s) {
    const auto at = [s](float dx, float dy) { return raster::Point{dx * s, dy * s}; };
    const std::vector<std::vector<raster::Point>> pieces = {
        raster::sampleBezierCubic(at(-45, 0), at(-45, -38), at(45, -38), at(45, 0), 20),
        raster::sampleBezierCubic(at(45, 0), at(45, 28), at(18, 30), at(0, 16), 14),
        raster::sampleBezierCubic(at(0, 16), at(-18, 30), at(-45, 28), at(-45, 0), 14),
    };
    raster::Polygon polygon;
    for (const std::vector<raster::Point>& piece : pieces) {
        polygon.vertices.insert(polygon.vertices.end(), piece.begin(), piece.end() - 1);
    }
    // Centre it vertically: the curves span y from about -28 to +22.
    for (raster::Point& p : polygon.vertices) {
        p.y += 3.0f * s;
    }
    return polygon;
}

std::vector<FurnitureType> buildCatalogue() {
    std::vector<FurnitureType> types;

    // ---- Bedroom -----------------------------------------------------------
    {
        FurnitureType t{"bed_double", "Double bed", "Bedroom", centredRectangle(160, 205), {120, 140, 175}, 100.0f, false, {}};
        t.parts.push_back(block(rectangleFromTo(-78, -94, 78, 102.5f), 0, 32, 0.75f));            // base
        t.parts.push_back(fixedBlock(rectangleFromTo(-76, -92, 76, 98), 32, 54, kWhiteLinen));     // mattress
        t.parts.push_back(block(rectangleFromTo(-76, -20, 76, 98), 54, 58, 1.1f));                 // duvet
        t.parts.push_back(block(rectangleFromTo(-80, -102.5f, 80, -94), 0, 100, 0.65f));           // headboard
        t.parts.push_back(fixedBlock(rectangleFromTo(-68, -88, -8, -60), 54, 64, kWhiteLinen));    // pillows
        t.parts.push_back(fixedBlock(rectangleFromTo(8, -88, 68, -60), 54, 64, kWhiteLinen));
        types.push_back(t);
    }
    {
        FurnitureType t{"bed_single", "Single bed", "Bedroom", centredRectangle(95, 205), {150, 170, 120}, 90.0f, false, {}};
        t.parts.push_back(block(rectangleFromTo(-46, -94, 46, 102.5f), 0, 32, 0.75f));
        t.parts.push_back(fixedBlock(rectangleFromTo(-44, -92, 44, 98), 32, 54, kWhiteLinen));
        t.parts.push_back(block(rectangleFromTo(-44, -20, 44, 98), 54, 58, 1.1f));
        t.parts.push_back(block(rectangleFromTo(-47.5f, -102.5f, 47.5f, -94), 0, 90, 0.65f));
        t.parts.push_back(fixedBlock(rectangleFromTo(-36, -88, 36, -60), 54, 64, kWhiteLinen));
        types.push_back(t);
    }
    {
        FurnitureType t{"nightstand", "Nightstand", "Bedroom", centredRectangle(45, 40), {181, 136, 99}, 75.0f, false, {}};
        t.parts.push_back(block(centredRectangle(45, 40), 0, 50));
        t.parts.push_back(fixedBlock(circlePolygon({0, -3}, 7, 16), 50, 56, kMetal));       // lamp base
        t.parts.push_back(fixedBlock(circlePolygon({0, -3}, 13, 20), 56, 75, kLampShade));  // shade
        types.push_back(t);
    }
    {
        FurnitureType t{"wardrobe", "Wardrobe", "Bedroom", centredRectangle(120, 60), {181, 136, 99}, 210.0f, false, {}};
        t.parts.push_back(block(rectangleFromTo(-60, -30, 60, 28), 0, 210));
        t.parts.push_back(block(rectangleFromTo(-58, 28, -1, 30), 10, 200, 1.12f));  // two door panels
        t.parts.push_back(block(rectangleFromTo(1, 28, 58, 30), 10, 200, 1.12f));
        types.push_back(t);
    }

    // ---- Living ------------------------------------------------------------
    {
        FurnitureType t{"sofa", "Sofa", "Living", centredRectangle(210, 90), {120, 150, 110}, 85.0f, false, {}};
        t.parts.push_back(block(centredRectangle(210, 90), 0, 42, 0.85f));                  // base and seat
        t.parts.push_back(block(rectangleFromTo(-105, -45, 105, -22), 42, 85, 0.95f));     // backrest
        t.parts.push_back(block(rectangleFromTo(-105, -22, -86, 45), 42, 62));             // arms
        t.parts.push_back(block(rectangleFromTo(86, -22, 105, 45), 42, 62));
        t.parts.push_back(block(rectangleFromTo(-85, -21, -1, 42), 42, 50, 1.1f));         // cushions
        t.parts.push_back(block(rectangleFromTo(1, -21, 85, 42), 42, 50, 1.1f));
        types.push_back(t);
    }
    {
        raster::Polygon l;
        l.vertices = {{-125, -85}, {125, -85}, {125, 85}, {55, 85}, {55, -15}, {-125, -15}};
        FurnitureType t{"sofa_l", "L-shaped sofa", "Living", l, {110, 135, 150}, 85.0f, false, {}};
        t.parts.push_back(block(l, 0, 42, 0.85f));                                          // seat (concave)
        t.parts.push_back(block(rectangleFromTo(-125, -85, 125, -63), 42, 85, 0.95f));      // back along the top
        t.parts.push_back(block(rectangleFromTo(103, -63, 125, 85), 42, 85, 0.95f));        // back along the side
        t.parts.push_back(block(rectangleFromTo(-125, -63, -107, -15), 42, 62));            // arm
        t.parts.push_back(block(rectangleFromTo(56, 66, 102, 85), 42, 62));                 // arm at the chaise end
        types.push_back(t);
    }
    {
        FurnitureType t{"armchair", "Armchair", "Living", centredRectangle(85, 85), {170, 110, 80}, 85.0f, false, {}};
        t.parts.push_back(block(centredRectangle(85, 85), 0, 42, 0.85f));
        t.parts.push_back(block(rectangleFromTo(-42.5f, -42.5f, 42.5f, -24), 42, 85, 0.95f));
        t.parts.push_back(block(rectangleFromTo(-42.5f, -24, -29, 42.5f), 42, 62));
        t.parts.push_back(block(rectangleFromTo(29, -24, 42.5f, 42.5f), 42, 62));
        t.parts.push_back(block(rectangleFromTo(-28, -23, 28, 40), 42, 50, 1.1f));
        types.push_back(t);
    }
    {
        const raster::Polygon top = kidney(1.3f);
        FurnitureType t{"coffee_table", "Coffee table", "Living", top, {150, 110, 80}, 42.0f, false, {}};
        t.parts.push_back(block(top, 38, 42));
        t.parts.push_back(block(xform::transformPolygon(xform::Mat3::scaling(0.75f, 0.6f), top), 0, 38, 0.6f));
        types.push_back(t);
    }
    {
        FurnitureType t{"tv_unit", "TV unit", "Living", centredRectangle(160, 42), {60, 60, 66}, 125.0f, false, {}};
        t.parts.push_back(block(centredRectangle(160, 42), 0, 50));
        t.parts.push_back(fixedBlock(rectangleFromTo(-8, -6, 8, 6), 50, 56, kMetal));        // stand
        t.parts.push_back(fixedBlock(rectangleFromTo(-62, -3, 62, 3), 56, 125, kScreen));    // screen
        types.push_back(t);
    }
    {
        FurnitureType t{"bookshelf", "Bookshelf", "Living", centredRectangle(90, 32), {181, 136, 99}, 190.0f, false, {}};
        t.parts.push_back(block(rectangleFromTo(-45, -16, 45, -12), 0, 190));                // back
        t.parts.push_back(block(rectangleFromTo(-45, -12, -42, 16), 0, 190));                // sides
        t.parts.push_back(block(rectangleFromTo(42, -12, 45, 16), 0, 190));
        for (int shelf = 0; shelf < 5; ++shelf) {                                            // shelves
            const float y = 2.0f + 46.0f * static_cast<float>(shelf);
            t.parts.push_back(block(rectangleFromTo(-42, -12, 42, 16), y, y + 3.0f, 0.9f));
        }
        // A few rows of books, each a coloured block.
        const raster::Color books[4] = {{170, 60, 60}, {60, 100, 150}, {200, 170, 80}, {80, 130, 90}};
        for (int shelf = 0; shelf < 4; ++shelf) {
            const float y = 5.0f + 46.0f * static_cast<float>(shelf);
            float x = -40.0f;
            for (int book = 0; x + 10.0f < 42.0f; ++book) {
                const float width = 5.0f + static_cast<float>((book * 7 + shelf * 3) % 5);
                const float height = 26.0f + static_cast<float>((book * 5 + shelf) % 4) * 4.0f;
                t.parts.push_back(fixedBlock(rectangleFromTo(x, -10, x + width - 0.6f, 12), y, y + height,
                                             books[(book + shelf) % 4]));
                x += width;
            }
        }
        types.push_back(t);
    }

    // ---- Dining ------------------------------------------------------------
    {
        const raster::Polygon top = circlePolygon({0, 0}, 50, 40);
        FurnitureType t{"table_round", "Round table", "Dining", top, {181, 136, 99}, 76.0f, false, {}};
        t.parts.push_back(block(top, 72, 76));
        t.parts.push_back(block(circlePolygon({0, 0}, 7, 16), 3, 72, 0.6f));     // pedestal
        t.parts.push_back(block(circlePolygon({0, 0}, 28, 24), 0, 3, 0.6f));     // foot
        types.push_back(t);
    }
    {
        FurnitureType t{"table_rect", "Dining table", "Dining", centredRectangle(160, 90), {181, 136, 99}, 76.0f, false, {}};
        t.parts.push_back(block(centredRectangle(160, 90), 72, 76));
        for (Part& leg : legs(160, 90, 6, 4, 72, kDarkWood)) {
            t.parts.push_back(leg);
        }
        types.push_back(t);
    }
    {
        FurnitureType t{"chair", "Chair", "Dining", centredRectangle(45, 48), {96, 125, 160}, 90.0f, false, {}};
        t.parts.push_back(block(rectangleFromTo(-22.5f, -20, 22.5f, 24), 43, 47));        // seat
        t.parts.push_back(block(rectangleFromTo(-22.5f, -24, 22.5f, -20), 43, 90, 0.85f)); // backrest
        for (Part& leg : legs(45, 48, 4, 1, 43, kDarkWood)) {
            t.parts.push_back(leg);
        }
        types.push_back(t);
    }

    // ---- Office ------------------------------------------------------------
    {
        FurnitureType t{"desk", "Desk", "Office", centredRectangle(130, 65), {200, 200, 195}, 112.0f, false, {}};
        t.parts.push_back(block(centredRectangle(130, 65), 72, 76));
        for (Part& leg : legs(130, 65, 5, 2, 72, kMetal)) {
            t.parts.push_back(leg);
        }
        t.parts.push_back(fixedBlock(rectangleFromTo(-28, -28, 28, -25), 76, 112, kScreen)); // monitor
        types.push_back(t);
    }
    {
        const raster::Polygon seat = circlePolygon({0, 0}, 26, 24);
        FurnitureType t{"office_chair", "Office chair", "Office", seat, {50, 55, 65}, 100.0f, false, {}};
        t.parts.push_back(fixedBlock(circlePolygon({0, 0}, 26, 5), 0, 5, kMetal));  // five-star base
        t.parts.push_back(fixedBlock(circlePolygon({0, 0}, 4, 12), 5, 42, kMetal)); // column
        t.parts.push_back(block(circlePolygon({0, 2}, 23, 24), 42, 50));           // seat
        t.parts.push_back(block(rectangleFromTo(-14, -20, 14, -15), 50, 100, 0.9f)); // back, inside the round footprint
        types.push_back(t);
    }

    // ---- Decor -------------------------------------------------------------
    {
        const raster::Polygon rug = ellipsePolygon({0, 0}, 110, 75, 48);
        FurnitureType t{"rug", "Oval rug", "Decor", rug, {190, 80, 90}, 1.2f, true, {}};
        t.parts.push_back(block(rug, 0, 0.8f));
        t.parts.push_back(block(ellipsePolygon({0, 0}, 85, 55, 48), 0.8f, 1.2f, 1.3f)); // inner band
        types.push_back(t);
    }
    {
        const raster::Polygon pot = circlePolygon({0, 0}, 22, 24);
        FurnitureType t{"plant", "Plant", "Decor", pot, {70, 140, 70}, 110.0f, false, {}};
        t.parts.push_back(fixedBlock(circlePolygon({0, 0}, 16, 20), 0, 38, kTerracotta)); // pot
        t.parts.push_back(block(circlePolygon({0, 0}, 22, 7), 38, 80));                  // foliage, chunky
        t.parts.push_back(block(circlePolygon({0, 0}, 15, 7), 80, 110, 1.15f));
        types.push_back(t);
    }
    {
        const raster::Polygon base = circlePolygon({0, 0}, 18, 20);
        FurnitureType t{"floor_lamp", "Floor lamp", "Decor", circlePolygon({0, 0}, 20, 20), {230, 215, 170}, 165.0f, false, {}};
        t.parts.push_back(fixedBlock(base, 0, 3, kMetal));
        t.parts.push_back(fixedBlock(circlePolygon({0, 0}, 2, 8), 3, 140, kMetal));
        t.parts.push_back(block(circlePolygon({0, 0}, 20, 20), 140, 165));
        types.push_back(t);
    }

    // Centre every type on the centroid of its footprint, so that a pose
    // rotation turns the piece on the spot (the L-shaped sofa and the
    // kidney table are not symmetric, so their natural drawing coordinates
    // are off-centre).
    for (FurnitureType& type : types) {
        const raster::Point c = raster::computeCentroid(type.footprint);
        const xform::Mat3 shift = xform::Mat3::translation(-c.x, -c.y);
        type.footprint = xform::transformPolygon(shift, type.footprint);
        for (Part& part : type.parts) {
            part.footprint = xform::transformPolygon(shift, part.footprint);
        }
    }
    return types;
}

} // namespace

const std::vector<FurnitureType>& catalogue() {
    static const std::vector<FurnitureType> types = buildCatalogue();
    return types;
}

const FurnitureType* findType(const std::string& id) {
    for (const FurnitureType& type : catalogue()) {
        if (type.id == id) {
            return &type;
        }
    }
    return nullptr;
}

} // namespace editor
