#include "editor/mesh.h"

#include "editor/triangulate.h"
#include "scene/extrude.h"
#include "xform/mat3.h"

#include <algorithm>
#include <cmath>

namespace editor {

namespace {

xform::Vec3 sub(xform::Vec3 a, xform::Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

xform::Vec3 crossProduct(xform::Vec3 a, xform::Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float dotProduct(xform::Vec3 a, xform::Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vertex makeVertex(xform::Vec3 p, xform::Vec3 n, raster::Color c) {
    Vertex v;
    v.px = p.x;
    v.py = p.y;
    v.pz = p.z;
    v.nx = n.x;
    v.ny = n.y;
    v.nz = n.z;
    v.r = static_cast<float>(c.r) / 255.0f;
    v.g = static_cast<float>(c.g) / 255.0f;
    v.b = static_cast<float>(c.b) / 255.0f;
    return v;
}

uint32_t push(MeshData& mesh, const Vertex& v) {
    mesh.vertices.push_back(v);
    return static_cast<uint32_t>(mesh.vertices.size() - 1);
}

} // namespace

xform::Vec3 toWorld(raster::Point plan, float height) {
    return {plan.x, height, plan.y};
}

// Winding rule. The cross product (b - a) x (c - a) points out of the side
// from which a -> b -> c is seen counter-clockwise (right-hand rule). If it
// points away from the wanted normal, swapping b and c reverses the winding.
void addTriangle(MeshData& mesh, xform::Vec3 a, xform::Vec3 b, xform::Vec3 c, xform::Vec3 normal, raster::Color color) {
    if (dotProduct(crossProduct(sub(b, a), sub(c, a)), normal) < 0.0f) {
        std::swap(b, c);
    }
    const uint32_t ia = push(mesh, makeVertex(a, normal, color));
    const uint32_t ib = push(mesh, makeVertex(b, normal, color));
    const uint32_t ic = push(mesh, makeVertex(c, normal, color));
    mesh.indices.insert(mesh.indices.end(), {ia, ib, ic});
}

void addHorizontalPolygon(MeshData& mesh, const raster::Polygon& plan, float y, bool facingUp, raster::Color color) {
    const xform::Vec3 normal{0.0f, facingUp ? 1.0f : -1.0f, 0.0f};
    const std::vector<Triangle> triangles = triangulate(plan);
    if (triangles.empty()) {
        return;
    }
    // Shared vertices for the whole cap (all have the same normal), then
    // indices per triangle with the winding fixed as in addTriangle.
    const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
    for (const raster::Point& p : plan.vertices) {
        mesh.vertices.push_back(makeVertex(toWorld(p, y), normal, color));
    }
    for (const Triangle& t : triangles) {
        const xform::Vec3 a = toWorld(plan.vertices[static_cast<std::size_t>(t[0])], y);
        const xform::Vec3 b = toWorld(plan.vertices[static_cast<std::size_t>(t[1])], y);
        const xform::Vec3 c = toWorld(plan.vertices[static_cast<std::size_t>(t[2])], y);
        uint32_t i0 = base + static_cast<uint32_t>(t[0]);
        uint32_t i1 = base + static_cast<uint32_t>(t[1]);
        uint32_t i2 = base + static_cast<uint32_t>(t[2]);
        if (dotProduct(crossProduct(sub(b, a), sub(c, a)), normal) < 0.0f) {
            std::swap(i1, i2);
        }
        mesh.indices.insert(mesh.indices.end(), {i0, i1, i2});
    }
}

// Sides: one rectangle (two triangles) per edge a -> b. Its outward normal
// is the edge direction turned by 90 degrees, (dy, -dx) on the plan, for a
// polygon whose signed area is positive (clockwise on screen, y down); the
// other winding needs the opposite turn. Plan y becomes world Z.
void addPrism(MeshData& mesh, const raster::Polygon& plan, float bottom, float top, raster::Color color,
              bool bottomCap) {
    const std::vector<raster::Point>& v = plan.vertices;
    if (v.size() < 3 || top <= bottom) {
        return;
    }
    const float sign = raster::computeSignedArea(plan) >= 0.0f ? 1.0f : -1.0f;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const raster::Point a = v[i];
        const raster::Point b = v[(i + 1) % v.size()];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length < 1e-6f) {
            continue;
        }
        const xform::Vec3 normal{sign * dy / length, 0.0f, sign * -dx / length};
        const xform::Vec3 a0 = toWorld(a, bottom), b0 = toWorld(b, bottom);
        const xform::Vec3 a1 = toWorld(a, top), b1 = toWorld(b, top);
        addTriangle(mesh, a0, b0, b1, normal, color);
        addTriangle(mesh, a0, b1, a1, normal, color);
    }
    addHorizontalPolygon(mesh, plan, top, true, color);
    if (bottomCap) {
        addHorizontalPolygon(mesh, plan, bottom, false, color);
    }
}

void addBox(MeshData& mesh, xform::Vec3 lo, xform::Vec3 hi, raster::Color color) {
    raster::Polygon plan = raster::makeRectangle(lo.x, lo.z, hi.x - lo.x, hi.z - lo.z);
    addPrism(mesh, plan, lo.y, hi.y, color, true);
}

void addItemModel(MeshData& mesh, const Item& item) {
    const xform::Mat3 place = xform::poseMatrix(item.pose);
    for (const Part& part : typeOf(item).parts) {
        const raster::Polygon plan = xform::transformPolygon(place, part.footprint);
        addPrism(mesh, plan, part.bottom * item.heightScale, part.top * item.heightScale,
                 partColor(part, item.color), part.bottom > 0.0f);
    }
}

void addItemFlat(MeshData& mesh, const Item& item, float y) {
    addHorizontalPolygon(mesh, itemFootprint(item), y, true, item.color);
}

void addLine(MeshData& mesh, xform::Vec3 a, xform::Vec3 b, raster::Color color) {
    const xform::Vec3 none{0.0f, 0.0f, 0.0f};
    const uint32_t ia = push(mesh, makeVertex(a, none, color));
    const uint32_t ib = push(mesh, makeVertex(b, none, color));
    mesh.indices.insert(mesh.indices.end(), {ia, ib});
}

void addPolygonOutline(MeshData& mesh, const raster::Polygon& plan, float y, raster::Color color) {
    const std::vector<raster::Point>& v = plan.vertices;
    for (std::size_t i = 0; i < v.size(); ++i) {
        addLine(mesh, toWorld(v[i], y), toWorld(v[(i + 1) % v.size()], y), color);
    }
}

void addGrid(MeshData& mesh, const Room& room, float spacing, float y, raster::Color color) {
    if (spacing <= 0.0f) {
        return;
    }
    for (float x = spacing; x < room.width - 0.5f; x += spacing) {
        addLine(mesh, {x, y, 0.0f}, {x, y, room.depth}, color);
    }
    for (float z = spacing; z < room.depth - 0.5f; z += spacing) {
        addLine(mesh, {0.0f, y, z}, {room.width, y, z}, color);
    }
}

void addItemEdges(MeshData& mesh, const Item& item, raster::Color color) {
    const xform::Mat3 place = xform::poseMatrix(item.pose);
    for (const Part& part : typeOf(item).parts) {
        // scene::extrude builds the prism from the floor; lift it to the
        // part's bottom afterwards.
        const float bottom = part.bottom * item.heightScale;
        const float height = (part.top - part.bottom) * item.heightScale;
        const xform::WireMesh wire = scene::extrude(xform::transformPolygon(place, part.footprint), height);
        for (const auto& edge : wire.edges) {
            xform::Vec3 a = wire.vertices[static_cast<std::size_t>(edge.first)];
            xform::Vec3 b = wire.vertices[static_cast<std::size_t>(edge.second)];
            a.y += bottom;
            b.y += bottom;
            addLine(mesh, a, b, color);
        }
    }
}

// ---- The room ----------------------------------------------------------------------

namespace {

// Splits [0, length] into the solid pieces between the openings of one wall.
// Each opening leaves a gap from `start` to `end`, filled below `bottom` and
// above `top`.
struct Span {
    float from, to, bottom, top;
};

std::vector<Span> wallSpans(const Room& room, Wall wall, float length, float height) {
    std::vector<Opening> gaps;
    for (const Opening& o : openings(room)) {
        if (o.wall == wall) {
            gaps.push_back(o);
        }
    }
    std::sort(gaps.begin(), gaps.end(), [](const Opening& a, const Opening& b) { return a.start < b.start; });

    std::vector<Span> spans;
    float cursor = 0.0f;
    for (const Opening& o : gaps) {
        const float start = std::clamp(o.start, 0.0f, length);
        const float end = std::clamp(o.end, start, length);
        if (start > cursor) {
            spans.push_back({cursor, start, 0.0f, height});
        }
        if (o.bottom > 0.0f) {
            spans.push_back({start, end, 0.0f, std::min(o.bottom, height)});
        }
        if (o.top < height) {
            spans.push_back({start, end, o.top, height});
        }
        cursor = end;
    }
    if (cursor < length) {
        spans.push_back({cursor, length, 0.0f, height});
    }
    return spans;
}

} // namespace

std::vector<WallMesh> buildWalls(const Room& room, float cutHeight) {
    const float w = room.width;
    const float d = room.depth;
    const float t = room.wallThickness;
    std::vector<WallMesh> walls;

    // For each wall: where "along" runs and how a span maps to a world box.
    // The top and bottom walls also cover the corners (x from -t to w + t).
    struct Spec {
        Wall wall;
        xform::Vec3 normal;
        xform::Vec3 centre;
        float length;
    };
    const Spec specs[4] = {
        {Wall::Top, {0, 0, -1}, {w / 2, 0, 0}, w},
        {Wall::Right, {1, 0, 0}, {w, 0, d / 2}, d},
        {Wall::Bottom, {0, 0, 1}, {w / 2, 0, d}, w},
        {Wall::Left, {-1, 0, 0}, {0, 0, d / 2}, d},
    };

    for (const Spec& spec : specs) {
        WallMesh mesh;
        mesh.wall = spec.wall;
        mesh.outwardNormal = spec.normal;
        mesh.centre = spec.centre;
        for (int version = 0; version < 2; ++version) {
            const float height = version == 0 ? room.wallHeight : std::min(cutHeight, room.wallHeight);
            MeshData& target = version == 0 ? mesh.full : mesh.cut;
            for (const Span& s : wallSpans(room, spec.wall, spec.length, height)) {
                if (s.top <= s.bottom) {
                    continue;
                }
                // Extend the first and last spans of the top and bottom walls
                // over the corners.
                float from = s.from;
                float to = s.to;
                const bool horizontal = spec.wall == Wall::Top || spec.wall == Wall::Bottom;
                if (horizontal && from <= 0.0f) {
                    from = -t;
                }
                if (horizontal && to >= spec.length) {
                    to = spec.length + t;
                }
                xform::Vec3 lo{}, hi{};
                switch (spec.wall) {
                case Wall::Top:
                    lo = {from, s.bottom, -t};
                    hi = {to, s.top, 0.0f};
                    break;
                case Wall::Bottom:
                    lo = {from, s.bottom, d};
                    hi = {to, s.top, d + t};
                    break;
                case Wall::Left:
                    lo = {-t, s.bottom, from};
                    hi = {0.0f, s.top, to};
                    break;
                case Wall::Right:
                    lo = {w, s.bottom, from};
                    hi = {w + t, s.top, to};
                    break;
                }
                addBox(target, lo, hi, room.wallColor);
            }
        }
        walls.push_back(mesh);
    }
    return walls;
}

void addFloor(MeshData& mesh, const Room& room) {
    addHorizontalPolygon(mesh, roomOutline(room), 0.0f, true, room.floorColor);
}

void addDoorAndWindow(MeshData& mesh, const Room& room) {
    for (const Opening& o : openings(room)) {
        addOpeningModel(mesh, room, o);
    }
}

void addOpeningModel(MeshData& mesh, const Room& room, const Opening& o) {
    const raster::Color doorColor{160, 118, 84};
    const raster::Color glass{170, 205, 230};
    const raster::Color frame{250, 250, 250};
    {
        if (o.isDoor) {
            // Hinged at the right end of the opening, swung 90 degrees into
            // the room (towards -Z): a 4 cm slab along the hinge side.
            const float hinge = o.end;
            const float width = o.end - o.start;
            addBox(mesh, {hinge - 4.0f, 0.0f, room.depth - width}, {hinge, o.top, room.depth}, doorColor);
        } else {
            // Glass pane in the middle of the wall's thickness, and a sill.
            const float t = room.wallThickness;
            addBox(mesh, {o.start, o.bottom, -t / 2 - 1.0f}, {o.end, o.top, -t / 2 + 1.0f}, glass);
            addBox(mesh, {o.start - 4.0f, o.bottom - 3.0f, -t}, {o.end + 4.0f, o.bottom, 4.0f}, frame);
        }
    }
}

void addDoorSwing(MeshData& mesh, const Room& room, float y, raster::Color color) {
    for (const Opening& o : openings(room)) {
        if (!o.isDoor) {
            continue;
        }
        const float width = o.end - o.start;
        const raster::Point hinge{o.end, room.depth};
        // The open leaf, pointing into the room.
        addLine(mesh, toWorld(hinge, y), toWorld({hinge.x, hinge.y - width}, y), color);
        // The swing: a quarter circle from the open leaf to the closed
        // position along the wall, centred on the hinge.
        constexpr int kSegments = 24;
        raster::Point previous{hinge.x, hinge.y - width};
        for (int i = 1; i <= kSegments; ++i) {
            const double angle = (raster::kPi / 2.0) * i / kSegments; // 0 = open, pi/2 = closed
            const raster::Point p{hinge.x - static_cast<float>(width * std::sin(angle)),
                                  hinge.y - static_cast<float>(width * std::cos(angle))};
            addLine(mesh, toWorld(previous, y), toWorld(p, y), color);
            previous = p;
        }
    }
}

} // namespace editor
