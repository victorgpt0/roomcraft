#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"

#include <vector>

// Line drawing and line operations.
//
// All coordinates are screen coordinates (origin top-left, y down, integer
// coordinates are pixel centres - see types.h). Every line includes BOTH end
// pixels. Pixels that fall outside the canvas are skipped by Canvas::setPixel,
// so lines may safely run off the canvas.
//
// RoomCraft use: walls (thick lines), the editor grid (lines clipped to the
// room), dimension/guide lines (dashed), furniture outlines (polygon outline).

namespace raster {

// DDA (Digital Differential Analyzer): steps one pixel at a time along the
// longer axis and adds the slope (a float) to the other coordinate.
void drawLineDDA(Canvas& canvas, int x0, int y0, int x1, int y1, Color color);

// Bresenham / midpoint line: same pixels as an ideal line rounded to the
// nearest pixel, but uses only integer additions and comparisons. Works in
// all eight octants (any direction, any slope).
void drawLineBresenham(Canvas& canvas, int x0, int y0, int x1, int y1, Color color);

// Line of the given thickness in pixels, measured perpendicular to the line.
// The ends are cut square to the major axis (no rounded caps).
// thickness <= 1 draws an ordinary 1-pixel Bresenham line.
void drawThickLine(Canvas& canvas, int x0, int y0, int x1, int y1, int thickness, Color color);

// Dashed line: repeats dashLength drawn pixels followed by gapLength skipped
// pixels, counted in Bresenham steps from (x0, y0). Draws nothing if
// dashLength <= 0; a gapLength <= 0 gives a solid line.
void drawDashedLine(Canvas& canvas, int x0, int y0, int x1, int y1,
                    int dashLength, int gapLength, Color color);

// Joins consecutive points with Bresenham lines (points are rounded to the
// nearest pixel). The path is left open. One point draws a single pixel.
void drawPolyline(Canvas& canvas, const std::vector<Point>& points, Color color);

// Draws every edge of the polygon, including the closing edge from the last
// vertex back to the first.
void drawPolygonOutline(Canvas& canvas, const Polygon& polygon, Color color);

// Cohen-Sutherland line clipping. Clips the segment (x0,y0)-(x1,y1) against
// clipWindow IN PLACE. The window edges count as inside.
// Returns true if some part of the segment is inside (the endpoints now hold
// the visible part), or false if the whole segment is outside (the
// endpoints are then unspecified and should not be drawn).
bool clipLineCohenSutherland(float& x0, float& y0, float& x1, float& y1, Rect clipWindow);

// Clips the segment a-b with Cohen-Sutherland, then draws the visible part
// with Bresenham. Draws nothing if the segment is entirely outside.
void drawLineClipped(Canvas& canvas, Point a, Point b, Rect clipWindow, Color color);

} // namespace raster
