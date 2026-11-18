// Tests for the CPU mesh builders: counts, normals, winding, positions.

#include "editor/catalogue.h"
#include "editor/layout.h"
#include "editor/mesh.h"
#include "editor/triangulate.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

struct V3 {
    double x, y, z;
};

V3 position(const editor::MeshData& m, uint32_t i) {
    const editor::Vertex& v = m.vertices[i];
    return {v.px, v.py, v.pz};
}

// Checks every triangle of a triangle mesh:
//   - indices in range;
//   - every vertex normal has unit length;
//   - the triangle is counter-clockwise seen from its normal's side
//     (geometric normal agrees with the vertex normal) - the front face rule
//     back-face culling relies on.
// Returns the total area.
double checkTriangles(const editor::MeshData& mesh, const std::string& name) {
    CHECK_MSG(mesh.indices.size() % 3 == 0, name);
    int bad = 0;
    double area = 0.0;
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t i0 = mesh.indices[t], i1 = mesh.indices[t + 1], i2 = mesh.indices[t + 2];
        if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size()) {
            ++bad;
            continue;
        }
        const V3 a = position(mesh, i0), b = position(mesh, i1), c = position(mesh, i2);
        const V3 e1{b.x - a.x, b.y - a.y, b.z - a.z};
        const V3 e2{c.x - a.x, c.y - a.y, c.z - a.z};
        const V3 g{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        const double length = std::sqrt(g.x * g.x + g.y * g.y + g.z * g.z);
        area += length / 2.0;
        for (const uint32_t i : {i0, i1, i2}) {
            const editor::Vertex& v = mesh.vertices[i];
            const double n = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (std::fabs(n - 1.0) > 1e-4) {
                ++bad;
            }
            if (length > 1e-9 && (g.x * v.nx + g.y * v.ny + g.z * v.nz) / length < 0.999) {
                ++bad; // wrong winding or a normal that is not the face's
            }
        }
    }
    CHECK_MSG(bad == 0, name + ": " + std::to_string(bad) + " problems");
    return area;
}

void testPrism() {
    // A 100 x 50 box from 10 to 40 cm: 4 sides (2 triangles each) + top
    // (2) + bottom (2) = 12 triangles; surface area known.
    editor::MeshData mesh;
    editor::addPrism(mesh, raster::makeRectangle(0, 0, 100, 50), 10, 40, {200, 100, 50}, true);
    CHECK(mesh.indices.size() == 12 * 3);
    const double area = checkTriangles(mesh, "box");
    CHECK_NEAR(area, 2.0 * (100 * 50) + 2.0 * (100 * 30) + 2.0 * (50 * 30), 1e-2);

    // Plan y becomes world Z, height becomes world Y.
    double minY = 1e9, maxY = -1e9, maxZ = -1e9;
    for (const editor::Vertex& v : mesh.vertices) {
        minY = std::min(minY, static_cast<double>(v.py));
        maxY = std::max(maxY, static_cast<double>(v.py));
        maxZ = std::max(maxZ, static_cast<double>(v.pz));
    }
    CHECK_NEAR(minY, 10, 0);
    CHECK_NEAR(maxY, 40, 0);
    CHECK_NEAR(maxZ, 50, 0);
    // Colour is 0..1.
    CHECK_NEAR(mesh.vertices[0].r, 200.0 / 255.0, 1e-6);

    // Side normals point AWAY from the centre (outward), for both windings
    // of the polygon.
    for (const bool reverse : {false, true}) {
        raster::Polygon l;
        l.vertices = {{0, 0}, {100, 0}, {100, 100}, {60, 100}, {60, 40}, {0, 40}};
        if (reverse) {
            std::reverse(l.vertices.begin(), l.vertices.end());
        }
        editor::MeshData prism;
        editor::addPrism(prism, l, 0, 50, {1, 1, 1}, false);
        // 6 sides x 2 + top (6 - 2) = 16 triangles.
        CHECK(prism.indices.size() == 16 * 3);
        checkTriangles(prism, reverse ? "L reversed" : "L");
        int inward = 0;
        for (std::size_t t = 0; t < prism.indices.size(); t += 3) {
            const editor::Vertex& v = prism.vertices[prism.indices[t]];
            if (v.ny != 0.0f) {
                CHECK(v.ny > 0.0f); // top cap faces up
                continue;
            }
            // Face centre nudged along the normal must be OUTSIDE the L.
            double cx = 0, cz = 0;
            for (int k = 0; k < 3; ++k) {
                cx += prism.vertices[prism.indices[t + static_cast<std::size_t>(k)]].px / 3.0;
                cz += prism.vertices[prism.indices[t + static_cast<std::size_t>(k)]].pz / 3.0;
            }
            const raster::Point outside{static_cast<float>(cx + v.nx * 0.5), static_cast<float>(cz + v.nz * 0.5)};
            const raster::Point inside{static_cast<float>(cx - v.nx * 0.5), static_cast<float>(cz - v.nz * 0.5)};
            if (raster::pointInPolygon(l, outside) || !raster::pointInPolygon(l, inside)) {
                ++inward;
            }
        }
        CHECK(inward == 0);
    }

    // Degenerate input adds nothing.
    editor::MeshData empty;
    editor::addPrism(empty, raster::makeRectangle(0, 0, 10, 10), 5, 5, {1, 1, 1});
    editor::addPrism(empty, raster::Polygon{}, 0, 5, {1, 1, 1});
    CHECK(empty.vertices.empty() && empty.indices.empty());
}

