#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"
#include "view/mat4.h"
#include "xform/mat3.h"
#include "xform/vec.h"

// 3D camera: orbit camera and the wireframe drawing pipeline.
//
// Drawing a 3D point takes these steps, each a transform from one coordinate
// system to the next:
//
//   world --view--> camera --projection--> clip --divide by w--> NDC --viewport--> pixels
//
//   world    centimetres in the room, Y up (X = plan x, Z = plan y)
//   camera   the camera at the origin looking down -z, y up on screen
//   clip     homogeneous (x, y, z, w) straight out of the projection matrix
//   NDC      "normalised device coordinates": the visible volume is the cube
//            -1..1 in x, y and z, with y UP
//   pixels   canvas coordinates, y DOWN (the CP1 convention)
//
// The first two steps are one matrix, viewProjection = projection * view,
// built once per frame. drawWireMesh does the rest.
//
// RoomCraft use: the 3D preview of the room, seen from a camera that orbits
// round it, and the "reveal" that tilts from the floor-plan view into 3D.

namespace view {

// Camera on a sphere around `target` (spherical coordinates).
struct OrbitCamera {
    xform::Vec3 target;
    float radius = 800.0f;
    float azimuthDegrees = 45.0f;     // round the vertical axis; 0 = on the +Z side
    float elevationDegrees = 30.0f;   // 90 = straight down (the floor-plan view)
};

// Spherical to Cartesian, with Y up:
//
//   eye = target + radius * ( cos(elev) sin(az),  sin(elev),  cos(elev) cos(az) )
//
// azimuth 0 puts the camera on the +Z side of the target (the bottom of the
// floor plan), azimuth 90 on the +X side (the right of the plan). Seen from
// above, increasing azimuth moves the camera counter-clockwise, like
// Mat4::rotationY. elevation 0 is level with the target, 90 straight above.
xform::Vec3 eyePosition(const OrbitCamera& camera);

// The camera's "up" direction: the direction the eye would move if the
// elevation increased (the tangent of the sphere, pointing towards the top).
// It is always perpendicular to the viewing direction, so unlike the fixed
// world up (0, 1, 0) it still works at elevation 90, where the camera looks
// straight down. There it points to -Z, the top of the floor plan, so the
// view from above shows the plan the right way up.
xform::Vec3 upVector(const OrbitCamera& camera);

// lookAt(eyePosition, target, upVector).
Mat4 viewMatrix(const OrbitCamera& camera);

// Clips the edge a-b (clip coordinates, BEFORE the divide) against the near
// plane, z = -w, keeping the part with z + w >= 0. Works for perspective and
// orthographic projections. Returns false if the whole edge is behind the
// near plane; otherwise a and b now hold the visible part.
bool clipToNearPlane(xform::Vec4& a, xform::Vec4& b);

// The 2D matrix from NDC (x, y in -1..1, y up) to canvas pixels in
// `viewport` (y down). It is a window-to-viewport mapping with the window
// -1..1, after a reflection that flips y (see camera3d.cpp).
xform::Mat3 ndcToViewport(const raster::Rect& viewport);

// Projects every edge (clipped at the near plane and the canvas) and draws it.
void drawWireMesh(raster::Canvas& canvas, const xform::WireMesh& mesh,
                  const Mat4& viewProjection, raster::Color color);

// The same, drawn into `viewport` (pixels, the half-open rule of
// view::Camera2D) instead of the whole canvas, and clipped to it. Used for
// side-by-side views. Edges whose indices are out of range are skipped.
void drawWireMesh(raster::Canvas& canvas, const xform::WireMesh& mesh,
                  const Mat4& viewProjection, raster::Color color, const raster::Rect& viewport);

// Projects one world point to pixel coordinates in `viewport`. Returns false
// (and leaves `pixel` unchanged) if the point is behind the near plane.
bool projectPoint(const Mat4& viewProjection, xform::Vec3 point, const raster::Rect& viewport,
                  raster::Point& pixel);

// Lifts a floor-plan polygon to `height` (plan (x, y) -> world (x, height, y)),
// projects it, clips it to the viewport with Sutherland-Hodgman and fills it
// with the scanline fill. Used to show the coloured floor plan inside the 3D
// view. The polygon is skipped if any vertex is behind the near plane (a
// whole polygon is not clipped in 3D, only edges are).
void fillPlanPolygon(raster::Canvas& canvas, const raster::Polygon& plan, float height,
                     const Mat4& viewProjection, raster::Color color, const raster::Rect& viewport);

} // namespace view
