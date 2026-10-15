#pragma once

#include "raster/canvas.h"
#include "raster/polygon.h"
#include "raster/types.h"

#include <string>
#include <vector>

// RoomCraft furniture on the 2D top-down floor plan.
//
// Units: 1 pixel = 1 cm on the floor plan. Coordinates are screen
// coordinates (origin top-left, y down - see raster/types.h).
//
// Every piece of furniture has a polygon FOOTPRINT: the area it covers on
// the floor, seen from above. All the editor logic (selection by clicking,
// checking it fits in the room) works on that polygon, using the polygon
// algorithms from raster/. Round furniture uses a many-sided polygon as its
// footprint (makeCircleFootprint); it can still be DRAWN with the midpoint
// circle algorithm for a perfectly round look.

namespace roomcraft {

struct Furniture {
    std::string name;
    raster::Polygon footprint;
    raster::Color color;
};

// Regular polygon with `sides` corners approximating a circle, for the
// footprint of round tables, chairs and plant pots. sides < 3 is treated as 3.
raster::Polygon makeCircleFootprint(raster::Point center, float radius, int sides);

// Did a click at `point` land on this piece? (Even-odd point-in-polygon.)
bool hitTest(const Furniture& piece, raster::Point point);

// Which piece was clicked? Pieces later in the list are drawn on top, so the
// list is searched from the back and the topmost hit wins.
// Returns the index into `pieces`, or -1 if the click hit nothing.
int findFurnitureAt(const std::vector<Furniture>& pieces, raster::Point point);

// Does the piece lie completely inside the room?
// The footprint is clipped against the room outline (Sutherland-Hodgman):
// if any part sticks out, clipping removes it and the area shrinks.
// The room outline must be convex (a rectangular room is). A piece touching
// the wall from the inside counts as fitting.
bool fitsInRoom(const Furniture& piece, const raster::Polygon& room);

// Area of the piece that lies inside the room, in square pixels (cm^2).
float areaInsideRoom(const Furniture& piece, const raster::Polygon& room);

// Fills the footprint with the piece's colour and draws a dark outline.
void drawFurniture(raster::Canvas& canvas, const Furniture& piece);

} // namespace roomcraft
