#pragma once

#include "editor/catalogue.h"
#include "raster/polygon.h"
#include "raster/types.h"
#include "roomcraft/furniture.h"
#include "xform/pose.h"

#include <string>
#include <vector>

// The room and the furniture placed in it: everything the user edits and
// saves.
//
// Plan coordinates, as in every checkpoint: centimetres, x to the right, y
// DOWN. The room's inside is the rectangle (0, 0) .. (width, depth); walls
// are built OUTSIDE it, so the usable floor is exactly width x depth.
//
// The 3D world uses the mapping shared since Checkpoint 2:
//     plan (x, y) at height h  ->  world (X = x, Y = h, Z = y)

namespace editor {

struct Room {
    float width = 560.0f;      // cm, along x
    float depth = 420.0f;      // cm, along y (world Z)
    float wallHeight = 260.0f;
    float wallThickness = 12.0f;
    raster::Color floorColor{214, 186, 148};
    raster::Color wallColor{236, 232, 224};
};

// A gap in a wall: the door or the window. Measured along the wall from its
// start (the left end of the top and bottom walls, the top end of the side
// walls), between heights `bottom` and `top`.
enum class Wall { Top, Right, Bottom, Left };

struct Opening {
    Wall wall = Wall::Bottom;
    float start = 0.0f;
    float end = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
    bool isDoor = false;
};

// The room's door (bottom wall, near the right) and window (top wall,
// centred). They follow the room when it is resized.
std::vector<Opening> openings(const Room& room);

// One placed piece of furniture.
struct Item {
    int id = 0;                    // unique within the layout; stays the same across undo
    std::string typeId;            // which catalogue type
    std::string name;              // shown in labels
    xform::Pose pose;              // position = centre of the piece in the room
    raster::Color color{};
    float heightScale = 1.0f;      // stretches the 3D model vertically
};

struct Layout {
    Room room;
    std::vector<Item> items;       // drawn in this order (later = on top in the plan)
    int nextId = 1;
};

// The room's inside as a polygon (for fitsInRoom and clipping).
raster::Polygon roomOutline(const Room& room);

// The catalogue type of an item (falls back to the first type for an
// unknown id, so a corrupt file still shows something).
const FurnitureType& typeOf(const Item& item);

// The item as a Checkpoint 1 Furniture: local footprint + pose. Lets the
// editor reuse hitTest, findFurnitureAt, fitsInRoom and worldFootprint.
roomcraft::Furniture toFurniture(const Item& item);

// The item's footprint placed in the room (pose applied).
raster::Polygon itemFootprint(const Item& item);

// Makes a new item of a catalogue type at a position (id assigned by the
// caller, usually Layout::nextId).
Item makeItem(const std::string& typeId, raster::Point position, int id);

// A furnished living/bedroom used as the starting layout.
Layout sampleLayout();

// ---- Checks ------------------------------------------------------------------

struct ItemStatus {
    bool fitsInRoom = true;
    bool overlaps = false;       // collides with another (non-rug) piece
    float overlapArea = 0.0f;    // cm^2 of floor where they collide, summed over all others
};

// Do two pieces occupy the same SPACE? Not just the same floor: a chair may
// stand under a table top. They collide when some part of one and some part
// of the other overlap on the plan (overlapArea) AND their height ranges
// overlap. Returns the plan area of the colliding parts (0 = no collision;
// under 1 cm^2 counts as just touching). Rugs (floorLevel) never collide.
float collisionArea(const Item& a, const Item& b);

// Status of every item, in layout order.
std::vector<ItemStatus> checkLayout(const Layout& layout);

// Floor area covered by furniture that stands on the floor (rugs excluded),
// as a fraction of the room's floor, ignoring overlaps.
float floorCoverage(const Layout& layout);

// Moves the item the smallest distance that brings its footprint's bounding
// box inside the room. A piece larger than the room is centred instead.
// Returns true if it moved.
bool clampInsideRoom(Item& item, const Room& room);

// ---- Saving and loading ---------------------------------------------------------
//
// A small text format, one line per thing, so files can be read and edited
// by hand:
//
//   roomcraft-layout 1
//   room <width> <depth> <wallHeight> <floor r g b> <wall r g b>
//   item <typeId> <x> <y> <angle> <scaleX> <scaleY> <heightScale> <r g b> <name...>

std::string serialize(const Layout& layout);
// Returns false (and leaves `layout` unchanged) if the text is not a valid
// layout. Unknown furniture types are skipped; `warning` (if given) says so.
bool deserialize(const std::string& text, Layout& layout, std::string* warning = nullptr);

bool saveLayout(const Layout& layout, const std::string& path);
bool loadLayout(const std::string& path, Layout& layout, std::string* warning = nullptr);

} // namespace editor
