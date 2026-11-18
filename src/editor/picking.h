#pragma once

#include "editor/camera_rig.h"
#include "editor/layout.h"
#include "raster/polygon.h"

// Choosing the piece of furniture under the mouse, in 2D and in 3D.
//
// Checkpoint 1 picked in 2D: the click point was tested against each
// footprint with even-odd point-in-polygon. In the 3D view the mouse is no
// longer a point on the floor: it is a RAY from the camera through the
// pixel. A piece is hit when the ray passes through one of its parts, and the
// NEAREST hit along the ray wins (it hides the ones behind it).
//
// Each part is a vertical PRISM (a polygon lifted between two heights), so
// "ray hits part" reduces to three simple tests (see picking.cpp).

namespace editor {

// Distance along the ray (in units of ray.direction, which is unit length)
// to the first point where it enters the prism, or a negative number if it
// misses. `plan` is the prism's polygon in plan coordinates (world X, Z).
float rayHitPrism(const Ray& ray, const raster::Polygon& plan, float bottom, float top);

struct PickResult {
    int index = -1;          // index into layout.items, -1 = nothing
    float distance = 0.0f;   // along the ray
    xform::Vec3 point;       // where the ray hit
};

// The nearest item whose 3D model the ray hits. Rugs are thin, so they are
// lifted to at least 2 cm to stay clickable.
PickResult pickItem(const Layout& layout, const Ray& ray);

} // namespace editor
