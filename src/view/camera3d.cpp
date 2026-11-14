#include "view/camera3d.h"

#include "raster/clipping.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "view/camera2d.h"

#include <cmath>
#include <utility>
#include <vector>

namespace view {

namespace {

void cosSin(float angleDegrees, float& c, float& s) {
    const double radians = angleDegrees * raster::kPi / 180.0;
    c = static_cast<float>(std::cos(radians));
    s = static_cast<float>(std::sin(radians));
}

xform::Vec4 lerp(const xform::Vec4& a, const xform::Vec4& b, float t) {
    return xform::Vec4{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                       a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

xform::Vec4 toClip(const Mat4& viewProjection, xform::Vec3 p) {
    return viewProjection * xform::Vec4{p.x, p.y, p.z, 1.0f};
}

// The perspective divide followed by the viewport mapping.
raster::Point toPixel(const xform::Mat3& ndcToPixels, const xform::Vec4& clip) {
    return xform::transformPoint(ndcToPixels, raster::Point{clip.x / clip.w, clip.y / clip.w});
}

} // namespace

// Spherical coordinates
// ---------------------
// Start with the offset (0, 0, radius): straight out along +Z. Tilting it UP
// by the elevation (a rotation about X) gives (0, r sin(el), r cos(el)): the
// height is r sin(el) and the distance along the floor is r cos(el).
// Turning that round the vertical axis by the azimuth (a rotation about Y)
// splits the floor distance between Z and X:
//
//   offset = ( r cos(el) sin(az),  r sin(el),  r cos(el) cos(az) )
//
// i.e. offset = rotationY(az) * rotationX(-el) * (0, 0, r). Its length is
// always r, because cos^2 + sin^2 = 1 twice.
xform::Vec3 eyePosition(const OrbitCamera& camera) {
    float cosEl = 1.0f, sinEl = 0.0f, cosAz = 1.0f, sinAz = 0.0f;
    cosSin(camera.elevationDegrees, cosEl, sinEl);
    cosSin(camera.azimuthDegrees, cosAz, sinAz);
    const xform::Vec3 offset{camera.radius * cosEl * sinAz, camera.radius * sinEl,
                             camera.radius * cosEl * cosAz};
    return add(camera.target, offset);
}

// The derivative of the unit offset with respect to the elevation:
//   d/d(el) (cos el sin az, sin el, cos el cos az) = (-sin el sin az, cos el, -sin el cos az)
//
// Why not simply (0, 1, 0)? lookAt needs `up` NOT parallel to the viewing
// direction. At elevation 90 the camera looks straight down, exactly along
// -Y, so cross(forward, (0,1,0)) = 0 and the camera basis falls apart. This
// tangent is perpendicular to the offset at every angle. For elevations
// below 90 it gives the same picture as (0, 1, 0) would (lookAt only uses
// the part of `up` perpendicular to the view direction, and that part points
// the same way), so nothing changes except that the top view now works.
xform::Vec3 upVector(const OrbitCamera& camera) {
    float cosEl = 1.0f, sinEl = 0.0f, cosAz = 1.0f, sinAz = 0.0f;
    cosSin(camera.elevationDegrees, cosEl, sinEl);
    cosSin(camera.azimuthDegrees, cosAz, sinAz);
    return xform::Vec3{-sinEl * sinAz, cosEl, -sinEl * cosAz};
}

Mat4 viewMatrix(const OrbitCamera& camera) {
    return Mat4::lookAt(eyePosition(camera), camera.target, upVector(camera));
}

// Near-plane clipping in clip space
// ---------------------------------
// After the projection matrix, a point is visible in depth when
// -w <= z <= w. The near plane is z = -w, so define the signed distance
//
//   d = z + w        (d >= 0: in front of the near plane; d < 0: behind it)
//
// d is a LINEAR function of the clip coordinates, and clip coordinates vary
// linearly along an edge (the projection is a matrix). So where the edge
// crosses the plane is found by simple linear interpolation:
//
//   t = d_a / (d_a - d_b),   crossing point = a + t (b - a)   (all four components)
//
// Why BEFORE the perspective divide? With perspective, w = distance in front
// of the camera. For a point BEHIND the camera w is negative, and dividing
// by it flips the signs of x and y: the point would be drawn mirrored on the
// opposite side of the screen. At the camera's own plane w = 0 and the
// divide is a division by zero. An edge running from in front of the camera
// to behind it passes through w = 0, so the line between its two divided end
// points is not the picture of the edge at all: it shoots off one side of
// the screen and comes back from the other. Clipping first guarantees that
// every point we divide has w >= zNear > 0. (For orthographic w is always 1,
// and the same test simply removes what is behind the near plane.)
bool clipToNearPlane(xform::Vec4& a, xform::Vec4& b) {
    const float da = a.z + a.w;
    const float db = b.z + b.w;
    if (da < 0.0f && db < 0.0f) {
        return false; // the whole edge is behind the near plane
    }
    if (da < 0.0f) {
        a = lerp(a, b, da / (da - db));
    } else if (db < 0.0f) {
        b = lerp(a, b, da / (da - db));
    }
    return true;
}

// NDC to pixels
// -------------
// NDC x runs -1 (left) .. +1 (right) and y runs -1 (bottom) .. +1 (top). The
// canvas has y DOWN. So:
//
//   1. reflect in the x axis (y -> -y): now -1 is the top, as on the canvas
//   2. window-to-viewport from the window -1..1 to the viewport
//
// This is the same windowToViewport as the 2D camera: the 3D pipeline ends
// with a 2D camera whose window is the NDC square.
xform::Mat3 ndcToViewport(const raster::Rect& viewport) {
    const Camera2D ndcCamera{raster::Rect{-1.0f, -1.0f, 1.0f, 1.0f}, viewport};
    return windowToViewport(ndcCamera) * xform::Mat3::reflectionAcrossXAxis();
}

void drawWireMesh(raster::Canvas& canvas, const xform::WireMesh& mesh,
                  const Mat4& viewProjection, raster::Color color) {
    const raster::Rect whole{0.0f, 0.0f, static_cast<float>(canvas.width()),
                             static_cast<float>(canvas.height())};
    drawWireMesh(canvas, mesh, viewProjection, color, whole);
}

// Per edge: world -> clip (matrix), clip at the near plane, divide by w,
// NDC -> pixels (matrix), then the CP1 line path: Cohen-Sutherland against
// the viewport and Bresenham (raster::drawLineClipped).
void drawWireMesh(raster::Canvas& canvas, const xform::WireMesh& mesh,
                  const Mat4& viewProjection, raster::Color color, const raster::Rect& viewport) {
    const xform::Mat3 ndcToPixels = ndcToViewport(viewport);
    const raster::Rect lineClip = viewportLineClip(viewport);
    const int count = static_cast<int>(mesh.vertices.size());

    // Every vertex is shared by several edges (3 per corner of a box), so
    // transform each one once.
    std::vector<xform::Vec4> clip;
    clip.reserve(mesh.vertices.size());
    for (const xform::Vec3& v : mesh.vertices) {
        clip.push_back(toClip(viewProjection, v));
    }

    for (const std::pair<int, int>& edge : mesh.edges) {
        if (edge.first < 0 || edge.first >= count || edge.second < 0 || edge.second >= count) {
            continue;
        }
        xform::Vec4 a = clip[edge.first];
        xform::Vec4 b = clip[edge.second];
        if (!clipToNearPlane(a, b) || a.w <= 0.0f || b.w <= 0.0f) {
            continue;
        }
        raster::drawLineClipped(canvas, toPixel(ndcToPixels, a), toPixel(ndcToPixels, b),
                                lineClip, color);
    }
}

bool projectPoint(const Mat4& viewProjection, xform::Vec3 point, const raster::Rect& viewport,
                  raster::Point& pixel) {
    const xform::Vec4 c = toClip(viewProjection, point);
    if (c.z + c.w < 0.0f || c.w <= 0.0f) {
        return false;
    }
    pixel = toPixel(ndcToViewport(viewport), c);
    return true;
}

void fillPlanPolygon(raster::Canvas& canvas, const raster::Polygon& plan, float height,
                     const Mat4& viewProjection, raster::Color color, const raster::Rect& viewport) {
    raster::Polygon onScreen;
    onScreen.vertices.reserve(plan.vertices.size());
    for (const raster::Point& p : plan.vertices) {
        raster::Point pixel;
        if (!projectPoint(viewProjection, xform::Vec3{p.x, height, p.y}, viewport, pixel)) {
            return;
        }
        onScreen.vertices.push_back(pixel);
    }
    const raster::Polygon visible = raster::clipPolygonSutherlandHodgman(
        onScreen, raster::makeRectangle(viewport.xmin, viewport.ymin, viewport.xmax - viewport.xmin,
                                        viewport.ymax - viewport.ymin));
    if (visible.vertices.size() >= 3) {
        raster::fillPolygonScanline(canvas, visible, color);
    }
}

} // namespace view
