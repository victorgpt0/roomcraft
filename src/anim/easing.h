#pragma once

#include "xform/pose.h"

// Easing curves and linear interpolation for numbers, angles and poses.
//
// In keyframe animation, motion between two poses is separated into two
// independent questions:
//   1. "How far through the step are we?" - answered by an EASING CURVE that
//      reshapes the raw time fraction t in [0, 1] into a smooth progress
//      value u = ease(type, t) in [0, 1].
//   2. "Where is the object at progress u?" - answered by LINEAR INTERPOLATION
//      (lerp, lerpAngle, lerpPose) between the start and end values.
//
// RoomCraft use: sliding, turning and scaling furniture smoothly between
// keyframes instead of moving at a mechanical, constant speed with abrupt
// starts and stops.

namespace anim {

enum class Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

// Reshapes a normalised time fraction t into a smooth progress value in [0, 1].
// Values of t outside [0, 1] are clamped to [0, 1] first.
//
// Formulas (for clamped t in [0, 1]):
//   Linear:    f(t) = t                  (constant speed)
//   EaseIn:    f(t) = t^2                (starts from rest, accelerates)
//   EaseOut:   f(t) = 1 - (1 - t)^2      (starts fast, decelerates to rest)
//   EaseInOut: f(t) = 3 t^2 - 2 t^3      (smoothstep: accelerates then decelerates)
float ease(Easing easing, float t);

// Linear interpolation between `a` (at t = 0) and `b` (at t = 1):
//   lerp(a, b, t) = (1 - t) * a + t * b
float lerp(float a, float b, float t);

// Linear interpolation between two angles in degrees, always taking the
// shorter way round the circle (so 350 -> 10 degrees turns +20 degrees
// through 0, not -340 degrees back through 180). Result is wrapped to [0, 360).
float lerpAngle(float aDegrees, float bDegrees, float t);

// Interpolates every component of a Pose: position.x, position.y, scaleX and
// scaleY with lerp(), and angleDegrees with lerpAngle().
xform::Pose lerpPose(const xform::Pose& a, const xform::Pose& b, float t);

} // namespace anim

