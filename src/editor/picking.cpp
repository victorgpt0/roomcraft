#include "editor/picking.h"

#include "xform/mat3.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace editor {

// A prism is bounded by its top cap, its bottom cap and its side faces. The
// ray enters it through one of them, so the entry point is the nearest of:
//
//   1. caps:  where the ray meets the plane Y = top (or Y = bottom), if that
//      point is inside the polygon (CP1 point-in-polygon on X, Z);
//   2. sides: for each polygon edge, where the ray's shadow on the floor
//      crosses the edge (a 2D segment intersection), if the ray's height
//      there is between bottom and top.
//
// Only the nearest positive candidate matters. If the ray starts inside the
// prism the first candidate is the exit point, which is still a hit.
float rayHitPrism(const Ray& ray, const raster::Polygon& plan, float bottom, float top) {
    float best = std::numeric_limits<float>::infinity();
    const xform::Vec3& o = ray.origin;
    const xform::Vec3& d = ray.direction;

    // 1. Caps.
    if (std::fabs(d.y) > 1e-7f) {
        for (const float y : {top, bottom}) {
            const float t = (y - o.y) / d.y;
            if (t >= 0.0f && t < best) {
                const raster::Point p{o.x + d.x * t, o.z + d.z * t};
                if (raster::pointInPolygon(plan, p)) {
                    best = t;
                }
            }
        }
    }

    // 2. Sides. Ray on the floor: (o.x, o.z) + t * (d.x, d.z). Edge: a + s * (b - a).
    // Solve o + t*d = a + s*e for t and s with Cramer's rule.
    const std::vector<raster::Point>& v = plan.vertices;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const raster::Point a = v[i];
        const raster::Point b = v[(i + 1) % v.size()];
        const double ex = static_cast<double>(b.x) - a.x;
        const double ez = static_cast<double>(b.y) - a.y;
        const double denom = static_cast<double>(d.x) * (-ez) - static_cast<double>(d.z) * (-ex);
        if (std::fabs(denom) < 1e-12) {
            continue; // parallel to the edge (or a vertical ray)
        }
        const double rx = static_cast<double>(a.x) - o.x;
        const double rz = static_cast<double>(a.y) - o.z;
        const double t = (rx * (-ez) - rz * (-ex)) / denom;
        const double s = (static_cast<double>(d.x) * rz - static_cast<double>(d.z) * rx) / denom;
        if (t < 0.0 || s < 0.0 || s > 1.0 || t >= best) {
            continue;
        }
        const double y = o.y + d.y * t;
        if (y >= bottom && y <= top) {
            best = static_cast<float>(t);
        }
    }
    return std::isinf(best) ? -1.0f : best;
}

PickResult pickItem(const Layout& layout, const Ray& ray) {
    PickResult result;
    float nearest = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        const Item& item = layout.items[i];
        const xform::Mat3 place = xform::poseMatrix(item.pose);
        for (const Part& part : typeOf(item).parts) {
            const float bottom = part.bottom * item.heightScale;
            const float top = std::max(part.top * item.heightScale, bottom + 2.0f);
            const float t = rayHitPrism(ray, xform::transformPolygon(place, part.footprint), bottom, top);
            // "<=" so that, at equal distance, the piece later in the list
            // (drawn on top in the plan) wins, as in findFurnitureAt.
            if (t >= 0.0f && t <= nearest) {
                nearest = t;
                result.index = static_cast<int>(i);
            }
        }
    }
    if (result.index >= 0) {
        result.distance = nearest;
        result.point = {ray.origin.x + ray.direction.x * nearest, ray.origin.y + ray.direction.y * nearest,
                        ray.origin.z + ray.direction.z * nearest};
    }
    return result;
}

} // namespace editor
