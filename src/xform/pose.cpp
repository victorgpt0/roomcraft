#include "xform/pose.h"

namespace xform {

// Why this order (scale, then rotate, then move)?
//
// - Scale first, while the shape is still axis-aligned and centred on its
//   own origin: scaleX then always means "wider along the piece's own
//   width", however the piece is turned afterwards. Scaling AFTER rotating
//   would stretch along the room's axes and skew a turned piece.
// - Rotate second, still about the local origin, so the piece turns on the
//   spot. Rotating AFTER moving would swing it around the room's origin
//   (the top-left corner of the canvas) instead.
// - Move last, so `position` is exactly where the local origin ends up.
Mat3 poseMatrix(const Pose& pose) {
    return Mat3::translation(pose.position.x, pose.position.y) *
           Mat3::rotation(pose.angleDegrees) *
           Mat3::scaling(pose.scaleX, pose.scaleY);
}

} // namespace xform
