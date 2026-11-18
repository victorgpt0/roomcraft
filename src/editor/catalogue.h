#pragma once

#include "raster/polygon.h"
#include "raster/types.h"

#include <string>
#include <vector>

// The furniture catalogue: every kind of piece the user can place.
//
// Each type has
//   - a FOOTPRINT: the outline it covers on the floor, in LOCAL coordinates
//     centred on (0, 0) (1 unit = 1 cm, y down as on the floor plan). This
//     is what the 2D plan draws and what selection, fit and overlap checks
//     use, exactly as in Checkpoints 1 and 2;
//   - a 3D MODEL made of PARTS. Each part is a polygon in the same local
//     coordinates, lifted into a prism between two heights (extrusion, as in
//     scene::extrude). A bed is a base, a mattress, a headboard and two
//     pillows; a table is a top and legs.
//
// The parts are positioned relative to the piece, and the piece is placed in
// the room by its Pose. That is a two-level hierarchy: room -> piece ->
// parts. Moving or turning the piece moves all its parts with it.
//
// Local orientation convention: the FRONT of a piece (where you sit, the side
// of a cabinet with the doors) faces +y, down on the plan. Backrests and
// headboards are at -y.

namespace editor {

struct Part {
    raster::Polygon footprint; // local plan coordinates of the piece
    float bottom = 0.0f;       // cm above the floor
    float top = 0.0f;
    bool useItemColor = true;  // true: the piece's colour times `shade`
    float shade = 1.0f;        // 1 = the piece's colour, < 1 darker, > 1 lighter
    raster::Color color{};     // used when useItemColor is false (pillows, legs, ...)
};

struct FurnitureType {
    std::string id;       // stable name used in saved layouts, e.g. "bed_double"
    std::string name;     // shown to the user, e.g. "Double bed"
    std::string category; // "Bedroom", "Living", "Dining", "Office", "Decor"
    raster::Polygon footprint;
    raster::Color color;  // default colour of a new piece
    float height = 0.0f;  // overall height in cm
    bool floorLevel = false; // lies flat on the floor (rugs): may overlap other pieces
    std::vector<Part> parts;
};

// All types, in the order the palette shows them.
const std::vector<FurnitureType>& catalogue();

// The type with this id, or nullptr.
const FurnitureType* findType(const std::string& id);

// The colour a part is drawn with on a piece of colour `itemColor`.
raster::Color partColor(const Part& part, raster::Color itemColor);

// Helpers used to build the catalogue, also handy in tests.
raster::Polygon centredRectangle(float width, float depth);         // centred on (0, 0)
raster::Polygon rectangleFromTo(float x0, float y0, float x1, float y1);
raster::Polygon circlePolygon(raster::Point centre, float radius, int sides);
raster::Polygon ellipsePolygon(raster::Point centre, float radiusX, float radiusY, int sides);

} // namespace editor
