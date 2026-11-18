#pragma once

#include "editor/layout.h"
#include "raster/polygon.h"
#include "raster/types.h"
#include "xform/vec.h"

#include <cstdint>
#include <vector>

// Building geometry for the GPU, on the CPU.
//
// OpenGL draws vertex buffers. This module turns the room and its furniture
// into plain arrays - no OpenGL here, so it can be tested without a window.
// The renderer (src/gfx) uploads the arrays to the GPU as they are.
//
// Every vertex carries a WORLD position (X = plan x, Y = height, Z = plan y),
// a unit NORMAL (the direction the surface faces, needed for lighting) and a
// colour. Triangles are listed COUNTER-CLOCKWISE as seen from the side their
// normal points to, OpenGL's default "front face", so back-face culling can
// skip the inside faces of closed boxes.

namespace editor {

struct Vertex {
    float px = 0.0f, py = 0.0f, pz = 0.0f; // position (world, cm)
    float nx = 0.0f, ny = 0.0f, nz = 0.0f; // unit normal
    float r = 0.0f, g = 0.0f, b = 0.0f;    // colour, 0..1
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices; // 3 per triangle, or 2 per line for line meshes

    void clear() {
        vertices.clear();
        indices.clear();
    }
};

xform::Vec3 toWorld(raster::Point plan, float height);

// ---- Triangle meshes -------------------------------------------------------------

// Adds one triangle, swapping two corners if needed so that it is
// counter-clockwise seen from the side `normal` points to. All three
// vertices get that normal (flat shading).
void addTriangle(MeshData& mesh, xform::Vec3 a, xform::Vec3 b, xform::Vec3 c, xform::Vec3 normal, raster::Color color);

// A horizontal polygon at height `y` (ear-clipped), facing up (normal +Y) or
// down. Used for floors, rugs, table tops and the flat 2D plan.
void addHorizontalPolygon(MeshData& mesh, const raster::Polygon& plan, float y, bool facingUp, raster::Color color);

// A prism: the plan polygon lifted from `bottom` to `top` - side walls with
// outward normals, a top cap and (optionally) a bottom cap.
void addPrism(MeshData& mesh, const raster::Polygon& plan, float bottom, float top, raster::Color color,
              bool bottomCap = false);

// An axis-aligned box in world coordinates.
void addBox(MeshData& mesh, xform::Vec3 minCorner, xform::Vec3 maxCorner, raster::Color color);

// The 3D model of an item: every part of its catalogue type, placed by the
// item's pose, with heights multiplied by heightScale.
void addItemModel(MeshData& mesh, const Item& item);

// The item's footprint only, flat at height y (the 2D plan look).
void addItemFlat(MeshData& mesh, const Item& item, float y);

// ---- Line meshes -----------------------------------------------------------------

void addLine(MeshData& mesh, xform::Vec3 a, xform::Vec3 b, raster::Color color);
void addPolygonOutline(MeshData& mesh, const raster::Polygon& plan, float y, raster::Color color);
// The floor grid: lines every `spacing` cm across the room, at height y.
void addGrid(MeshData& mesh, const Room& room, float spacing, float y, raster::Color color);
// Edges of every part of the item's 3D model (the selection outline in 3D),
// from scene::extrude.
void addItemEdges(MeshData& mesh, const Item& item, raster::Color color);

// ---- The room ----------------------------------------------------------------------

// One wall, as its own mesh so the renderer can hide it ("cutaway") when it
// stands between the camera and the room.
struct WallMesh {
    Wall wall = Wall::Top;
    xform::Vec3 outwardNormal; // horizontal, pointing out of the room
    xform::Vec3 centre;        // middle of the wall's inner face, at floor level
    MeshData full;             // the wall at full height, with its openings
    MeshData cut;              // the same wall cut down to a short stub
};

std::vector<WallMesh> buildWalls(const Room& room, float cutHeight = 25.0f);

// The floor (top face only).
void addFloor(MeshData& mesh, const Room& room);

// The model of one opening: for the door, the leaf standing open at 90
// degrees into the room, hinged at the right-hand end of the opening; for
// the window, the glass pane and the sill.
void addOpeningModel(MeshData& mesh, const Room& room, const Opening& opening);

// Both openings of the room.
void addDoorAndWindow(MeshData& mesh, const Room& room);

// The plan symbol of the door: the open leaf and its quarter-circle swing.
void addDoorSwing(MeshData& mesh, const Room& room, float y, raster::Color color);

} // namespace editor
