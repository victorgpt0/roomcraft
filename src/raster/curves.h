#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

#include <vector>

// Curves: cubic Bezier, Catmull-Rom spline and uniform cubic B-spline.
//
// All curves are evaluated mathematically and drawn by sampling them into
// short straight pieces (a polyline) that are rasterised with Bresenham.
// More samples give a smoother curve. Coordinates are screen coordinates
// (origin top-left, y down - see types.h).
//
// RoomCraft use: curved furniture such as a kidney-shaped coffee table or a
// curved sofa back (Bezier), smooth paths through points the user clicks
// (Catmull-Rom, which passes through every point), and smooth rounded
// outlines that only need to follow the points loosely (B-spline).

namespace raster {

// ---- Cubic Bezier --------------------------------------------------------
// The curve starts at p0 (t = 0), ends at p3 (t = 1) and is pulled towards
// the control points p1 and p2 without passing through them.

// Bernstein (polynomial) form:
//   B(t) = (1-t)^3 p0 + 3(1-t)^2 t p1 + 3(1-t) t^2 p2 + t^3 p3
Point evaluateBezierCubic(Point p0, Point p1, Point p2, Point p3, float t);

// The same curve, computed with de Casteljau's algorithm (repeated linear
// interpolation). Gives the same point as evaluateBezierCubic.
Point evaluateBezierDeCasteljau(Point p0, Point p1, Point p2, Point p3, float t);

// segments + 1 points on the curve at t = 0, 1/segments, ..., 1.
// segments < 1 is treated as 1 (a straight line from p0 to p3).
std::vector<Point> sampleBezierCubic(Point p0, Point p1, Point p2, Point p3, int segments);

// Draws the curve as a polyline of `segments` straight pieces.
void drawBezierCurve(Canvas& canvas, Point p0, Point p1, Point p2, Point p3, Color color, int segments);

// ---- Catmull-Rom spline --------------------------------------------------
// An interpolating spline: it passes through every given point.

// One uniform Catmull-Rom segment. It runs from p1 (t = 0) to p2 (t = 1);
// p0 and p3 are the neighbouring points that set the direction of the curve
// at p1 and p2. t outside [0, 1] extrapolates.
Point catmullRom(Point p0, Point p1, Point p2, Point p3, float t);

// Smooth curve through ALL points, as (N - 1) * samplesPerSegment + 1
// sample points. Segment i goes from points[i] to points[i + 1]. The first
// and last points have no outer neighbour, so they are duplicated
// (the first segment uses points[0] twice, the last uses points[N-1] twice);
// that way the curve still starts and ends exactly on them.
// samplesPerSegment < 1 is treated as 1. Returns the input unchanged if it
// has fewer than 2 points.
std::vector<Point> catmullRomSpline(const std::vector<Point>& points, int samplesPerSegment);

// Point on the whole spline above, with t in [0, 1] spread evenly over the
// segments: t = k / (N - 1) is exactly points[k]. t is clamped to [0, 1].
// Returns {0, 0} for an empty input and the single point for one point.
Point evaluateCatmullRomSpline(const std::vector<Point>& controlPoints, float t);

// Draws catmullRomSpline(points, samplesPerSegment) as a polyline.
void drawCatmullRomSpline(Canvas& canvas, const std::vector<Point>& points, int samplesPerSegment, Color color);

// ---- Uniform cubic B-spline (approximating) ------------------------------
// A smoother curve that generally does NOT pass through its control points;
// it follows them loosely and stays inside their convex hull. N control
// points give N - 3 segments. Returns (N - 3) * samplesPerSegment + 1
// points, or an empty vector if there are fewer than 4 control points.
std::vector<Point> uniformBSpline(const std::vector<Point>& controlPoints, int samplesPerSegment);

} // namespace raster
