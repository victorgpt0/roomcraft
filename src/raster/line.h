#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

namespace raster {

void drawLineDDA(Canvas& canvas, int x0, int y0, int x1, int y1, Color color);
void drawLineBresenham(Canvas& canvas, int x0, int y0, int x1, int y1, Color color);

// Clips the line (x0,y0)-(x1,y1) against clipWindow in place. Returns false
// if the line lies entirely outside the window.
bool clipLineCohenSutherland(int& x0, int& y0, int& x1, int& y1, Rect clipWindow);

} // namespace raster
