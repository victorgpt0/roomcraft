// Tests for the Checkpoint 2 animation engine (anim::ease, lerp, lerpAngle,
// lerpPose, PoseTrack, FixedTimestep, poseOnPath and poseOnPathConstantSpeed).

#include "anim/easing.h"
#include "anim/path.h"
#include "anim/track.h"
#include "raster/curves.h"
#include "raster/types.h"
#include "xform/pose.h"

#include "test_helpers.h"

#include <cmath>
#include <vector>

using anim::Easing;
using anim::FixedTimestep;
using anim::PoseTrack;
using raster::Point;
using xform::Pose;

namespace {

void checkPoseNear(const Pose& actual, const Pose& expected, double tolerance) {
    CHECK_NEAR(actual.position.x, expected.position.x, tolerance);
    CHECK_NEAR(actual.position.y, expected.position.y, tolerance);
    CHECK_NEAR(actual.angleDegrees, expected.angleDegrees, tolerance);
    CHECK_NEAR(actual.scaleX, expected.scaleX, tolerance);
    CHECK_NEAR(actual.scaleY, expected.scaleY, tolerance);
}

void testEasing() {
    const Easing allEasings[] = {
        Easing::Linear,
        Easing::EaseIn,
        Easing::EaseOut,
        Easing::EaseInOut,
    };

    for (Easing e : allEasings) {
        // Clamping outside [0, 1] and exact endpoints at 0 and 1.
        CHECK_NEAR(anim::ease(e, -2.5f), 0.0, 1e-6);
        CHECK_NEAR(anim::ease(e, -0.01f), 0.0, 1e-6);
        CHECK_NEAR(anim::ease(e, 0.0f), 0.0, 1e-6);
        CHECK_NEAR(anim::ease(e, 1.0f), 1.0, 1e-6);
        CHECK_NEAR(anim::ease(e, 1.01f), 1.0, 1e-6);
        CHECK_NEAR(anim::ease(e, 5.0f), 1.0, 1e-6);

        // Monotonicity: never decreases on [0, 1], and stays within [0, 1].
        float prev = anim::ease(e, 0.0f);
        for (int i = 1; i <= 200; ++i) {
            const float t = static_cast<float>(i) / 200.0f;
            const float curr = anim::ease(e, t);
            CHECK(curr >= 0.0f && curr <= 1.0f);
            CHECK(curr >= prev - 1e-6f);
            prev = curr;
        }
    }

    // Symmetry and midpoint properties.
    CHECK_NEAR(anim::ease(Easing::Linear, 0.5f), 0.5, 1e-6);
    CHECK_NEAR(anim::ease(Easing::EaseInOut, 0.5f), 0.5, 1e-6);
    CHECK_NEAR(anim::ease(Easing::EaseIn, 0.5f), 0.25, 1e-6);
    CHECK_NEAR(anim::ease(Easing::EaseOut, 0.5f), 0.75, 1e-6);

    // Smoothstep symmetry: ease(EaseInOut, t) + ease(EaseInOut, 1 - t) == 1.
    // EaseIn / EaseOut duality: ease(EaseOut, t) == 1 - ease(EaseIn, 1 - t).
    for (int i = 1; i < 100; ++i) {
        const float t = static_cast<float>(i) / 100.0f;
        CHECK_NEAR(anim::ease(Easing::EaseInOut, t) + anim::ease(Easing::EaseInOut, 1.0f - t), 1.0, 1e-5);
        CHECK_NEAR(anim::ease(Easing::EaseOut, t), 1.0f - anim::ease(Easing::EaseIn, 1.0f - t), 1e-5);
    }

    // Speed at the ends:
    // - EaseIn starts slower than Linear (for small t, ease(EaseIn, t) < ease(Linear, t)).
    // - EaseOut ends slower than Linear (near t = 1, the remaining distance
    //   1 - ease(EaseOut, t) is smaller than 1 - ease(Linear, t), i.e. the step
    //   covered over [1 - dt, 1] is smaller for EaseOut than for Linear).
    for (float t : {0.05f, 0.1f, 0.2f, 0.25f}) {
        CHECK(anim::ease(Easing::EaseIn, t) < anim::ease(Easing::Linear, t));
        const float stepOut = anim::ease(Easing::EaseOut, 1.0f) - anim::ease(Easing::EaseOut, 1.0f - t);
        const float stepLin = anim::ease(Easing::Linear, 1.0f) - anim::ease(Easing::Linear, 1.0f - t);
        CHECK(stepOut < stepLin);
    }
}

void testLerpAndAngles() {
    // Plain scalar lerp endpoints, midpoint and quarter points.
    CHECK_NEAR(anim::lerp(10.0f, 50.0f, 0.0f), 10.0, 1e-6);
    CHECK_NEAR(anim::lerp(10.0f, 50.0f, 1.0f), 50.0, 1e-6);
    CHECK_NEAR(anim::lerp(10.0f, 50.0f, 0.5f), 30.0, 1e-6);
    CHECK_NEAR(anim::lerp(-20.0f, 20.0f, 0.25f), -10.0, 1e-6);
    CHECK_NEAR(anim::lerp(100.0f, -100.0f, 0.75f), -50.0, 1e-6);

    // Ordinary angle interpolation without seam crossing.
    CHECK_NEAR(anim::lerpAngle(20.0f, 80.0f, 0.0f), 20.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(20.0f, 80.0f, 0.5f), 50.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(20.0f, 80.0f, 1.0f), 80.0, 1e-5);

    // Crossing the 360/0 seam forward: 350 -> 10 degrees (+20 deg turn).
    CHECK_NEAR(anim::lerpAngle(350.0f, 10.0f, 0.0f), 350.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(350.0f, 10.0f, 0.25f), 355.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(350.0f, 10.0f, 0.5f), 0.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(350.0f, 10.0f, 0.75f), 5.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(350.0f, 10.0f, 1.0f), 10.0, 1e-5);

    // Crossing the 360/0 seam backward: 10 -> 350 degrees (-20 deg turn).
    CHECK_NEAR(anim::lerpAngle(10.0f, 350.0f, 0.0f), 10.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(10.0f, 350.0f, 0.25f), 5.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(10.0f, 350.0f, 0.5f), 0.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(10.0f, 350.0f, 0.75f), 355.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(10.0f, 350.0f, 1.0f), 350.0, 1e-5);

    // Negative and multi-turn input angles still take the shortest arc.
    CHECK_NEAR(anim::lerpAngle(-10.0f, 10.0f, 0.5f), 0.0, 1e-5);
    CHECK_NEAR(anim::lerpAngle(710.0f, 10.0f, 0.5f), 0.0, 1e-5);

    // Full Pose interpolation.
    const Pose a{{100.0f, 200.0f}, 350.0f, 1.0f, 2.0f};
    const Pose b{{300.0f, 100.0f}, 10.0f, 2.0f, 1.0f};
    checkPoseNear(anim::lerpPose(a, b, 0.0f), a, 1e-5);
    checkPoseNear(anim::lerpPose(a, b, 1.0f), b, 1e-5);
    checkPoseNear(anim::lerpPose(a, b, 0.5f), Pose{{200.0f, 150.0f}, 0.0f, 1.5f, 1.5f}, 1e-5);
}

void testPoseTrack() {
    // Empty track returns the default identity pose and has 0 duration.
    PoseTrack empty;
    CHECK_NEAR(empty.duration(), 0.0, 1e-6);
    checkPoseNear(empty.sample(0.0f), Pose{}, 1e-6);
    checkPoseNear(empty.sample(5.0f), Pose{}, 1e-6);

    // Single-key track always returns that key's pose.
    PoseTrack single;
    const Pose onlyKey{{120.0f, 80.0f}, 45.0f, 1.2f, 0.8f};
    single.addKey(1.5f, onlyKey);
    CHECK_NEAR(single.duration(), 1.5, 1e-6);
    checkPoseNear(single.sample(-1.0f), onlyKey, 1e-6);
    checkPoseNear(single.sample(1.5f), onlyKey, 1e-6);
    checkPoseNear(single.sample(10.0f), onlyKey, 1e-6);

    // Multi-key track with keys added OUT OF ORDER.
    PoseTrack track;
    const Pose k0{{100.0f, 100.0f}, 0.0f, 1.0f, 1.0f};
    const Pose k1{{300.0f, 200.0f}, 90.0f, 2.0f, 1.5f};
    const Pose k2{{500.0f, 400.0f}, 180.0f, 1.0f, 0.5f};

    track.addKey(2.0f, k1, Easing::Linear);
    track.addKey(4.0f, k2, Easing::EaseInOut);
    track.addKey(0.0f, k0, Easing::Linear);

    CHECK(track.keyCount() == 3);
    CHECK_NEAR(track.duration(), 4.0, 1e-6);

    // Clamping before the first key and after the last key.
    checkPoseNear(track.sample(-5.0f), k0, 1e-5);
    checkPoseNear(track.sample(-0.001f), k0, 1e-5);
    checkPoseNear(track.sample(4.001f), k2, 1e-5);
    checkPoseNear(track.sample(99.0f), k2, 1e-5);

    // Exact match at each keyframe time.
    checkPoseNear(track.sample(0.0f), k0, 1e-5);
    checkPoseNear(track.sample(2.0f), k1, 1e-5);
    checkPoseNear(track.sample(4.0f), k2, 1e-5);

    // Midpoint between two Linear keys (t = 1.0s is halfway between 0.0s and 2.0s).
    checkPoseNear(track.sample(1.0f), Pose{{200.0f, 150.0f}, 45.0f, 1.5f, 1.25f}, 1e-5);

    // Quarter-way into the second segment (t = 2.5s, local fraction u = 0.25)
    // uses k2's EaseInOut curve: ease(EaseInOut, 0.25) = 3*(1/16) - 2*(1/64) = 5/32 = 0.15625,
    // which is slower than Linear (0.25) at the start of the segment.
    const Pose quarterEase = track.sample(2.5f);
    CHECK_NEAR(quarterEase.position.x, 300.0 + 0.15625 * (500.0 - 300.0), 1e-4);
    CHECK_NEAR(quarterEase.position.y, 200.0 + 0.15625 * (400.0 - 200.0), 1e-4);

    // Halfway into an EaseInOut segment (t = 3.0s, local fraction u = 0.5),
    // ease(EaseInOut, 0.5) == 0.5, so it passes through the exact midpoint pose.
    checkPoseNear(track.sample(3.0f), Pose{{400.0f, 300.0f}, 135.0f, 1.5f, 1.0f}, 1e-5);
}

void testFixedTimestep() {
    const FixedTimestep step(20, 3.0f);
    CHECK(step.frameCount() == 60);
    CHECK_NEAR(step.dt(), 0.05, 1e-6);
    CHECK_NEAR(step.frameTime(0), 0.0, 1e-6);
    CHECK_NEAR(step.frameTime(1), 0.05, 1e-6);
    CHECK_NEAR(step.frameTime(20), 1.0, 1e-6);
    CHECK_NEAR(step.frameTime(59), 2.95, 1e-6);
    CHECK_NEAR(step.progress(0), 0.0, 1e-6);
    CHECK_NEAR(step.progress(59), 1.0, 1e-6);
}

void testPoseOnPath() {
    // Empty and single-point paths do not crash.
    checkPoseNear(anim::poseOnPath({}, 0.5f), Pose{}, 1e-6);
    checkPoseNear(anim::poseOnPath({{42.0f, 99.0f}}, 0.5f), Pose{{42.0f, 99.0f}, 0.0f, 1.0f, 1.0f}, 1e-6);

    // Path going straight right (+x): heading must be 0 degrees everywhere.
    const std::vector<Point> rightPath = {{50.0f, 100.0f}, {150.0f, 100.0f}, {350.0f, 100.0f}};
    const Pose rightStart = anim::poseOnPath(rightPath, 0.0f);
    const Pose rightMid = anim::poseOnPath(rightPath, 0.5f);
    const Pose rightEnd = anim::poseOnPath(rightPath, 1.0f);
    CHECK_NEAR(rightStart.position.x, 50.0, 1e-4);
    CHECK_NEAR(rightStart.position.y, 100.0, 1e-4);
    CHECK_NEAR(rightEnd.position.x, 350.0, 1e-4);
    CHECK_NEAR(rightEnd.position.y, 100.0, 1e-4);
    CHECK_NEAR(rightStart.angleDegrees, 0.0, 1e-3);
    CHECK_NEAR(rightMid.angleDegrees, 0.0, 1e-3);
    CHECK_NEAR(rightEnd.angleDegrees, 0.0, 1e-3);

    // Path going straight down (+y, screen coordinates): heading must be +90 degrees.
    const std::vector<Point> downPath = {{200.0f, 40.0f}, {200.0f, 180.0f}, {200.0f, 360.0f}};
    const Pose downStart = anim::poseOnPath(downPath, 0.0f);
    const Pose downMid = anim::poseOnPath(downPath, 0.5f);
    const Pose downEnd = anim::poseOnPath(downPath, 1.0f);
    CHECK_NEAR(downStart.position.x, 200.0, 1e-4);
    CHECK_NEAR(downStart.position.y, 40.0, 1e-4);
    CHECK_NEAR(downEnd.position.x, 200.0, 1e-4);
    CHECK_NEAR(downEnd.position.y, 360.0, 1e-4);
    CHECK_NEAR(downStart.angleDegrees, 90.0, 1e-3);
    CHECK_NEAR(downMid.angleDegrees, 90.0, 1e-3);
    CHECK_NEAR(downEnd.angleDegrees, 90.0, 1e-3);

    // Curved path with deliberately uneven control-point spacing:
    // segment 0 is short (~40 cm), segment 1 is long (~300 cm), segment 2 bends
    // sharply across the room.
    const std::vector<Point> curvedPath = {
        {80.0f, 80.0f},
        {120.0f, 80.0f},
        {420.0f, 120.0f},
        {500.0f, 340.0f},
        {160.0f, 380.0f},
    };

    // Both poseOnPath and poseOnPathConstantSpeed pass through the first and
    // last points (and every interior control point for poseOnPath at k / (N - 1)).
    for (std::size_t k = 0; k < curvedPath.size(); ++k) {
        const float u = static_cast<float>(k) / static_cast<float>(curvedPath.size() - 1);
        const Pose p = anim::poseOnPath(curvedPath, u);
        CHECK_NEAR(p.position.x, curvedPath[k].x, 1e-4);
        CHECK_NEAR(p.position.y, curvedPath[k].y, 1e-4);
    }
    const Pose constStart = anim::poseOnPathConstantSpeed(curvedPath, 0.0f);
    const Pose constEnd = anim::poseOnPathConstantSpeed(curvedPath, 1.0f);
    CHECK_NEAR(constStart.position.x, curvedPath.front().x, 1e-4);
    CHECK_NEAR(constStart.position.y, curvedPath.front().y, 1e-4);
    CHECK_NEAR(constEnd.position.x, curvedPath.back().x, 1e-4);
    CHECK_NEAR(constEnd.position.y, curvedPath.back().y, 1e-4);

    // Measure arc length covered in N equal time steps under plain u versus
    // constant-speed arc-length reparameterisation.
    constexpr int kSteps = 20;
    const double totalLen = anim::pathLength(curvedPath, 1024);
    const double expectedStepLen = totalLen / kSteps;

    // Measure each step's true curve distance by integrating 50 sub-steps.
    double minRawStep = 1e9;
    double maxRawStep = 0.0;
    for (int i = 0; i < kSteps; ++i) {
        const float u0 = static_cast<float>(i) / kSteps;
        const float u1 = static_cast<float>(i + 1) / kSteps;
        double rawDist = 0.0;
        Point prev = anim::poseOnPath(curvedPath, u0).position;
        for (int sub = 1; sub <= 50; ++sub) {
            const float u = anim::lerp(u0, u1, static_cast<float>(sub) / 50.0f);
            const Point curr = anim::poseOnPath(curvedPath, u).position;
            rawDist += std::hypot(curr.x - prev.x, curr.y - prev.y);
            prev = curr;
        }
        minRawStep = std::min(minRawStep, rawDist);
        maxRawStep = std::max(maxRawStep, rawDist);

        // Constant-speed version: distance between s0 and s1 along the curve.
        const float s0 = static_cast<float>(i) / kSteps;
        const float s1 = static_cast<float>(i + 1) / kSteps;
        double constDist = 0.0;
        Point prevConst = anim::poseOnPathConstantSpeed(curvedPath, s0, 1024).position;
        for (int sub = 1; sub <= 50; ++sub) {
            const float s = anim::lerp(s0, s1, static_cast<float>(sub) / 50.0f);
            const Point currConst = anim::poseOnPathConstantSpeed(curvedPath, s, 1024).position;
            constDist += std::hypot(currConst.x - prevConst.x, currConst.y - prevConst.y);
            prevConst = currConst;
        }
        CHECK_NEAR(constDist, expectedStepLen, 0.5);
    }
    // Plain u has wildly uneven step lengths (more than a 4x gap between slowest
    // and fastest step on this path), proving why reparameterisation is needed.
    CHECK(maxRawStep > 4.0 * minRawStep);
}

} // namespace

int main() {
    testEasing();
    testLerpAndAngles();
    testPoseTrack();
    testFixedTimestep();
    testPoseOnPath();
    return test::summary("test_anim");
}

