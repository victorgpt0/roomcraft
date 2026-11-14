#include "scene/extrude.h"

#include <cmath>
#include <cstddef>
#include <utility>

namespace scene {

xform::WireMesh extrude(const raster::Polygon& worldFootprint, float height) {
    xform::WireMesh mesh;
    const std::size_t n = worldFootprint.vertices.size();
    mesh.vertices.reserve(2 * n);
    mesh.edges.reserve(3 * n);

    // Bottom ring first, then top ring, so vertex i and vertex n + i are the
    // two ends of the same vertical upright.
    for (const raster::Point& p : worldFootprint.vertices) {
        mesh.vertices.push_back(xform::Vec3{p.x, 0.0f, p.y});
    }
    for (const raster::Point& p : worldFootprint.vertices) {
        mesh.vertices.push_back(xform::Vec3{p.x, height, p.y});
    }

    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t next = (i + 1) % n;
        const int bottom = static_cast<int>(i);
        const int bottomNext = static_cast<int>(next);
        const int top = static_cast<int>(n + i);
        const int topNext = static_cast<int>(n + next);

        mesh.edges.push_back({bottom, bottomNext}); // floor outline
        mesh.edges.push_back({top, topNext});       // ceiling / table top
        mesh.edges.push_back({bottom, top});        // wall / side of the prism
    }

    return mesh;
}

xform::WireMesh extrudedRoom(const raster::Polygon& roomOutline, float wallHeight) {
    // An extruded room outline already contains the three pieces we need:
    // the bottom ring is the floor perimeter, each upright is a wall corner,
    // and the top ring joins the corners along the top of the walls.
    return extrude(roomOutline, wallHeight);
}

raster::Point obliqueProject(const xform::Vec3& point, raster::Point offset) {
    // cos(45) = sin(45) = sqrt(2) / 2. The 0.5 is the cabinet-projection
    // depth scale: half-size depth keeps the drawing readable.
    constexpr double kDepthScale = 0.5;
    const double cos45 = std::cos(45.0 * raster::kPi / 180.0);
    const double sin45 = std::sin(45.0 * raster::kPi / 180.0);

    const double screenX = point.x + kDepthScale * point.z * cos45 + offset.x;
    const double screenY = -point.y + kDepthScale * point.z * sin45 + offset.y;
    return raster::Point{static_cast<float>(screenX), static_cast<float>(screenY)};
}

} // namespace scene
