// Tests for the camera rig (plan <-> 3D blend, rays through pixels) and ray
// picking of furniture.

#include "editor/camera_rig.h"
#include "editor/layout.h"
#include "editor/picking.h"
#include "raster/polygon.h"
#include "roomcraft/furniture.h"
#include "view/mat4.h"

#include "test_helpers.h"

#include <cmath>
#include <string>

using xform::Vec3;

namespace {

double distancePointToRay(const editor::Ray& ray, Vec3 p) {
    const Vec3 w = view::subtract(p, ray.origin);
    const float along = view::dot(w, ray.direction);
    const Vec3 closest = view::add(ray.origin, view::scaled(ray.direction, along));
    return view::length(view::subtract(p, closest));
}

void testPlanCamera() {
    editor::CameraRig rig;
    rig.target = {280, 0, 210};
    rig.viewHeight = 600;
    const editor::CameraState s = rig.state(1.5f);
    CHECK(s.orthographic);
    CHECK_NEAR(s.blend, 0.0, 0);
    CHECK_NEAR(s.elevationDegrees, 90.0, 0);

    // Straight down: forward is -Y; screen up is -Z (the top of the plan),
    // screen right is +X: the plan appears the right way up, as in CP1.
    CHECK_NEAR(s.forward.y, -1.0, 1e-5);
    CHECK_NEAR(s.up.z, -1.0, 1e-5);
    CHECK_NEAR(s.right.x, 1.0, 1e-5);

    // The target is at the centre of the view; a point 300 cm "up" the plan
    // (smaller Z) is at the top edge (half the view height is 300).
    float x = 0, y = 0;
    CHECK(editor::projectToNdc(s, {280, 0, 210}, x, y));
    CHECK_NEAR(x, 0.0, 1e-4);
    CHECK_NEAR(y, 0.0, 1e-4);
    CHECK(editor::projectToNdc(s, {280, 0, -90}, x, y));
    CHECK_NEAR(y, 1.0, 1e-4);
    CHECK(editor::projectToNdc(s, {280 + 450, 0, 210}, x, y)); // half width = 300 * 1.5
    CHECK_NEAR(x, 1.0, 1e-4);
    // Orthographic: height does not change where a point appears.
    float x2 = 0, y2 = 0;
    CHECK(editor::projectToNdc(s, {280 + 450, 200, 210}, x2, y2));
    CHECK_NEAR(x2, x, 1e-4);

    // Rays are parallel and straight down; the ray through NDC (1, 1) hits
    // the floor at the top-right corner of the view.
    const editor::Ray ray = editor::rayThroughNdc(s, 1.0f, 1.0f);
    CHECK_NEAR(ray.direction.y, -1.0, 1e-5);
    Vec3 hit{};
    CHECK(editor::intersectHorizontalPlane(ray, 0.0f, hit));
    CHECK_NEAR(hit.x, 280 + 450, 1e-2);
    CHECK_NEAR(hit.z, 210 - 300, 1e-2);
}

void testRoundTripAllBlends() {
    // For every stage of the transition, every point projected to the
    // screen lies on the ray cast back through that screen position. This
    // is what makes clicking work while the view is moving.
    editor::CameraRig rig;
    rig.target = {300, 0, 200};
    rig.viewHeight = 500;
    rig.azimuth3D = 40;
    rig.elevation3D = 30;
    const Vec3 points[] = {{300, 0, 200}, {0, 0, 0}, {560, 260, 420}, {100, 80, 350}, {450, 40, 30}};
    int failures = 0;
    for (int step = 0; step <= 20; ++step) {
        rig.blend = static_cast<float>(step) / 20.0f;
        for (const float aspect : {0.75f, 1.0f, 1.8f}) {
            const editor::CameraState s = rig.state(aspect);
            CHECK(s.orthographic == (step == 0));
            for (const Vec3& p : points) {
                float x = 0, y = 0;
                if (!editor::projectToNdc(s, p, x, y)) {
                    ++failures;
                    continue;
                }
                const editor::Ray ray = editor::rayThroughNdc(s, x, y);
                if (distancePointToRay(ray, p) > 0.05) {
                    ++failures;
                }
                if (std::fabs(view::length(ray.direction) - 1.0f) > 1e-4f) {
                    ++failures;
                }
            }
        }
    }
    CHECK_MSG(failures == 0, std::to_string(failures) + " round-trip failures");
}

void testTransition() {
    editor::CameraRig rig;
    rig.azimuth3D = 30;
    rig.elevation3D = 38;
    CHECK(!rig.is3D());
    rig.set3D(true);
    CHECK(rig.is3D() && rig.transitioning());

    // Half way in time: eased blend is 0.5 (smoothstep is symmetric).
    rig.update(rig.transitionSeconds / 2.0f);
    CHECK_NEAR(rig.blend, 0.5, 1e-5);
    editor::CameraState s = rig.state(1.0f);
    CHECK(!s.orthographic);
    CHECK_NEAR(s.blend, 0.5, 1e-5);
    CHECK_NEAR(s.elevationDegrees, (90.0 + 38.0) / 2.0, 1e-3);

    // Early on the eased blend lags behind time (ease in).
    editor::CameraRig early;
    early.set3D(true);
    early.update(early.transitionSeconds * 0.1f);
    CHECK(early.state(1.0f).blend < 0.1f);

    // The dolly zoom keeps the target area the same size on screen: a point
    // 100 cm to the side of the target projects to the same NDC distance at
    // every blend while the camera is still nearly overhead.
    editor::CameraRig dolly;
    dolly.azimuth3D = 0;
    dolly.elevation3D = 90; // only the field of view changes
    float reference = 0;
    for (int step = 0; step <= 10; ++step) {
        dolly.blend = static_cast<float>(step) / 10.0f;
        const editor::CameraState d = dolly.state(1.0f);
        float x = 0, y = 0;
        CHECK(editor::projectToNdc(d, view::add(dolly.target, {100, 0, 0}), x, y));
        if (step == 0) {
            reference = x;
        }
        CHECK_NEAR(x, reference, 1e-3);
    }

    // Finishing and reversing.
    rig.update(10.0f);
    CHECK(!rig.transitioning());
    CHECK_NEAR(rig.blend, 1.0, 0);
    s = rig.state(1.0f);
    CHECK_NEAR(s.elevationDegrees, 38.0, 1e-4);
    CHECK_NEAR(s.azimuthDegrees, 30.0, 1e-4);
    rig.set3D(false);
    rig.update(10.0f);
    CHECK(rig.state(1.0f).orthographic);

    // Orbit clamps the elevation (never under the floor, never exactly
    // overhead) and frameRoom centres on the room.
    rig.orbit(0, -10000);
    CHECK_NEAR(rig.elevation3D, 8.0, 0);
    rig.orbit(0, 10000);
    CHECK_NEAR(rig.elevation3D, 89.0, 0);
    rig.frameRoom(560, 420, 1.0f, false);
    CHECK_NEAR(rig.target.x, 280, 0);
    CHECK_NEAR(rig.target.z, 210, 0);
    CHECK(rig.viewHeight >= 560.0f); // aspect 1: the width decides
    rig.frameRoom(1000, 200, 2.0f, true);
    rig.update(0.1f);
    CHECK(rig.framing);
    rig.update(10.0f);
    CHECK(!rig.framing);
    CHECK_NEAR(rig.target.x, 500, 1e-3);
}

void testRayHitPrism() {
    const raster::Polygon square = raster::makeRectangle(0, 0, 100, 100); // plan x 0..100, z 0..100

    // Straight down onto the top at height 50, from height 200.
    editor::Ray down{{50, 200, 50}, {0, -1, 0}};
    CHECK_NEAR(editor::rayHitPrism(down, square, 0, 50), 150.0, 1e-4);
    // Straight down next to it: miss.
    editor::Ray beside{{150, 200, 50}, {0, -1, 0}};
    CHECK(editor::rayHitPrism(beside, square, 0, 50) < 0.0f);

    // Horizontal ray at height 20 towards the side x = 0.
    editor::Ray side{{-30, 20, 40}, {1, 0, 0}};
    CHECK_NEAR(editor::rayHitPrism(side, square, 0, 50), 30.0, 1e-4);
    // Same ray above the prism: miss.
    editor::Ray over{{-30, 60, 40}, {1, 0, 0}};
    CHECK(editor::rayHitPrism(over, square, 0, 50) < 0.0f);
    // Pointing away: miss.
    editor::Ray away{{-30, 20, 40}, {-1, 0, 0}};
    CHECK(editor::rayHitPrism(away, square, 0, 50) < 0.0f);

    // Diagonal ray entering through the top: from (150, 150, 50) going
    // (-1, -1, 0)/sqrt(2) reaches the plan square's corner region at
    // height ... use a ray from above at 45 degrees in x/y.
    const float s = 1.0f / std::sqrt(2.0f);
    editor::Ray slant{{130, 110, 50}, {-s, -s, 0}};
    // y reaches 50 after moving 60 in x: x = 70, inside -> hits top at t = 60*sqrt(2).
    CHECK_NEAR(editor::rayHitPrism(slant, square, 0, 50), 60.0 * std::sqrt(2.0), 1e-3);

    // A prism floating between 72 and 76 (a table top): a horizontal ray at
    // 40 passes under it.
    editor::Ray under{{-30, 40, 50}, {1, 0, 0}};
    CHECK(editor::rayHitPrism(under, square, 72, 76) < 0.0f);
    editor::Ray at{{-30, 74, 50}, {1, 0, 0}};
    CHECK_NEAR(editor::rayHitPrism(at, square, 72, 76), 30.0, 1e-4);

    // Concave L: a vertical ray into the notch misses, one on the arm hits.
    raster::Polygon l;
    l.vertices = {{0, 0}, {100, 0}, {100, 100}, {60, 100}, {60, 40}, {0, 40}};
    editor::Ray notch{{20, 100, 80}, {0, -1, 0}};
    CHECK(editor::rayHitPrism(notch, l, 0, 40) < 0.0f);
    editor::Ray arm{{80, 100, 80}, {0, -1, 0}};
    CHECK_NEAR(editor::rayHitPrism(arm, l, 0, 40), 60.0, 1e-4);
    // A horizontal ray through the notch hits the far inner wall of the L.
    editor::Ray throughNotch{{-10, 20, 70}, {1, 0, 0}};
    CHECK_NEAR(editor::rayHitPrism(throughNotch, l, 0, 40), 70.0, 1e-4); // the side x = 60

    // Starting inside: still a hit (the exit point).
    editor::Ray inside{{50, 10, 50}, {1, 0, 0}};
    CHECK(editor::rayHitPrism(inside, square, 0, 50) >= 0.0f);
}

void testPickItem() {
    editor::Layout layout;
    layout.items.push_back(editor::makeItem("rug", {300, 200}, 1));
    layout.items.push_back(editor::makeItem("coffee_table", {300, 200}, 2));
    layout.items.push_back(editor::makeItem("wardrobe", {100, 200}, 3));

    // From straight above, the table (on top of the rug) is picked at its
    // centre; the rug is picked where the table is not.
    editor::PickResult r = editor::pickItem(layout, {{300, 500, 200}, {0, -1, 0}});
    CHECK(r.index == 1);
    CHECK_NEAR(r.point.y, 42.0, 1e-3);
    r = editor::pickItem(layout, {{390, 500, 200}, {0, -1, 0}});
    CHECK(r.index == 0);
    r = editor::pickItem(layout, {{300, 500, 380}, {0, -1, 0}});
    CHECK(r.index == -1);

    // From the side at 1 m height, looking along +X from the left: the tall
    // wardrobe hides the table behind it.
    r = editor::pickItem(layout, {{-100, 100, 200}, {1, 0, 0}});
    CHECK(r.index == 2);
    // The wardrobe is 120 wide along x around x = 100: its left face is at
    // x = 40, 140 cm from the ray's start.
    CHECK_NEAR(r.point.x, 40.0, 1e-2);
    CHECK_NEAR(r.distance, 140.0, 1e-2);

    // At 300 cm, above everything, nothing is hit.
    r = editor::pickItem(layout, {{-100, 300, 200}, {1, 0, 0}});
    CHECK(r.index == -1);

    // Through the camera: the centre pixel of a 3D view aimed at the table.
    editor::CameraRig rig;
    rig.target = {300, 40, 200};
    rig.blend = 1.0f;
    rig.blendTarget = 1.0f;
    rig.viewHeight = 300;
    const editor::CameraState s = rig.state(1.6f);
    r = editor::pickItem(layout, editor::rayThroughNdc(s, 0, 0));
    CHECK(r.index == 1);

    // Plan picking agrees with the CP1 2D function for points on the floor.
    rig.blend = 0;
    rig.blendTarget = 0;
    const editor::CameraState plan = rig.state(1.6f);
    int disagreements = 0;
    for (float x = 20; x < 560; x += 13) {
        for (float z = 20; z < 420; z += 11) {
            float nx = 0, ny = 0;
            editor::projectToNdc(plan, {x, 0, z}, nx, ny);
            const editor::Ray ray = editor::rayThroughNdc(plan, nx, ny);
            xform::Vec3 hit{};
            editor::intersectHorizontalPlane(ray, 0.0f, hit);
            std::vector<roomcraft::Furniture> pieces;
            for (const editor::Item& item : layout.items) {
                pieces.push_back(editor::toFurniture(item));
            }
            const int flat = roomcraft::findFurnitureAt(pieces, {hit.x, hit.z});
            const bool anyFlat = flat >= 0;
            const bool any3D = editor::pickItem(layout, ray).index >= 0;
            if (anyFlat != any3D) {
                ++disagreements;
            }
        }
    }
    // Boundaries can differ by a pixel; demand near-total agreement.
    CHECK_MSG(disagreements <= 3, std::to_string(disagreements) + " disagreements");
}

} // namespace

int main() {
    testPlanCamera();
    testRoundTripAllBlends();
    testTransition();
    testRayHitPrism();
    testPickItem();
    return test::summary("test_camera_picking");
}
