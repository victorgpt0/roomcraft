#pragma once

#include "raster/polygon.h"
#include "raster/types.h"
#include "xform/vec.h"

// Lifting the 2D floor plan into 3D.
//
// The floor plan lives in screen coordinates: x to the right, y DOWN. The 3D
// preview uses a right-handed world with Y UP. Parts 2, 3 and 4 share one
// mapping between them:
//
//     plan point (x, y) at height h  ->  world point (X = x, Y = h, Z = y)
//
// So the plan's x becomes world X, the plan's y becomes world Z (depth), and
// height becomes world Y. The floor is Y = 0; walls and furniture rise in +Y.
//
// RoomCraft use: turning furniture footprints and the room outline into
// wireframe prisms for the 3D preview, without needing a modelling library.

namespace scene {

// Lifts a floor-plan polygon into a 3D prism from the floor up to `height`.
//
// A footprint with n vertices produces exactly:
//   - 2n vertices: n on the bottom ring (Y = 0) and n on the top ring
//     (Y = height), both in the same order as the footprint;
//   - 3n edges: the n bottom edges, the n top edges, and the n vertical
//     uprights joining matching bottom/top vertices.
// The bottom and top rings are closed (the last vertex joins the first).
xform::WireMesh extrude(const raster::Polygon& worldFootprint, float height);

// Builds the room itself: the floor outline at Y = 0 plus walls of the given
// height. The bottom ring is the floor perimeter, the uprights are the wall
// corners, and the top ring outlines the tops of the walls.
xform::WireMesh extrudedRoom(const raster::Polygon& roomOutline, float wallHeight);

// Oblique (cabinet) projection used by the Part 2 demo until the real 3D
// camera arrives in Part 3:
//
//     screen x = X + 0.5 * Z * cos(45 degrees) + offset.x
//     screen y = -Y + 0.5 * Z * sin(45 degrees) + offset.y
//
// The -Y flips world-up into screen-down, and the Z terms fold depth back
// into the picture. This is a SHEAR: points are slid by an amount
// proportional to Z, so parallel lines stay parallel and the floor plan keeps
// its shape (there is no vanishing point). The 0.5 factor is why cabinet
// drawings look "half as deep" as the front face.
raster::Point obliqueProject(const xform::Vec3& point, raster::Point offset = {0.0f, 0.0f});

} // namespace scene
