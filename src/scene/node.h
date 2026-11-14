#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"
#include "xform/mat3.h"
#include "xform/pose.h"

#include <string>
#include <vector>

// A hierarchical model: one part of a piece of furniture, or a whole room.
//
// Why a tree?
// -----------
// A chair is not one polygon. It has a base, a seat, a backrest and armrests.
// When the chair moves, every part moves with it. When the seat swivels, its
// backrest and armrests move with the seat but the base stays put. Writing
// that as a list of "absolute" positions would mean recomputing every part
// by hand on every frame.
//
// Instead each Node stores its SHAPE in its own LOCAL coordinates, and a
// POSE relative to its PARENT. The world matrix of a node is the product of
// every pose from the root down to that node:
//
//     world = rootPose * childPose * grandchildPose * ...
//
// A child therefore always follows its parent for free. This is the same
// idea as a scene graph in a game engine or Blender's outliner.
//
// RoomCraft use: articulated furniture (fan with blades, swivel chair,
// hinged door) and rooms whose furniture must follow when the room moves.

namespace scene {

// One part of a hierarchical model. `pose` is relative to the PARENT.
struct Node {
    std::string name;
    raster::Polygon shape;       // local coordinates; may be empty (pure group)
    raster::Color color;
    xform::Pose pose;
    std::vector<Node> children;
};

// Draws the node and its children. world = parent * poseMatrix(node.pose).
//
// The recursion is a MATRIX STACK:
//   - on the way DOWN we multiply the incoming parent matrix by this node's
//     pose, producing the world matrix for this node;
//   - that world matrix is passed to every child, so it acts as the new top
//     of the stack (the "push");
//   - because `parent` is a const reference and `world` is a local value, the
//     parent matrix is still untouched when the child call returns. The
//     caller then loops to the next child with the original parent matrix
//     (the "pop").
// Nothing is ever mutated, so the same Node tree can be drawn again and again
// from a different root matrix without rebuilding it.
void drawNode(raster::Canvas& canvas, const Node& node, const xform::Mat3& parent);

// Finds a node by name and returns its world matrix (the full chain from the
// root, including the root's own pose). If the name is not in the tree,
// *found (when supplied) is set to false and the identity matrix is returned.
// This lets other code ask "where did the fan blade end up?" without walking
// the tree itself, which is useful for labels, hit testing and animation.
xform::Mat3 worldMatrix(const Node& root, const std::string& name, bool* found = nullptr);

} // namespace scene
