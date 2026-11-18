#pragma once

#include "view/mat4.h"
#include "xform/vec.h"

// The camera of the interactive app: one camera for both the 2D plan and the
// 3D view, and the animated transition between them.
//
// 2D plan  = looking straight down with an ORTHOGRAPHIC projection: the
//            floor plan exactly as in Checkpoints 1 and 2, no perspective.
// 3D view  = an ORBIT camera (spherical coordinates around a target point,
//            view::OrbitCamera) with a PERSPECTIVE projection.
//
// The transition (the "2D-to-3D reveal")
// -------------------------------------
// `blend` runs from 0 (plan) to 1 (3D) over about 0.8 s and is reshaped by
// the CP2 ease-in-out curve. As it grows:
//   - the elevation tilts from 90 degrees (straight down) to the 3D elevation,
//   - the azimuth turns from 0 to the 3D azimuth (shorter way round),
//   - the field of view widens from 2 degrees to the 3D field of view, while
//     the distance shrinks so the target area keeps the same size on screen.
//     This is a DOLLY ZOOM: a 2-degree perspective camera far away is almost
//     a parallel projection, so the switch from the exact orthographic
//     projection at blend = 0 to perspective just after it is invisible.
//
// Zoom is `viewHeight`: how many centimetres of floor fit vertically in the
// view at the target. It means the same in both modes.

namespace editor {

struct Ray {
    xform::Vec3 origin;
    xform::Vec3 direction; // unit length
};

// Everything the renderer and the picking code need for one frame.
struct CameraState {
    view::Mat4 view;
    view::Mat4 projection;
    view::Mat4 viewProjection;
    xform::Vec3 eye;
    xform::Vec3 forward; // unit, from the eye towards the target
    xform::Vec3 right;   // unit, screen right
    xform::Vec3 up;      // unit, screen up
    bool orthographic = true;
    float halfHeight = 0.0f;  // orthographic: half the visible height in cm
    float tanHalfFov = 0.0f;  // perspective: tan(fov / 2)
    float aspect = 1.0f;      // width / height
    float blend = 0.0f;       // eased blend: 0 = plan, 1 = 3D
    float elevationDegrees = 90.0f;
    float azimuthDegrees = 0.0f;
};

struct CameraRig {
    xform::Vec3 target{280.0f, 0.0f, 210.0f};
    float viewHeight = 600.0f;     // zoom, cm of floor visible vertically
    float azimuth3D = 30.0f;       // 3D orbit angles (kept while in 2D), degrees
    float elevation3D = 38.0f;
    float fov3D = 50.0f;
    float blend = 0.0f;            // 0 = plan, 1 = 3D (linear in time)
    float blendTarget = 0.0f;
    float transitionSeconds = 0.8f;

    // Smooth "frame the room" move.
    bool framing = false;
    xform::Vec3 frameFromTarget, frameToTarget;
    float frameFromHeight = 0.0f, frameToHeight = 0.0f;
    float frameTime = 0.0f;

    bool is3D() const { return blendTarget > 0.5f; }
    void set3D(bool on) { blendTarget = on ? 1.0f : 0.0f; }
    bool transitioning() const { return blend != blendTarget; }

    // Advances the transition and the framing move by dt seconds.
    void update(float dt);

    // Starts a smooth move that shows a width x depth room with a margin.
    void frameRoom(float width, float depth, float aspect, bool animate = true);

    // Orbit (3D only): drag by (dx, dy) pixels. Immediate.
    void orbit(float dxPixels, float dyPixels);

    // Turn by `turnDegrees` round the vertical axis and tilt by
    // `tiltDegrees` (up = positive), gliding there over a fraction of a
    // second. Repeated calls add up, so holding a key or button turns
    // smoothly and continuously.
    void orbitBy(float turnDegrees, float tiltDegrees);

    // Glides back to the default 3D angles.
    void resetOrbit();

    // Turntable: while in 3D, the camera turns by autoRotateSpeed degrees per
    // second. Dragging or stepping the camera turns it off.
    bool autoRotate = false;
    float autoRotateSpeed = 15.0f;

    // Where orbitBy is heading. Exposed for tests and the UI.
    float azimuthGoal = 30.0f;
    float elevationGoal = 38.0f;
    bool orbitGliding = false;

    static constexpr float kDefaultAzimuth = 30.0f;
    static constexpr float kDefaultElevation = 38.0f;
    static constexpr float kMinElevation = 8.0f;
    static constexpr float kMaxElevation = 89.0f;

    CameraState state(float aspect) const;
};

// The ray from the camera through a point of the view, given in NDC
// (-1..1, y up). For the orthographic plan the rays are parallel.
Ray rayThroughNdc(const CameraState& camera, float ndcX, float ndcY);

// Where the ray meets the horizontal plane Y = height. Returns false if it is
// parallel to the plane or the plane is behind the ray's origin.
bool intersectHorizontalPlane(const Ray& ray, float height, xform::Vec3& hit);

// World point -> NDC (x, y in -1..1 when visible, y up). Returns false if
// the point is behind the camera.
bool projectToNdc(const CameraState& camera, xform::Vec3 point, float& ndcX, float& ndcY);

} // namespace editor
