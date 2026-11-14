#include "anim/easing.h"

#include <algorithm>
#include <cmath>

namespace anim {

// Easing curves map t in [0, 1] to [0, 1]:
//
// - Linear:    f(t) = t
//   Constant derivative f'(t) = 1. Useful as a baseline, but looks robotic
//   because velocity jumps instantaneously at t = 0 and t = 1.
//
// - EaseIn:    f(t) = t^2
//   Quadratic ease-in. f(0) = 0, f(1) = 1, f'(0) = 0, f'(1) = 2.
//   Starts slowly from rest (f(t) < t for t in (0, 1)) and speeds up.
//
// - EaseOut:   f(t) = 1 - (1 - t)^2 = t * (2 - t)
//   Time-reversed quadratic. f(0) = 0, f(1) = 1, f'(0) = 2, f'(1) = 0.
//   Starts fast and glides smoothly to a stop (f(t) > t for t in (0, 1)).
//
// - EaseInOut: f(t) = 3 t^2 - 2 t^3 = t^2 * (3 - 2 t)
//   Hermite smoothstep cubic. Chosen to satisfy four boundary conditions:
//     f(0) = 0,  f(1) = 1,  f'(0) = 0,  f'(1) = 0
//   Symmetric about the midpoint: f(0.5) = 0.5 and f(1 - t) = 1 - f(t).
float ease(Easing easing, float t) {
    const double u = std::min(std::max(static_cast<double>(t), 0.0), 1.0);
    switch (easing) {
        case Easing::Linear:
            return static_cast<float>(u);
        case Easing::EaseIn:
            return static_cast<float>(u * u);
        case Easing::EaseOut: {
            const double inv = 1.0 - u;
            return static_cast<float>(1.0 - inv * inv);
        }
        case Easing::EaseInOut:
            return static_cast<float>(u * u * (3.0 - 2.0 * u));
    }
    return static_cast<float>(u);
}

// Written in the two-weight form (1 - t) * a + t * b so that t = 0 returns `a`
// and t = 1 returns `b` without floating-point drift when a and b differ a lot.
float lerp(float a, float b, float t) {
    const double u = static_cast<double>(t);
    return static_cast<float>((1.0 - u) * static_cast<double>(a) + u * static_cast<double>(b));
}

// Shortest-arc angle interpolation in degrees.
//
// Why plain lerp fails on angles:
//   350 degrees and 10 degrees are only 20 degrees apart across the 360/0
//   seam, but lerp(350, 10, 0.5) gives 180 - spinning the furniture 340
//   degrees the wrong way round.
//
// Fix: wrap the signed difference (b - a) into (-180, 180] before multiplying
// by t, then wrap the interpolated angle into [0, 360).
float lerpAngle(float aDegrees, float bDegrees, float t) {
    double diff = std::fmod(static_cast<double>(bDegrees) - static_cast<double>(aDegrees), 360.0);
    if (diff > 180.0) {
        diff -= 360.0;
    } else if (diff <= -180.0) {
        diff += 360.0;
    }

    double result = static_cast<double>(aDegrees) + static_cast<double>(t) * diff;
    result = std::fmod(result, 360.0);
    if (result < 0.0) {
        result += 360.0;
    }
    // Guard against -0.0 or a value rounding to 360.0.
    if (result >= 360.0 || std::fabs(result) < 1e-9) {
        result = 0.0;
    }
    return static_cast<float>(result);
}

// Interpolates the four components of a Pose instead of interpolating the 3x3
// matrix entries directly (interpolating two rotation matrices averages their
// cosines and sines, which shrinks the shape in the middle of a turn).
xform::Pose lerpPose(const xform::Pose& a, const xform::Pose& b, float t) {
    xform::Pose out;
    out.position.x = lerp(a.position.x, b.position.x, t);
    out.position.y = lerp(a.position.y, b.position.y, t);
    out.angleDegrees = lerpAngle(a.angleDegrees, b.angleDegrees, t);
    out.scaleX = lerp(a.scaleX, b.scaleX, t);
    out.scaleY = lerp(a.scaleY, b.scaleY, t);
    return out;
}

} // namespace anim

