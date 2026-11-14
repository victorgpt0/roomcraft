#pragma once

#include "raster/types.h"
#include "xform/pose.h"

#include <vector>

// Motion along a Catmull-Rom spline path, with heading from the tangent and
// arc-length reparameterisation for constant speed.
//
// Why plain spline parameter u gives uneven speed
// -----------------------------------------------
// raster::evaluateCatmullRomSpline(points, u) splits u in [0, 1] evenly across
// the N - 1 segments: each segment gets a parameter slice of 1 / (N - 1), no
// matter how short or long that segment is in centimetres. If points[0..1] are
// 40 cm apart and points[1..2] are 240 cm apart, both segments take the same
// time, so an object moving at constant du/dt crawls along the first segment
// and races six times faster along the second. Even within a single segment,
// the cubic polynomial's derivative |C'(t)| varies between the middle and the
// ends whenever the control points are unevenly spaced or turn a corner.
//
// How the arc-length table fixes this
// -----------------------------------
// We sample the spline at M evenly spaced parameters u_k = k / M, measure the
// straight-line distance of each small step, and build a cumulative table of
// arc lengths L[0..M]. To place an object at fraction s in [0, 1] of the total
// distance, we look up target distance d = s * L[M] in the table with binary
// search (std::lower_bound), linearly interpolate between the two surrounding
// table entries to recover the spline parameter u(s), and evaluate poseOnPath
// there. Equal steps in s now cover equal centimetres on the floor plan.

namespace anim {

// Position on a Catmull-Rom path through `points` at u in [0, 1], facing along the path.
// Heading is measured in degrees with the RoomCraft screen convention:
//   0 degrees = right (+x), 90 degrees = down (+y), 180 = left, -90 = up.
xform::Pose poseOnPath(const std::vector<raster::Point>& points, float u);

// Maps a normalised arc-length fraction s in [0, 1] to the spline parameter u
// in [0, 1] using a cumulative arc-length table of `tableSamples` chords.
float parameterForArcLength(const std::vector<raster::Point>& points, float s,
                            int tableSamples = 512);

// Total length of the Catmull-Rom spline through `points`, approximated by
// summing `tableSamples` straight chords along the curve.
float pathLength(const std::vector<raster::Point>& points, int tableSamples = 512);

// Position and heading on the Catmull-Rom path at fraction s in [0, 1] of the
// total arc length, so that advancing s linearly in time produces constant
// speed along the path.
xform::Pose poseOnPathConstantSpeed(const std::vector<raster::Point>& points, float s,
                                    int tableSamples = 512);

} // namespace anim

