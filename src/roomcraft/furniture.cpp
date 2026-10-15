#include "roomcraft/furniture.h"

#include "raster/clipping.h"
#include "raster/fill.h"
#include "raster/line.h"

#include <algorithm>
#include <cmath>

namespace roomcraft {

raster::Polygon makeCircleFootprint(raster::Point center, float radius, int sides) {
    sides = std::max(sides, 3);
    raster::Polygon polygon;
    for (int i = 0; i < sides; ++i) {
        const double angle = 2.0 * raster::kPi * i / sides;
        polygon.vertices.push_back({static_cast<float>(center.x + radius * std::cos(angle)),
                                    static_cast<float>(center.y + radius * std::sin(angle))});
    }
    return polygon;
}

bool hitTest(const Furniture& piece, raster::Point point) {
    return raster::pointInPolygon(piece.footprint, point);
}

int findFurnitureAt(const std::vector<Furniture>& pieces, raster::Point point) {
    for (size_t i = pieces.size(); i > 0; --i) {
        if (hitTest(pieces[i - 1], point)) {
            return static_cast<int>(i - 1);
        }
    }
    return -1;
}

float areaInsideRoom(const Furniture& piece, const raster::Polygon& room) {
    return raster::computeArea(raster::clipPolygonSutherlandHodgman(piece.footprint, room));
}

bool fitsInRoom(const Furniture& piece, const raster::Polygon& room) {
    const float total = raster::computeArea(piece.footprint);
    if (total <= 0.0f) {
        return false; // a degenerate footprint is not a real piece of furniture
    }
    // Clipping computes new vertices in floating point, so allow a tiny
    // relative error instead of demanding exact equality.
    return areaInsideRoom(piece, room) >= total * (1.0f - 1e-4f);
}

void drawFurniture(raster::Canvas& canvas, const Furniture& piece) {
    raster::fillPolygonScanline(canvas, piece.footprint, piece.color);
    raster::drawPolygonOutline(canvas, piece.footprint, raster::Color{40, 40, 40});
}

} // namespace roomcraft
