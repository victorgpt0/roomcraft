#pragma once

#include <utility>
#include <vector>

// Small 3D types shared by the Checkpoint 2 parts.
//
// The 2D floor plan keeps using raster::Point. These types are for the 3D
// preview of the room: plan point (x, y) at height h becomes the world point
// (X = x, Y = h, Z = y), with Y pointing UP and units in centimetres.

namespace xform {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Homogeneous 3D point. w = 1 for an ordinary point; after a perspective
// projection w holds the depth that x, y and z must be divided by.
struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

// Wireframe model: vertices plus index pairs for the edges. Each edge is a
// pair of indices into `vertices`.
//
// RoomCraft use: furniture footprints extruded into boxes for the 3D preview.
struct WireMesh {
    std::vector<Vec3> vertices;
    std::vector<std::pair<int, int>> edges;
};

} // namespace xform