void testHorizontalPolygon() {
    raster::Polygon kidney = editor::findType("coffee_table")->footprint;
    for (const bool up : {true, false}) {
        editor::MeshData mesh;
        editor::addHorizontalPolygon(mesh, kidney, 42, up, {1, 2, 3});
        CHECK(mesh.vertices.size() == kidney.vertices.size()); // shared vertices
        CHECK(mesh.indices.size() == 3 * (kidney.vertices.size() - 2));
        const double area = checkTriangles(mesh, up ? "kidney up" : "kidney down");
        CHECK_NEAR(area, raster::computeArea(kidney), raster::computeArea(kidney) * 1e-4);
        for (const editor::Vertex& v : mesh.vertices) {
            CHECK(v.py == 42.0f && v.ny == (up ? 1.0f : -1.0f));
        }
    }
}

void testItemModels() {
    // Every catalogue type, turned and scaled, builds a clean mesh whose
    // top reaches the type's height (times heightScale) and whose floor
    // shadow stays within the item's footprint bounding box.
    for (const editor::FurnitureType& type : editor::catalogue()) {
        editor::Item item = editor::makeItem(type.id, {300, 200}, 1);
        item.pose.angleDegrees = 33;
        item.heightScale = 1.5f;
        editor::MeshData mesh;
        editor::addItemModel(mesh, item);
        CHECK_MSG(!mesh.indices.empty(), type.id);
        checkTriangles(mesh, type.id);

        double maxY = 0;
        const raster::Rect box = raster::computeBoundingBox(editor::itemFootprint(item));
        int outside = 0;
        for (const editor::Vertex& v : mesh.vertices) {
            maxY = std::max(maxY, static_cast<double>(v.py));
            if (v.px < box.xmin - 0.01f || v.px > box.xmax + 0.01f || v.pz < box.ymin - 0.01f || v.pz > box.ymax + 0.01f) {
                ++outside;
            }
        }
        CHECK_MSG(outside == 0, type.id + " model outside its footprint");
        CHECK_MSG(std::fabs(maxY - type.height * 1.5) < 0.01, type.id + " height");

        // The flat (2D plan) version is the footprint at one height.
        editor::MeshData flat;
        editor::addItemFlat(flat, item, 1.0f);
        const double area = checkTriangles(flat, type.id + " flat");
        CHECK_NEAR(area, raster::computeArea(editor::itemFootprint(item)), area * 1e-3 + 0.1);

        // Selection edges: 3 edges per footprint vertex per part.
        editor::MeshData edges;
        editor::addItemEdges(edges, item, {255, 140, 0});
        std::size_t expected = 0;
        for (const editor::Part& part : type.parts) {
            expected += 3 * part.footprint.vertices.size();
        }
        CHECK_MSG(edges.indices.size() == 2 * expected, type.id + " edges");
    }
}

