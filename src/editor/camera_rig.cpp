#include "editor/camera_rig.h"

#include "anim/easing.h"
#include "view/camera3d.h"

#include <algorithm>
#include <cmath>

namespace editor {

namespace {

constexpr float kPlanFovDegrees = 2.0f;
constexpr float kFrameSeconds = 0.45f;

float radians(float degrees) {
    return degrees * static_cast<float>(raster::kPi) / 180.0f;
}

} // namespace

void CameraRig::update(float dt) {
    // The blend moves at a constant rate; the EASING is applied when the
    // camera is built (state()), so a reversed transition mid-way stays
    // smooth.
    const float step = transitionSeconds > 0.0f ? dt / transitionSeconds : 1.0f;
    if (blend < blendTarget) {
        blend = std::min(blendTarget, blend + step);
    } else if (blend > blendTarget) {
        blend = std::max(blendTarget, blend - step);
    }

    if (framing) {
        frameTime = std::min(frameTime + dt, kFrameSeconds);
        const float u = anim::ease(anim::Easing::EaseInOut, frameTime / kFrameSeconds);
        target.x = anim::lerp(frameFromTarget.x, frameToTarget.x, u);
        target.y = anim::lerp(frameFromTarget.y, frameToTarget.y, u);
        target.z = anim::lerp(frameFromTarget.z, frameToTarget.z, u);
        viewHeight = anim::lerp(frameFromHeight, frameToHeight, u);
        if (frameTime >= kFrameSeconds) {
            framing = false;
        }
    }
}

void CameraRig::frameRoom(float width, float depth, float aspect, bool animate) {
    // Visible height needed: the room's depth, or its width squeezed into
    // the view's aspect ratio, whichever is larger, plus a margin.
    const float margin = 1.18f;
    const float needed = std::max(depth, width / std::max(aspect, 0.1f)) * margin;
    frameFromTarget = target;
    frameToTarget = {width / 2.0f, 0.0f, depth / 2.0f};
    frameFromHeight = viewHeight;
    frameToHeight = needed;
    if (animate) {
        framing = true;
        frameTime = 0.0f;
    } else {
        framing = false;
        target = frameToTarget;
        viewHeight = frameToHeight;
    }
}

void CameraRig::orbit(float dxPixels, float dyPixels) {
    azimuth3D -= dxPixels * 0.3f;
    azimuth3D = std::fmod(azimuth3D, 360.0f);
    elevation3D = std::clamp(elevation3D + dyPixels * 0.25f, 8.0f, 89.0f);
}

CameraState CameraRig::state(float aspect) const {
    CameraState s;
    s.aspect = std::max(aspect, 1e-3f);
    s.blend = anim::ease(anim::Easing::EaseInOut, blend);
    s.elevationDegrees = anim::lerp(90.0f, elevation3D, s.blend);
    s.azimuthDegrees = anim::lerpAngle(0.0f, azimuth3D, s.blend);
    s.orthographic = blend <= 0.0f;

    const float fov = anim::lerp(kPlanFovDegrees, fov3D, s.blend);
    s.tanHalfFov = std::tan(radians(fov) / 2.0f);
    s.halfHeight = viewHeight / 2.0f;
    // Distance at which a fov-degree camera sees viewHeight cm at the target.
    const float radius = s.halfHeight / s.tanHalfFov;

    view::OrbitCamera orbitCamera;
    orbitCamera.target = target;
    orbitCamera.radius = radius;
    orbitCamera.azimuthDegrees = s.azimuthDegrees;
    orbitCamera.elevationDegrees = s.elevationDegrees;

    s.eye = view::eyePosition(orbitCamera);
    s.view = view::viewMatrix(orbitCamera);
    s.forward = view::normalize(view::subtract(target, s.eye));
    s.right = view::normalize(view::cross(s.forward, view::upVector(orbitCamera)));
    s.up = view::cross(s.right, s.forward);

    // Depth range: generous around the target. The near plane grows with
    // the distance so the depth buffer keeps its precision.
    const float zNear = std::max(2.0f, radius * 0.02f);
    const float zFar = radius * 2.0f + 4000.0f;
    if (s.orthographic) {
        const float halfWidth = s.halfHeight * s.aspect;
        s.projection = view::Mat4::orthographic(-halfWidth, halfWidth, -s.halfHeight, s.halfHeight, zNear, zFar);
    } else {
        s.projection = view::Mat4::perspective(fov, s.aspect, zNear, zFar);
    }
    s.viewProjection = s.projection * s.view;
    return s;
}

// Orthographic: every ray is parallel to the view direction and starts on
// the camera's image plane, shifted right and up by the NDC position times
// the half-size of the view.
// Perspective: every ray starts at the eye. A point at NDC (x, y) lies on
// the image plane one unit in front of the eye at
//     forward + right * (x * tan(fov/2) * aspect) + up * (y * tan(fov/2)).
Ray rayThroughNdc(const CameraState& camera, float ndcX, float ndcY) {
    Ray ray;
    if (camera.orthographic) {
        const float halfWidth = camera.halfHeight * camera.aspect;
        ray.origin = view::add(camera.eye, view::add(view::scaled(camera.right, ndcX * halfWidth),
                                                     view::scaled(camera.up, ndcY * camera.halfHeight)));
        ray.direction = camera.forward;
    } else {
        ray.origin = camera.eye;
        const xform::Vec3 offset = view::add(view::scaled(camera.right, ndcX * camera.tanHalfFov * camera.aspect),
                                             view::scaled(camera.up, ndcY * camera.tanHalfFov));
        ray.direction = view::normalize(view::add(camera.forward, offset));
    }
    return ray;
}

bool intersectHorizontalPlane(const Ray& ray, float height, xform::Vec3& hit) {
    if (std::fabs(ray.direction.y) < 1e-6f) {
        return false;
    }
    const float t = (height - ray.origin.y) / ray.direction.y;
    if (t < 0.0f) {
        return false;
    }
    hit = view::add(ray.origin, view::scaled(ray.direction, t));
    return true;
}

bool projectToNdc(const CameraState& camera, xform::Vec3 point, float& ndcX, float& ndcY) {
    const xform::Vec4 clip = camera.viewProjection * xform::Vec4{point.x, point.y, point.z, 1.0f};
    if (clip.w <= 1e-6f) {
        return false;
    }
    ndcX = clip.x / clip.w;
    ndcY = clip.y / clip.w;
    return true;
}

} // namespace editor
