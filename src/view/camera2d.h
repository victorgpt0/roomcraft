#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"
#include "roomcraft/furniture.h"
#include "xform/mat3.h"

#include <vector>

// 2D camera: window-to-viewport mapping, pan and zoom.
//
// The floor plan lives in ROOM coordinates (centimetres). Checkpoint 1 drew
// it at a fixed 1 pixel = 1 cm. A camera lets the editor show any part of the
// room at any size, in any rectangle of the canvas:
//
//   window    the rectangle of the ROOM we want to see (room coordinates)
//   viewport  the rectangle of the CANVAS where it should appear (pixels)
//
// The window-to-viewport transform is the matrix that maps the first
// rectangle onto the second. Moving the window is PANNING; making it smaller
// (so less of the room fills the same viewport) is ZOOMING IN.
//
// Both rectangles use the CP1 convention: y grows downward. Room coordinates
// and pixel coordinates therefore point the same way and, unlike the
// textbook version (world y up, screen y down), no flip is needed.
//
// Which pixels belong to the viewport: the same half-open rule as the
// scanline fill (raster/fill.h). Viewport {10, 20, 110, 70} covers pixel
// columns 10..109 and rows 20..69. Everything drawn through the camera is
// clipped to exactly those pixels, so several viewports can share one canvas
// (a main view and a minimap) without drawing over each other.
//
// RoomCraft use: zooming into a corner of the room to place a piece
// precisely, panning across a large room, the minimap, and mapping a mouse
// click on the screen back to a point in the room (screenToRoom).

namespace view {

struct Camera2D {
    raster::Rect window;   // the part of the room to show (room coordinates)
    raster::Rect viewport; // where it appears on the canvas (pixels)
};

// The matrix that maps room coordinates to canvas pixels:
//
//   M = T(viewport corner) * S(viewport size / window size) * T(-window corner)
//
// (derivation in camera2d.cpp). The window must have a non-zero width and
// height; for a degenerate window the identity is returned.
xform::Mat3 windowToViewport(const Camera2D& camera);

// The camera's scale factors: pixels per room centimetre, horizontally and
// vertically. They are equal when the window and viewport have the same
// aspect ratio (see matchViewportAspect).
float scaleX(const Camera2D& camera);
float scaleY(const Camera2D& camera);

// Room point -> canvas pixel coordinates (not rounded).
raster::Point roomToScreen(const Camera2D& camera, raster::Point roomPoint);

// Canvas pixel -> room point, through the INVERSE of windowToViewport. This
// is how a mouse click on the zoomed view is turned back into a position in
// the room, for hit testing (roomcraft::findFurnitureAt) or placing a piece.
// Returns false if the camera cannot be inverted (degenerate window).
bool screenToRoom(const Camera2D& camera, raster::Point screenPoint, raster::Point& roomPoint);

// Pan: moves the window by (dx, dy) room centimetres. The room appears to
// slide the OTHER way on screen, by (dx * scaleX, dy * scaleY) pixels.
void pan(Camera2D& camera, float dx, float dy);

// Zoom about an anchor point (room coordinates). factor > 1 zooms in (the
// window shrinks), factor < 1 zooms out. The anchor stays at the same pixel
// on screen, so the user can zoom "into" the piece under the mouse cursor.
// A factor <= 0 is ignored.
void zoomAbout(Camera2D& camera, float factor, raster::Point anchor);

// Makes the window's aspect ratio (width / height) equal to the viewport's
// by GROWING the window about its centre in one direction. The whole of the
// requested window stays visible, and afterwards scaleX == scaleY, so circles
// stay round and squares stay square on screen.
void matchViewportAspect(Camera2D& camera);

// The clip rectangle for LINES drawn in the viewport: the centres of its
// first and last pixels, {xmin, ymin, xmax - 1, ymax - 1} (see the pixel rule
// above). Polygons are clipped to the viewport itself and then filled with
// the half-open rule, which gives the same pixels.
raster::Rect viewportLineClip(const raster::Rect& viewport);

// ---- Drawing through a camera ---------------------------------------------
// Each function transforms room geometry with windowToViewport, clips it to
// the viewport with a Checkpoint 1 clipping algorithm, and draws it with a
// Checkpoint 1 drawing algorithm. Nothing outside the viewport is touched.

// Line: transform both ends, clip with Cohen-Sutherland, draw with Bresenham.
void drawLineThroughCamera(raster::Canvas& canvas, raster::Point a, raster::Point b,
                           const Camera2D& camera, raster::Color color);

// Outline: every edge (including the closing one) through
// drawLineThroughCamera. Clipping each EDGE as a line, instead of drawing the
// outline of the clipped polygon, means the viewport border does not get a
// false outline where a piece is cut off.
void drawPolygonOutlineThroughCamera(raster::Canvas& canvas, const raster::Polygon& polygon,
                                     const Camera2D& camera, raster::Color color);

// Filled polygon: transform every vertex, clip with Sutherland-Hodgman
// against the viewport rectangle, fill with the scanline fill.
void fillPolygonThroughCamera(raster::Canvas& canvas, const raster::Polygon& polygon,
                              const Camera2D& camera, raster::Color color);

// The furniture list as the editor shows it: each piece's WORLD footprint
// (pose applied) is filled with its colour and outlined in dark grey, both
// clipped to the viewport. Pieces later in the list are drawn on top.
void drawFurnitureThroughCamera(raster::Canvas& canvas,
                                const std::vector<roomcraft::Furniture>& pieces,
                                const Camera2D& camera);

} // namespace view
