#pragma once

#include "raster/canvas.h"
#include "raster/types.h"

#include <string>

// Text drawn with a built-in bitmap font.
//
// A bitmap font stores each character as a tiny picture: here a grid 5
// pixels wide and 7 pixels high, where each cell is either on or off.
// Drawing a character means copying its "on" cells to the canvas. Nothing
// is loaded from a file; the whole font is a table of numbers in text.cpp.
//
// The font covers the printable ASCII characters (space to ~). Any other
// character (including each byte of a non-ASCII character) is drawn as an
// empty box, so missing glyphs are visible instead of silently skipped.
//
// RoomCraft use: frame counters and captions in the animations, and labels
// and measurements on the floor plan (Checkpoint 1 could only print these to
// the terminal).

namespace output {

constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
constexpr int kGlyphAdvance = 6; // glyph width plus one empty column between characters

// Draws `text` on one line. (x, y) is the top-left corner of the first
// character. Each font cell becomes a scale x scale block of pixels; scale
// below 1 is treated as 1. Pixels outside the canvas are skipped, so text
// may run off any edge.
void drawText(raster::Canvas& canvas, int x, int y, const std::string& text,
              raster::Color color, int scale = 1);

// Width in pixels of `text` as drawn by drawText: 6 * scale per character,
// without the empty column after the last one. 0 for an empty string.
// Use it to centre or right-align text.
int textWidth(const std::string& text, int scale = 1);

// Height in pixels of a line of text: 7 * scale.
int textHeight(int scale = 1);

} // namespace output
