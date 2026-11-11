#pragma once

#include "raster/types.h"
#include "xform/mat3.h"

// Pose: where a piece of furniture is, as three plain numbers-with-meaning
// instead of a matrix.
//
// In Checkpoint 1 a transform was applied by rewriting the polygon's
// vertices. That is fine for one move, but for animation it has two
// problems: rotating a little on every frame piles up rounding errors until
// the shape is visibly distorted, and once the vertices are overwritten we
// no longer know "how far is it turned?".
//
// So from Checkpoint 2 a piece keeps its shape in LOCAL coordinates, which
// never change, plus a Pose. The matrix is rebuilt from the pose whenever it
// is needed. Animating a piece is then just changing these numbers over time.

namespace xform {

struct Pose {
    raster::Point position{0.0f, 0.0f}; // where the local origin ends up in the room
    float angleDegrees = 0.0f;          // clockwise on screen
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

// The matrix that places a local shape in the room:
//   translation(position) * rotation(angleDegrees) * scaling(scaleX, scaleY)
// Read right to left: the shape is scaled, then rotated, then moved.
// The default Pose gives the identity matrix.
Mat3 poseMatrix(const Pose& pose);

} // namespace xform