void testRoom() {
    editor::Room room;
    const std::vector<editor::WallMesh> walls = editor::buildWalls(room);
    CHECK(walls.size() == 4);
    for (const editor::WallMesh& wall : walls) {
        checkTriangles(wall.full, "wall");
        checkTriangles(wall.cut, "cut wall");
        double maxFull = 0, maxCut = 0;
        for (const editor::Vertex& v : wall.full.vertices) {
            maxFull = std::max(maxFull, static_cast<double>(v.py));
        }
        for (const editor::Vertex& v : wall.cut.vertices) {
            maxCut = std::max(maxCut, static_cast<double>(v.py));
        }
        CHECK_NEAR(maxFull, room.wallHeight, 1e-3);
        CHECK_NEAR(maxCut, 25.0, 1e-3);
        // Outward normal is horizontal and unit length, and the wall's
        // vertices all lie on the outer side of the room boundary.
        const xform::Vec3 n = wall.outwardNormal;
        CHECK(n.y == 0.0f && std::fabs(n.x * n.x + n.z * n.z - 1.0f) < 1e-6f);
        int inside = 0;
        for (const editor::Vertex& v : wall.full.vertices) {
            const double along = (v.px - wall.centre.x) * n.x + (v.pz - wall.centre.z) * n.z;
            if (along < -1e-3) {
                ++inside;
            }
        }
        CHECK(inside == 0);
    }

    // The door and window leave holes: a point in the middle of each
    // opening is not inside any wall box. Approximated by checking that no
    // wall triangle of the bottom wall covers the door's middle at 100 cm
    // height (the door is 210 tall): the wall's top at the door's x must be
    // above 210 (the lintel), and the full wall has more triangles than a
    // plain box.
    const editor::Opening door = editor::openings(room)[0];
    CHECK(door.isDoor);
    const editor::WallMesh& bottom = walls[2];
    CHECK(bottom.wall == editor::Wall::Bottom);
    CHECK(bottom.full.indices.size() > 12 * 3);
    int blocking = 0;
    const float doorMid = (door.start + door.end) / 2.0f;
    for (std::size_t t = 0; t < bottom.full.indices.size(); t += 3) {
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (int k = 0; k < 3; ++k) {
            const editor::Vertex& v = bottom.full.vertices[bottom.full.indices[t + static_cast<std::size_t>(k)]];
            minX = std::min(minX, static_cast<double>(v.px));
            maxX = std::max(maxX, static_cast<double>(v.px));
            minY = std::min(minY, static_cast<double>(v.py));
            maxY = std::max(maxY, static_cast<double>(v.py));
        }
        if (minX < doorMid && maxX > doorMid && minY < 100 && maxY > 100) {
            ++blocking;
        }
    }
    CHECK(blocking == 0);

    editor::MeshData floor;
    editor::addFloor(floor, room);
    CHECK_NEAR(checkTriangles(floor, "floor"), room.width * room.depth, 1e-1);

    editor::MeshData doorAndWindow;
    editor::addDoorAndWindow(doorAndWindow, room);
    checkTriangles(doorAndWindow, "door and window");

    editor::MeshData grid;
    editor::addGrid(grid, room, 50, 0.1f, {1, 1, 1});
    // 560 / 50 -> lines at 50..550 (11), 420 / 50 -> 50..400 (8).
    CHECK(grid.indices.size() == 2 * (11 + 8));

    editor::MeshData swing;
    editor::addDoorSwing(swing, room, 0.5f, {1, 1, 1});
    CHECK(swing.indices.size() == 2 * (1 + 24));
    // Every swing point is one door width from the hinge.
    const double hingeX = door.end, hingeZ = room.depth, width = door.end - door.start;
    for (const editor::Vertex& v : swing.vertices) {
        const double r = std::hypot(v.px - hingeX, v.pz - hingeZ);
        CHECK(std::fabs(r - width) < 1e-3 || r < 1e-3);
    }
}

} // namespace

int main() {
    testPrism();
    testHorizontalPolygon();
    testItemModels();
    testRoom();
    return test::summary("test_mesh");
}
