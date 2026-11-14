#include "anim/path.h"

#include "raster/curves.h"

#include <algorithm>
#include <cmath>

namespace anim {

namespace {

// Builds a cumulative arc-length table L[0..samples] for the Catmull-Rom
// spline through `points`, where L[k] is the polyline length from u = 0 to
// u = k / samples.
std::vector<double> buildArcLengthTable(const std::vector<raster::Point>& points, int samples) {
    samples = std::max(samples, 1);
    std::vector<double> lengths(static_cast<std::size_t>(samples) + 1, 0.0);
    if (points.size() < 2) {
        return lengths;
    }

    raster::Point prev = raster::evaluateCatmullRomSpline(points, 0.0f);
    double cumulative = 0.0;
    for (int k = 1; k <= samples; ++k) {
        const float u = static_cast<float>(k) / static_cast<float>(samples);
        const raster::Point curr = raster::evaluateCatmullRomSpline(points, u);
        const double dx = static_cast<double>(curr.x) - static_cast<double>(prev.x);
        const double dy = static_cast<double>(curr.y) - static_cast<double>(prev.y);
        cumulative += std::hypot(dx, dy);
        lengths[static_cast<std::size_t>(k)] = cumulative;
        prev = curr;
    }
    return lengths;
}

} // namespace

xform::Pose poseOnPath(const std::vector<raster::Point>& points, float u) {
    xform::Pose pose{};
    if (points.empty()) {
        return pose;
    }
    if (points.size() == 1) {
        pose.position = points[0];
        return pose;
    }

    const float clampedU = std::min(std::max(u, 0.0f), 1.0f);
    pose.position = raster::evaluateCatmullRomSpline(points, clampedU);

    // Estimate the tangent direction by stepping a small amount ahead along
    // the curve. Near the very end (clampedU + step > 1) we step slightly
    // backward from 1 so the interval [u0, u1] stays inside [0, 1].
    const float step = 1e-3f / static_cast<float>(points.size() - 1);
    float u0 = clampedU;
    float u1 = clampedU + step;
    if (u1 > 1.0f) {
        u1 = 1.0f;
        u0 = std::max(0.0f, 1.0f - step);
    }

    const raster::Point p0 = (u0 == clampedU) ? pose.position : raster::evaluateCatmullRomSpline(points, u0);
    const raster::Point p1 = (u1 == clampedU) ? pose.position : raster::evaluateCatmullRomSpline(points, u1);
    const double dx = static_cast<double>(p1.x) - static_cast<double>(p0.x);
    const double dy = static_cast<double>(p1.y) - static_cast<double>(p0.y);

    if (dx * dx + dy * dy > 1e-12) {
        // Screen coordinates: +x is right (0 deg) and +y is down (+90 deg),
        // so atan2(dy, dx) gives the clockwise angle in radians directly.
        pose.angleDegrees = static_cast<float>(std::atan2(dy, dx) * 180.0 / raster::kPi);
    }
    return pose;
}

float parameterForArcLength(const std::vector<raster::Point>& points, float s, int tableSamples) {
    const double clampedS = std::min(std::max(static_cast<double>(s), 0.0), 1.0);
    if (points.size() < 2) {
        return 0.0f;
    }
    if (clampedS <= 0.0) {
        return 0.0f;
    }
    if (clampedS >= 1.0) {
        return 1.0f;
    }

    const int samples = std::max(tableSamples, 16);
    const std::vector<double> table = buildArcLengthTable(points, samples);
    const double totalLength = table.back();
    if (totalLength <= 1e-9) {
        return static_cast<float>(clampedS);
    }

    const double targetDist = clampedS * totalLength;
    auto it = std::lower_bound(table.begin(), table.end(), targetDist);
    if (it == table.begin()) {
        return 0.0f;
    }
    if (it == table.end()) {
        return 1.0f;
    }

    const std::size_t idx = static_cast<std::size_t>(it - table.begin());
    const double d0 = table[idx - 1];
    const double d1 = table[idx];
    const double segLen = d1 - d0;
    const double frac = (segLen > 1e-12) ? (targetDist - d0) / segLen : 0.0;

    const double u = (static_cast<double>(idx - 1) + frac) / static_cast<double>(samples);
    return static_cast<float>(std::min(std::max(u, 0.0), 1.0));
}

float pathLength(const std::vector<raster::Point>& points, int tableSamples) {
    if (points.size() < 2) {
        return 0.0f;
    }
    const std::vector<double> table = buildArcLengthTable(points, std::max(tableSamples, 16));
    return static_cast<float>(table.back());
}

xform::Pose poseOnPathConstantSpeed(const std::vector<raster::Point>& points, float s, int tableSamples) {
    const float u = parameterForArcLength(points, s, tableSamples);
    return poseOnPath(points, u);
}

} // namespace anim

