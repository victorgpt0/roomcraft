#include "anim/track.h"

#include <algorithm>
#include <cmath>

namespace anim {

void PoseTrack::addKey(float timeSeconds, const xform::Pose& pose, Easing easingIntoKey) {
    const Keyframe key{timeSeconds, pose, easingIntoKey};

    // Keep keys sorted by ascending timeSeconds. If a key with the exact same
    // time already exists, overwrite it so we never get a zero-length interval
    // between two keys.
    auto it = std::lower_bound(
        keys_.begin(), keys_.end(), timeSeconds,
        [](const Keyframe& existing, float t) { return existing.timeSeconds < t; });

    if (it != keys_.end() && std::fabs(it->timeSeconds - timeSeconds) <= 1e-6f) {
        *it = key;
    } else {
        keys_.insert(it, key);
    }
}

xform::Pose PoseTrack::sample(float timeSeconds) const {
    if (keys_.empty()) {
        return xform::Pose{};
    }
    if (keys_.size() == 1 || timeSeconds <= keys_.front().timeSeconds) {
        return keys_.front().pose;
    }
    if (timeSeconds >= keys_.back().timeSeconds) {
        return keys_.back().pose;
    }

    // Find the first key with timeSeconds > query time; the active segment is
    // between the key just before it (k0) and that key (k1).
    auto upper = std::upper_bound(
        keys_.begin(), keys_.end(), timeSeconds,
        [](float t, const Keyframe& k) { return t < k.timeSeconds; });

    if (upper == keys_.begin()) {
        return keys_.front().pose;
    }
    if (upper == keys_.end()) {
        return keys_.back().pose;
    }

    const Keyframe& k0 = *(upper - 1);
    const Keyframe& k1 = *upper;

    if (std::fabs(timeSeconds - k0.timeSeconds) <= 1e-6f) {
        return k0.pose;
    }
    const float span = k1.timeSeconds - k0.timeSeconds;
    if (span <= 1e-6f) {
        return k1.pose;
    }

    const float localT = (timeSeconds - k0.timeSeconds) / span;
    const float easedT = ease(k1.easingIntoKey, localT);
    return lerpPose(k0.pose, k1.pose, easedT);
}

float PoseTrack::duration() const {
    if (keys_.empty()) {
        return 0.0f;
    }
    return std::max(0.0f, keys_.back().timeSeconds);
}

FixedTimestep::FixedTimestep(int fps, float duration)
    : framesPerSecond(fps > 0 ? fps : 20),
      durationSeconds(duration > 0.0f ? duration : 0.0f) {}

float FixedTimestep::dt() const {
    const int fps = std::max(framesPerSecond, 1);
    return 1.0f / static_cast<float>(fps);
}

int FixedTimestep::frameCount() const {
    const int fps = std::max(framesPerSecond, 1);
    const int count = static_cast<int>(std::lround(static_cast<double>(durationSeconds) * fps));
    return std::max(count, 1);
}

float FixedTimestep::frameTime(int frameIndex) const {
    if (frameIndex <= 0) {
        return 0.0f;
    }
    const float t = static_cast<float>(frameIndex) * dt();
    return std::min(t, std::max(0.0f, durationSeconds));
}

float FixedTimestep::progress(int frameIndex) const {
    const int count = frameCount();
    if (count <= 1 || frameIndex <= 0) {
        return 0.0f;
    }
    if (frameIndex >= count - 1) {
        return 1.0f;
    }
    return static_cast<float>(frameIndex) / static_cast<float>(count - 1);
}

} // namespace anim

