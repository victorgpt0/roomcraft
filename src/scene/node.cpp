#include "scene/node.h"

#include "raster/fill.h"
#include "raster/line.h"
#include "xform/pose.h"

namespace scene {

namespace {

// A dark outline, matching roomcraft::drawFurniture in Checkpoint 1.
const raster::Color kOutline{40, 40, 40};

// Recursive helper for worldMatrix(). `parent` is the world matrix of the
// node's parent; the node's own pose is multiplied on the right so it is
// applied first, in the parent's local frame.
bool findWorldMatrix(const Node& node, const xform::Mat3& parent,
                     const std::string& name, xform::Mat3& out) {
    const xform::Mat3 world = parent * xform::poseMatrix(node.pose);
    if (node.name == name) {
        out = world;
        return true;
    }
    for (const Node& child : node.children) {
        if (findWorldMatrix(child, world, name, out)) {
            return true;
        }
    }
    return false;
}

} // namespace

void drawNode(raster::Canvas& canvas, const Node& node, const xform::Mat3& parent) {
    // Push: the child frame is parent * this node's pose.
    const xform::Mat3 world = parent * xform::poseMatrix(node.pose);

    // Pure groups (for example the fan root) have no shape of their own, but
    // still contribute a transform to their children.
    if (!node.shape.vertices.empty()) {
        const raster::Polygon placed = xform::transformPolygon(world, node.shape);
        raster::fillPolygonScanline(canvas, placed, node.color);
        raster::drawPolygonOutline(canvas, placed, kOutline);
    }

    // Draw children in order; later children are drawn on top. Each call gets
    // `world` as its parent matrix, and returns without changing it, which is
    // the pop half of the stack.
    for (const Node& child : node.children) {
        drawNode(canvas, child, world);
    }
}

xform::Mat3 worldMatrix(const Node& root, const std::string& name, bool* found) {
    xform::Mat3 result = xform::Mat3::identity();
    const bool ok = findWorldMatrix(root, xform::Mat3::identity(), name, result);
    if (found != nullptr) {
        *found = ok;
    }
    return result;
}

} // namespace scene
