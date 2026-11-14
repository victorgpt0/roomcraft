#pragma once

#include "anim/easing.h"
#include "xform/pose.h"

#include <cstddef>
#include <vector>

// Keyframe animation tracks and fixed-timestep frame timing.
//
// Instead of writing a custom formula for every frame of an animation, an
// animator specifies only the important moments - KEYFRAMES - each with a
// timestamp, a target Pose, and the easing curve used to arrive at that key.
// PoseTrack sorts the keys by time and fills in all the frames in between.
//
// FixedTimestep converts a duration in seconds and a frame rate (frames per
// second) into a discrete frame count and the exact time of each frame, so
// every animation advances time by the same constant delta t = 1 / fps.

namespace anim {

// Keyframed pose over time. sample() clamps before the first and after the last key.
class PoseTrack {
public:
    struct Keyframe {
        float timeSeconds = 0.0f;
        xform::Pose pose{};
        Easing easingIntoKey = Easing::EaseInOut;
    };

    // Inserts a keyframe at `timeSeconds` and keeps the track sorted by time.
    // If a key already exists at the exact same timestamp, it is replaced.
    // `easingIntoKey` shapes the transition FROM the previous key INTO this one.
    void addKey(float timeSeconds, const xform::Pose& pose, Easing easingIntoKey = Easing::EaseInOut);

    // Evaluates the track at `timeSeconds`:
    //   - empty track: returns the default identity Pose;
    //   - before or at the first key: returns the first key's Pose;
    //   - at or after the last key: returns the last key's Pose;
    //   - between keys[i] and keys[i + 1]: maps timeSeconds to a local
    //     fraction u in [0, 1], applies keys[i + 1].easingIntoKey, and
    //     interpolates with lerpPose().
    xform::Pose sample(float timeSeconds) const;

    // End time of the track in seconds (0 if empty).
    float duration() const;

    std::size_t keyCount() const { return keys_.size(); }
    const std::vector<Keyframe>& keys() const { return keys_; }

private:
    std::vector<Keyframe> keys_;
};

// Fixed-timestep helper: given frames per second and a duration in seconds,
// computes the total frame count and the simulation time of each frame.
//
// Why a fixed timestep?
//   If an animation advanced by the wall-clock time since the previous frame,
//   a slow frame would cause a large jump and the GIF would play at uneven
//   speed depending on how busy the computer was when rendering it. With a
//   fixed timestep dt = 1 / fps, frame i always represents the exact moment
//   t_i = i * dt, making the animation 100% deterministic and reproducible.
struct FixedTimestep {
    int framesPerSecond = 20;
    float durationSeconds = 1.0f;

    FixedTimestep() = default;
    FixedTimestep(int fps, float duration);

    // Constant time step between consecutive frames: 1 / framesPerSecond.
    float dt() const;

    // Total number of frames in the animation (at least 1):
    // round(durationSeconds * framesPerSecond).
    int frameCount() const;

    // Simulation time in seconds of `frameIndex` (0, dt, 2*dt, ...), clamped
    // to [0, durationSeconds].
    float frameTime(int frameIndex) const;

    // Normalised progress in [0, 1] across the whole animation, so frame 0
    // is 0.0 and frame (frameCount() - 1) is 1.0.
    float progress(int frameIndex) const;
};

} // namespace anim

