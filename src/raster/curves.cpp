#include "raster/curves.h"

#include "raster/line.h"

#include <algorithm>
#include <cmath>

namespace raster {

namespace {

// Linear interpolation between a and b: a at t = 0, b at t = 1.
// Written as (1 - t) a + t b (not a + t (b - a)) so that t = 1 gives
// exactly b, with no rounding error.
Point lerp(Point a, Point b, double t) {
    return Point{static_cast<float>((1.0 - t) * a.x + t * b.x),
                 static_cast<float>((1.0 - t) * a.y + t * b.y)};
}

} // namespace

// Bernstein form. The four weights
//   b0 = (1-t)^3,  b1 = 3 (1-t)^2 t,  b2 = 3 (1-t) t^2,  b3 = t^3
// are the terms of ((1-t) + t)^3 = 1, so they always add up to 1 and are
// all >= 0 for t in [0, 1]: every point is a weighted average of the
// control points (so the curve stays inside their convex hull).
// At t = 0 only b0 is 1 (curve starts at p0); at t = 1 only b3 is 1.
Point evaluateBezierCubic(Point p0, Point p1, Point p2, Point p3, float t) {
    const double u = 1.0 - t;
    const double b0 = u * u * u;
    const double b1 = 3.0 * u * u * t;
    const double b2 = 3.0 * u * t * t;
    const double b3 = static_cast<double>(t) * t * t;
    return Point{static_cast<float>(b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x),
                 static_cast<float>(b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y)};
}

// de Casteljau: interpolate along each edge of the control polygon at the
// same t, then along the edges of the resulting shorter polygon, until one
// point is left:
//   level 1:  a = lerp(p0,p1)  b = lerp(p1,p2)  c = lerp(p2,p3)
//   level 2:  d = lerp(a,b)    e = lerp(b,c)
//   level 3:  point = lerp(d,e)
// Multiplying out the lerps gives exactly the Bernstein formula, which is
// why both functions return the same point. de Casteljau only uses
// interpolation, which makes it numerically very stable; the points d and e
// also give the tangent direction (e - d) at t.
Point evaluateBezierDeCasteljau(Point p0, Point p1, Point p2, Point p3, float t) {
    const Point a = lerp(p0, p1, t);
    const Point b = lerp(p1, p2, t);
    const Point c = lerp(p2, p3, t);
    const Point d = lerp(a, b, t);
    const Point e = lerp(b, c, t);
    return lerp(d, e, t);
}

std::vector<Point> sampleBezierCubic(Point p0, Point p1, Point p2, Point p3, int segments) {
    segments = std::max(segments, 1);
    std::vector<Point> samples;
    samples.reserve(static_cast<size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        samples.push_back(evaluateBezierCubic(p0, p1, p2, p3, t));
    }
    return samples;
}

void drawBezierCurve(Canvas& canvas, Point p0, Point p1, Point p2, Point p3, Color color, int segments) {
    drawPolyline(canvas, sampleBezierCubic(p0, p1, p2, p3, segments), color);
}

// Uniform Catmull-Rom segment between p1 and p2:
//   C(t) = 1/2 * ( 2 p1
//                + (-p0 + p2) t
//                + (2 p0 - 5 p1 + 4 p2 - p3) t^2
//                + (-p0 + 3 p1 - 3 p2 + p3) t^3 )
// This cubic is chosen so that
//   C(0) = p1,  C(1) = p2                    (passes through the points)
//   C'(0) = (p2 - p0) / 2,  C'(1) = (p3 - p1) / 2
// i.e. the direction at each point is parallel to the line joining its two
// neighbours. Neighbouring segments therefore meet at the same point with
// the same tangent, so the whole spline is smooth (C1 continuous).
Point catmullRom(Point p0, Point p1, Point p2, Point p3, float t) {
    const double t2 = static_cast<double>(t) * t;
    const double t3 = t2 * t;
    auto axis = [&](double v0, double v1, double v2, double v3) {
        return 0.5 * (2.0 * v1 +
                      (-v0 + v2) * t +
                      (2.0 * v0 - 5.0 * v1 + 4.0 * v2 - v3) * t2 +
                      (-v0 + 3.0 * v1 - 3.0 * v2 + v3) * t3);
    };
    return Point{static_cast<float>(axis(p0.x, p1.x, p2.x, p3.x)),
                 static_cast<float>(axis(p0.y, p1.y, p2.y, p3.y))};
}

namespace {

// The four points segment i (from points[i] to points[i+1]) needs, with
// the end points duplicated where a neighbour is missing.
void segmentPoints(const std::vector<Point>& points, size_t i,
                   Point& p0, Point& p1, Point& p2, Point& p3) {
    const size_t last = points.size() - 1;
    p0 = points[i == 0 ? 0 : i - 1];
    p1 = points[i];
    p2 = points[i + 1];
    p3 = points[std::min(i + 2, last)];
}

} // namespace

std::vector<Point> catmullRomSpline(const std::vector<Point>& points, int samplesPerSegment) {
    if (points.size() < 2) {
        return points;
    }
    samplesPerSegment = std::max(samplesPerSegment, 1);

    std::vector<Point> samples;
    samples.reserve((points.size() - 1) * static_cast<size_t>(samplesPerSegment) + 1);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        Point p0, p1, p2, p3;
        segmentPoints(points, i, p0, p1, p2, p3);
        // Each segment adds its samples for t in [0, 1); the next segment
        // starts with the shared point, so it is not added twice.
        for (int s = 0; s < samplesPerSegment; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(samplesPerSegment);
            samples.push_back(catmullRom(p0, p1, p2, p3, t));
        }
    }
    samples.push_back(points.back()); // t = 1 of the last segment, exactly
    return samples;
}

