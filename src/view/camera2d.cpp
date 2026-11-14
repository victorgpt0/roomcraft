#include "view/camera2d.h"

#include "raster/clipping.h"
#include "raster/fill.h"
#include "raster/line.h"

namespace view {

namespace {

const raster::Color kOutline{40, 40, 40}; // same as roomcraft::drawFurniture

float width(const raster::Rect& r) { return r.xmax - r.xmin; }
float height(const raster::Rect& r) { return r.ymax - r.ymin; }

raster::Polygon rectPolygon(const raster::Rect& r) {
    return raster::makeRectangle(r.xmin, r.ymin, width(r), height(r));
}

// The drawing helpers below take the matrix and the viewport separately, so
// drawFurnitureThroughCamera builds the matrix once for the whole list.

void drawLine(raster::Canvas& canvas, raster::Point a, raster::Point b,
              const xform::Mat3& toScreen, const raster::Rect& viewport, raster::Color color) {
    // Transform first, then clip in PIXELS: the viewport is a plain
    // axis-aligned rectangle there, which is what Cohen-Sutherland needs.
    // drawLineClipped = Cohen-Sutherland + Bresenham (raster/line.h).
    raster::drawLineClipped(canvas, xform::transformPoint(toScreen, a),
                            xform::transformPoint(toScreen, b), viewportLineClip(viewport), color);
}

void drawOutline(raster::Canvas& canvas, const raster::Polygon& polygon,
                 const xform::Mat3& toScreen, const raster::Rect& viewport, raster::Color color) {
    const std::vector<raster::Point>& v = polygon.vertices;
    for (size_t i = 0; i < v.size(); ++i) {
        drawLine(canvas, v[i], v[(i + 1) % v.size()], toScreen, viewport, color);
    }
}

void fillPolygon(raster::Canvas& canvas, const raster::Polygon& polygon,
                 const xform::Mat3& toScreen, const raster::Rect& viewport, raster::Color color) {
    const raster::Polygon onScreen = xform::transformPolygon(toScreen, polygon);
    // Sutherland-Hodgman needs a CONVEX clip window; a rectangle is convex.
    // The subject (the piece) may be concave, like the L-shaped sofa.
    const raster::Polygon visible = raster::clipPolygonSutherlandHodgman(onScreen, rectPolygon(viewport));
    if (visible.vertices.size() >= 3) {
        raster::fillPolygonScanline(canvas, visible, color);
    }
}

} // namespace

// Window-to-viewport as a composite of three simple transforms
// -------------------------------------------------------------
// A room point p inside the window must land at the SAME RELATIVE position
// inside the viewport: if p is 30% of the way across the window, its pixel
// is 30% of the way across the viewport. Written out for x:
//
//   (x' - vxmin) / (vxmax - vxmin) = (x - wxmin) / (wxmax - wxmin)
//
//   x' = vxmin + (x - wxmin) * sx,    sx = (vxmax - vxmin) / (wxmax - wxmin)
//
// and the same for y with sy. Read from the inside out, that is three steps:
//
//   1. T(-wxmin, -wymin)   move the window's corner to the origin
//   2. S(sx, sy)           stretch the window to the size of the viewport
//   3. T(+vxmin, +vymin)   move the origin to the viewport's corner
//
// Points are column vectors, so the first step is written on the RIGHT:
//
//   M = T(vxmin, vymin) * S(sx, sy) * T(-wxmin, -wymin)
//
//       | sx  0   vxmin - sx * wxmin |
//     = | 0   sy  vymin - sy * wymin |
//       | 0   0   1                  |
//
// Check: the window corner (wxmin, wymin) -> (0, 0) -> (0, 0) -> (vxmin, vymin),
// and (wxmax, wymax) -> (ww, wh) -> (vw, vh) -> (vxmax, vymax).
xform::Mat3 windowToViewport(const Camera2D& camera) {
    const raster::Rect& w = camera.window;
    const raster::Rect& v = camera.viewport;
    if (width(w) == 0.0f || height(w) == 0.0f) {
        return xform::Mat3::identity();
    }
    const float sx = width(v) / width(w);
    const float sy = height(v) / height(w);
    return xform::Mat3::translation(v.xmin, v.ymin) * xform::Mat3::scaling(sx, sy) *
           xform::Mat3::translation(-w.xmin, -w.ymin);
}

float scaleX(const Camera2D& camera) {
    return width(camera.window) == 0.0f ? 0.0f : width(camera.viewport) / width(camera.window);
}

float scaleY(const Camera2D& camera) {
    return height(camera.window) == 0.0f ? 0.0f : height(camera.viewport) / height(camera.window);
}

raster::Point roomToScreen(const Camera2D& camera, raster::Point roomPoint) {
    return xform::transformPoint(windowToViewport(camera), roomPoint);
}

// The inverse matrix undoes the three steps in reverse order:
//   M^-1 = T(wxmin, wymin) * S(1/sx, 1/sy) * T(-vxmin, -vymin)
// xform::inverse computes it in general; we use that rather than a second
// hand-written formula, so the two directions can never disagree.
bool screenToRoom(const Camera2D& camera, raster::Point screenPoint, raster::Point& roomPoint) {
    if (width(camera.window) == 0.0f || height(camera.window) == 0.0f) {
        return false;
    }
    xform::Mat3 toRoom;
    if (!xform::inverse(windowToViewport(camera), toRoom)) {
        return false;
    }
    roomPoint = xform::transformPoint(toRoom, screenPoint);
    return true;
}

// Panning moves the WINDOW (what we look at), not the room. The viewport
// stays where it is on the canvas.
void pan(Camera2D& camera, float dx, float dy) {
    camera.window.xmin += dx;
    camera.window.xmax += dx;
    camera.window.ymin += dy;
    camera.window.ymax += dy;
}

// Zooming about an anchor A is a scaling of the WINDOW about A by 1/factor:
// every window corner c moves to A + (c - A) / factor.
//
// Why A stays still on screen: its pixel is
//   vxmin + (A.x - wxmin) * vw / ww.
// Both (A.x - wxmin) and ww are divided by the same factor, so their ratio,
// and therefore the pixel, does not change. Everything else moves away from
// A (zoom in) or towards it (zoom out).
void zoomAbout(Camera2D& camera, float factor, raster::Point anchor) {
    if (factor <= 0.0f) {
        return;
    }
    raster::Rect& w = camera.window;
    w.xmin = anchor.x + (w.xmin - anchor.x) / factor;
    w.xmax = anchor.x + (w.xmax - anchor.x) / factor;
    w.ymin = anchor.y + (w.ymin - anchor.y) / factor;
    w.ymax = anchor.y + (w.ymax - anchor.y) / factor;
}

// If the window is relatively wider than the viewport, sx < sy and a circle
// would come out as a tall ellipse. Growing the window's SHORTER side (in
// aspect terms) until the ratios agree makes sx == sy. We grow rather than
// shrink so that nothing the caller asked to see is cut off.
void matchViewportAspect(Camera2D& camera) {
    raster::Rect& w = camera.window;
    const float ww = width(w);
    const float wh = height(w);
    const float vw = width(camera.viewport);
    const float vh = height(camera.viewport);
    if (ww <= 0.0f || wh <= 0.0f || vw <= 0.0f || vh <= 0.0f) {
        return;
    }
    const float viewportAspect = vw / vh;
    const float cx = (w.xmin + w.xmax) / 2.0f;
    const float cy = (w.ymin + w.ymax) / 2.0f;
    if (ww / wh < viewportAspect) {
        const float half = wh * viewportAspect / 2.0f; // too narrow: widen
        w.xmin = cx - half;
        w.xmax = cx + half;
    } else {
        const float half = ww / viewportAspect / 2.0f; // too wide: make taller
        w.ymin = cy - half;
        w.ymax = cy + half;
    }
}

raster::Rect viewportLineClip(const raster::Rect& viewport) {
    return raster::Rect{viewport.xmin, viewport.ymin, viewport.xmax - 1.0f, viewport.ymax - 1.0f};
}

void drawLineThroughCamera(raster::Canvas& canvas, raster::Point a, raster::Point b,
                           const Camera2D& camera, raster::Color color) {
    drawLine(canvas, a, b, windowToViewport(camera), camera.viewport, color);
}

void drawPolygonOutlineThroughCamera(raster::Canvas& canvas, const raster::Polygon& polygon,
                                     const Camera2D& camera, raster::Color color) {
    drawOutline(canvas, polygon, windowToViewport(camera), camera.viewport, color);
}

void fillPolygonThroughCamera(raster::Canvas& canvas, const raster::Polygon& polygon,
                              const Camera2D& camera, raster::Color color) {
    fillPolygon(canvas, polygon, windowToViewport(camera), camera.viewport, color);
}

// Two matrices are involved per piece: the piece's pose (local -> room,
// inside worldFootprint) and the camera (room -> screen). The full chain is
//   screen = windowToViewport * poseMatrix * local
// Fill first, outline second, so the outline is not painted over.
void drawFurnitureThroughCamera(raster::Canvas& canvas,
                                const std::vector<roomcraft::Furniture>& pieces,
                                const Camera2D& camera) {
    const xform::Mat3 toScreen = windowToViewport(camera);
    for (const roomcraft::Furniture& piece : pieces) {
        const raster::Polygon world = roomcraft::worldFootprint(piece);
        fillPolygon(canvas, world, toScreen, camera.viewport, piece.color);
        drawOutline(canvas, world, toScreen, camera.viewport, kOutline);
    }
}

} // namespace view