Point evaluateCatmullRomSpline(const std::vector<Point>& controlPoints, float t) {
    if (controlPoints.empty()) {
        return Point{};
    }
    if (controlPoints.size() == 1) {
        return controlPoints[0];
    }
    const double clamped = std::min(std::max(static_cast<double>(t), 0.0), 1.0);
    const size_t segments = controlPoints.size() - 1;

    // Which segment, and how far along it: t * segments = i + local.
    const double scaled = clamped * static_cast<double>(segments);
    size_t i = static_cast<size_t>(std::floor(scaled));
    if (i >= segments) {
        return controlPoints.back(); // t == 1
    }
    const double local = scaled - static_cast<double>(i);

    Point p0, p1, p2, p3;
    segmentPoints(controlPoints, i, p0, p1, p2, p3);
    return catmullRom(p0, p1, p2, p3, static_cast<float>(local));
}

void drawCatmullRomSpline(Canvas& canvas, const std::vector<Point>& points, int samplesPerSegment, Color color) {
    drawPolyline(canvas, catmullRomSpline(points, samplesPerSegment), color);
}

// Uniform cubic B-spline. Each segment is shaped by 4 consecutive control
// points with the weights
//   w0 = (1-t)^3 / 6
//   w1 = (3t^3 - 6t^2 + 4) / 6
//   w2 = (-3t^3 + 3t^2 + 3t + 1) / 6
//   w3 = t^3 / 6
// The weights are >= 0 and add up to 1, so the curve stays inside the
// convex hull of those 4 points. Moving to the next segment drops one
// control point and adds the next, and the weights are built so that the
// position, first and second derivative all match where segments meet
// (C2 continuous: smoother than Catmull-Rom). The price is that the curve
// does not pass through the control points: at t = 0 it is at
// (p0 + 4 p1 + p2) / 6, a weighted average near p1.
std::vector<Point> uniformBSpline(const std::vector<Point>& controlPoints, int samplesPerSegment) {
    if (controlPoints.size() < 4) {
        return {};
    }
    samplesPerSegment = std::max(samplesPerSegment, 1);
    const size_t segments = controlPoints.size() - 3;

    std::vector<Point> samples;
    samples.reserve(segments * static_cast<size_t>(samplesPerSegment) + 1);
    for (size_t i = 0; i < segments; ++i) {
        const Point& p0 = controlPoints[i];
        const Point& p1 = controlPoints[i + 1];
        const Point& p2 = controlPoints[i + 2];
        const Point& p3 = controlPoints[i + 3];
        const int count = (i + 1 == segments) ? samplesPerSegment + 1 : samplesPerSegment; // last includes t = 1
        for (int s = 0; s < count; ++s) {
            const double t = static_cast<double>(s) / samplesPerSegment;
            const double u = 1.0 - t;
            const double w0 = u * u * u / 6.0;
            const double w1 = (3.0 * t * t * t - 6.0 * t * t + 4.0) / 6.0;
            const double w2 = (-3.0 * t * t * t + 3.0 * t * t + 3.0 * t + 1.0) / 6.0;
            const double w3 = t * t * t / 6.0;
            samples.push_back(Point{static_cast<float>(w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x),
                                    static_cast<float>(w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y)});
        }
    }
    return samples;
}

} // namespace raster
