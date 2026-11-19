#include "app.h"
#include "platform.h"

#include "editor/layout.h"
#include "editor/picking.h"
#include "gfx/gl.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace app {

namespace {

void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

float radians(float degrees) {
    return degrees * static_cast<float>(raster::kPi) / 180.0f;
}

} // namespace

App::App(Options options) : options_(std::move(options)) {}

App::~App() {
    shutdown();
}

// ---- Window and OpenGL context ------------------------------------------------------

bool App::initWindow() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "Could not initialise GLFW (is there a display?)\n");
        return false;
    }
    // OpenGL 3.3 core profile: modern, shader-based OpenGL with the old
    // fixed-function calls removed. Supported by every GPU of the last
    // fifteen years.
#ifdef __EMSCRIPTEN__
    // In the browser: WebGL 2 (OpenGL ES 3.0) on the page's <canvas>.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);   // used for the floor shadows
    glfwWindowHint(GLFW_SAMPLES, 4);        // 4x multisampling: smooth edges
    if (!options_.captureDirectory.empty()) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    } else {
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    }

    // In the browser the window is the whole page, not a fixed size.
    platform::pageSize(options_.width, options_.height);
    window_ = glfwCreateWindow(options_.width, options_.height, "RoomCraft - 2D to 3D room planner", nullptr, nullptr);
    if (window_ == nullptr) {
        std::fprintf(stderr, "Could not create an OpenGL 3.3 window\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // wait for the screen refresh: no tearing, no wasted frames

#ifndef __EMSCRIPTEN__
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
        std::fprintf(stderr, "Could not load the OpenGL functions\n");
        return false;
    }
#endif
    std::printf("OpenGL %s on %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
#ifndef __EMSCRIPTEN__
    glEnable(GL_MULTISAMPLE); // WebGL: multisampling is always on when the canvas asks for it
#endif

    std::string error;
    renderer_ = std::make_unique<gfx::Renderer>();
    if (!renderer_->init(error)) {
        std::fprintf(stderr, "Shader error: %s\n", error.c_str());
        return false;
    }

    // Dear ImGui: immediate-mode UI drawn with OpenGL on top of the scene.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // fixed panel layout: nothing to remember

    float scaleX = 1.0f, scaleY = 1.0f;
    if (options_.captureDirectory.empty()) {
        glfwGetWindowContentScale(window_, &scaleX, &scaleY);
    }
#ifdef __EMSCRIPTEN__
    // In the browser, sizes are CSS pixels, which the browser already scales
    // for high-DPI screens: the panels keep their normal size. Only the font
    // is drawn at the screen's real pixel density, so text stays sharp.
    uiScale_ = 1.0f;
    applyTheme(uiScale_, std::clamp(scaleX, 1.0f, 3.0f));
#else
    uiScale_ = std::clamp(scaleX, 1.0f, 3.0f);
    applyTheme(uiScale_);
#endif

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window_, "#canvas"); // mouse wheel, canvas resize
#endif
    ImGui_ImplOpenGL3_Init(ROOMCRAFT_IMGUI_GLSL_VERSION);
    return true;
}

void App::shutdown() {
    if (window_ == nullptr) {
        return;
    }
    stopRecording();
    releaseSoftwareView();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    renderer_.reset(); // free GPU objects while the context still exists
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

int App::run() {
    if (!initWindow()) {
        return 1;
    }
    updateViewport();
    frameRoom(false);
    setStatus("Welcome to RoomCraft. Drag furniture to move it; Tab switches between the 2D plan and 3D.");
    lastTime_ = glfwGetTime();

#ifdef __EMSCRIPTEN__
    // A browser page must not block: the browser calls step() once per
    // display refresh (requestAnimationFrame) instead of a while loop.
    // run() does not return; the page keeps the App alive.
    emscripten_set_main_loop_arg([](void* self) { static_cast<App*>(self)->step(); }, this, 0, true);
    return 0;
#else
    while (glfwWindowShouldClose(window_) == GLFW_FALSE && step()) {
    }
    const int code = captureFailed_ ? 1 : 0;
    shutdown();
    return code;
#endif
}

// One pass of the main loop: events, one frame, swap. Returns false when
// the scripted capture has finished.
bool App::step() {
    glfwPollEvents();
#ifdef __EMSCRIPTEN__
    fitCanvasToPage();
#endif
    const double now = glfwGetTime();
    float dt = static_cast<float>(now - lastTime_);
    lastTime_ = now;
    if (!options_.captureDirectory.empty()) {
        dt = 1.0f / 60.0f; // the scripted capture runs on a fixed clock
    }
    dt = std::min(dt, 0.1f);

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    if (fbWidth == 0 || fbHeight == 0) {
#ifndef __EMSCRIPTEN__
        glfwWaitEvents(); // minimised
#endif
        return true;
    }
    frame(dt);

    if (!options_.captureDirectory.empty()) {
        if (!captureStep(captureFrame_++)) {
            return false;
        }
    } else {
        // Screenshots are read back after the frame is drawn and before the
        // swap, while the back buffer holds exactly what is about to be
        // shown. (In a browser the buffer is cleared once shown, so this is
        // the only moment it can be read.)
        if (screenshotPending_) {
            screenshotPending_ = false;
            saveScreenshot("");
        }
        recordFrame(dt);
    }
    glfwSwapBuffers(window_);
    return true;
}

#ifdef __EMSCRIPTEN__
// The window is the whole browser page. When the page is resized, resize the
// GLFW window (and so the canvas and its framebuffer) to match. The page's
// size is measured, not the canvas's: Emscripten's GLFW fixes the canvas's
// CSS size to the window size, so the canvas never grows by itself. The side
// panels keep their widths; the room view in the middle takes the rest.
void App::fitCanvasToPage() {
    int width = 0, height = 0;
    if (!platform::pageSize(width, height)) {
        return;
    }
    int current[2] = {0, 0};
    glfwGetWindowSize(window_, &current[0], &current[1]);
    if (width > 0 && height > 0 && (width != current[0] || height != current[1])) {
        glfwSetWindowSize(window_, width, height);
    }
}
#endif

// ---- One frame -------------------------------------------------------------------------

void App::frame(float dt) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawUi(); // panels first: they decide where the 3D viewport is
    updateViewport();
    handleKeyboard();
    handleMouse();
    rig_.update(dt);

    // Close an inspector gesture once no widget is held any more.
    if (!ImGui::IsAnyItemActive() && !editor_.dragging()) {
        editor_.endGesture();
    }

    int fbWidth = 0, fbHeight = 0, winWidth = 0, winHeight = 0;
    glfwGetFramebufferSize(window_, &fbWidth, &fbHeight);
    glfwGetWindowSize(window_, &winWidth, &winHeight);
    const float sx = static_cast<float>(fbWidth) / static_cast<float>(std::max(winWidth, 1));
    const float sy = static_cast<float>(fbHeight) / static_cast<float>(std::max(winHeight, 1));

    // Whole window background (behind the panels).
    glViewport(0, 0, fbWidth, fbHeight);
    glClearColor(0.16f, 0.17f, 0.19f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // The room, in the viewport between the panels. OpenGL's y runs up
    // from the bottom of the framebuffer; window coordinates run down.
    gfx::Viewport viewport;
    viewport.x = static_cast<int>(std::lround(viewX_ * sx));
    viewport.width = std::max(1, static_cast<int>(std::lround(viewW_ * sx)));
    viewport.height = std::max(1, static_cast<int>(std::lround(viewH_ * sy)));
    viewport.y = fbHeight - static_cast<int>(std::lround(viewY_ * sy)) - viewport.height;
    renderer_->render(editor_, cameraState(), renderOptions_, lighting(), hoverId_, viewport);

    drawLabels();
    ImGui::Render();
    glViewport(0, 0, fbWidth, fbHeight);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void App::updateViewport() {
    const ImGuiIO& io = ImGui::GetIO();
    viewX_ = leftWidth_;
    viewY_ = menuHeight_;
    viewW_ = std::max(1.0f, io.DisplaySize.x - leftWidth_ - rightWidth_);
    viewH_ = std::max(1.0f, io.DisplaySize.y - menuHeight_ - statusHeight_);
}

editor::CameraState App::cameraState() const {
    return rig_.state(viewW_ / viewH_);
}

gfx::Lighting App::lighting() const {
    gfx::Lighting l;
    // Sun direction from azimuth/elevation, the same spherical coordinates
    // as the orbit camera (Y up, azimuth 0 = +Z).
    const float az = radians(light_.sunAzimuth);
    const float el = radians(light_.sunElevation);
    l.sunDirection = {std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az)};
    l.sunColor = {light_.sunColor[0] * light_.sunIntensity, light_.sunColor[1] * light_.sunIntensity,
                  light_.sunColor[2] * light_.sunIntensity};
    l.ambient = {light_.ambient * 0.95f, light_.ambient, light_.ambient * 1.08f};
    l.lampOn = light_.lampOn;
    l.lampColor = {light_.lampColor[0] * light_.lampIntensity, light_.lampColor[1] * light_.lampIntensity,
                   light_.lampColor[2] * light_.lampIntensity};
    l.shininess = light_.shininess;
    l.specular = light_.specular;
    return l;
}

void App::setStatus(const std::string& message) {
    status_ = message;
    statusTime_ = glfwGetTime();
}

void App::frameRoom(bool animate) {
    const editor::Room& room = editor_.layout().room;
    rig_.frameRoom(room.width + 2.0f * room.wallThickness, room.depth + 2.0f * room.wallThickness, viewW_ / viewH_,
                   animate);
}

// ---- Mouse ----------------------------------------------------------------------------------

bool App::mouseRay(editor::Ray& ray) const {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (mouse.x < -1e30f) {
        return false; // no mouse position (outside the window)
    }
    // Window pixel -> NDC of the viewport (-1..1, y up).
    const float ndcX = (mouse.x - viewX_) / viewW_ * 2.0f - 1.0f;
    const float ndcY = 1.0f - (mouse.y - viewY_) / viewH_ * 2.0f;
    ray = editor::rayThroughNdc(cameraState(), ndcX, ndcY);
    return true;
}

bool App::floorPointUnderMouse(float height, xform::Vec3& hit) const {
    editor::Ray ray;
    return mouseRay(ray) && editor::intersectHorizontalPlane(ray, height, hit);
}

// In the plan the CP1 2D test is used, exactly as the plan is drawn (list
// order, flat footprints). In 3D (and during the transition) the ray is
// tested against every part of every model and the nearest hit wins.
int App::pickUnderMouse(xform::Vec3* hitPoint) const {
    editor::Ray ray;
    if (!mouseRay(ray)) {
        return -1;
    }
    const editor::Layout& layout = editor_.layout();
    if (rig_.blend <= 0.0f) {
        xform::Vec3 floor{};
        if (!editor::intersectHorizontalPlane(ray, 0.0f, floor)) {
            return -1;
        }
        std::vector<roomcraft::Furniture> pieces;
        pieces.reserve(layout.items.size());
        for (const editor::Item& item : layout.items) {
            pieces.push_back(editor::toFurniture(item));
        }
        const int index = roomcraft::findFurnitureAt(pieces, {floor.x, floor.z});
        if (hitPoint != nullptr) {
            *hitPoint = floor;
        }
        return index;
    }
    const editor::PickResult pick = editor::pickItem(layout, ray);
    if (hitPoint != nullptr && pick.index >= 0) {
        *hitPoint = pick.point;
    }
    return pick.index;
}

void App::handleMouse() {
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse = io.MousePos;
    const bool overView = mouse.x >= viewX_ && mouse.x < viewX_ + viewW_ && mouse.y >= viewY_ &&
                          mouse.y < viewY_ + viewH_ && !io.WantCaptureMouse;
    const float dx = mouse.x - lastMouseX_;
    const float dy = mouse.y - lastMouseY_;
    lastMouseX_ = mouse.x;
    lastMouseY_ = mouse.y;

    // Hover highlight.
    hoverId_ = -1;
    if (overView && !editor_.dragging() && !orbiting_ && !panning_) {
        const int index = pickUnderMouse(nullptr);
        if (index >= 0) {
            hoverId_ = editor_.layout().items[static_cast<std::size_t>(index)].id;
        }
    }

    // Left button: on a piece, select and drag it. On empty floor in 3D,
    // a drag orbits the camera and a click (no movement) deselects.
    if (overView && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        xform::Vec3 hit{};
        const int index = pickUnderMouse(&hit);
        if (index >= 0) {
            const editor::Item& item = editor_.layout().items[static_cast<std::size_t>(index)];
            // Drag on the horizontal plane at the height where the piece was
            // grabbed, so the grabbed point stays under the mouse in 3D too.
            dragPlaneHeight_ = rig_.blend <= 0.0f ? 0.0f : hit.y;
            editor_.beginDrag(item.id, {hit.x, hit.z});
        } else if (rig_.is3D() && !rig_.transitioning()) {
            leftOrbitPending_ = true;
            pressX_ = mouse.x;
            pressY_ = mouse.y;
        } else {
            editor_.select(-1);
        }
    }
    if (leftOrbitPending_) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            leftOrbitPending_ = false; // released without moving: a click
            editor_.select(-1);
        } else if (std::hypot(mouse.x - pressX_, mouse.y - pressY_) > 4.0f) {
            leftOrbitPending_ = false; // moved: it is an orbit
            orbiting_ = true;
            orbitButton_ = ImGuiMouseButton_Left;
            // Catch up on the movement since the press, except this frame's,
            // which the orbit code below applies.
            rig_.orbit(mouse.x - dx - pressX_, mouse.y - dy - pressY_);
        }
    }
    if (editor_.dragging()) {
        xform::Vec3 hit{};
        if (floorPointUnderMouse(dragPlaneHeight_, hit)) {
            editor_.dragTo({hit.x, hit.z});
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            editor_.endDrag();
        }
    }

    // Right button: orbit in 3D, pan in the plan. Middle button: pan.
    const bool rightPressed = overView && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    const bool middlePressed = overView && ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
    if ((rightPressed && rig_.is3D()) && !rig_.transitioning()) {
        orbiting_ = true;
        orbitButton_ = ImGuiMouseButton_Right;
    } else if (rightPressed || middlePressed) {
        // Panning keeps the floor point that was grabbed under the mouse:
        // after each move, shift the camera by however far that point has
        // drifted (moving the camera moves every ray by the same amount).
        panning_ = floorPointUnderMouse(0.0f, panGrab_);
        rig_.framing = false;
    }
    if (orbiting_) {
        if (!ImGui::IsMouseDown(orbitButton_)) {
            orbiting_ = false;
        } else if (dx != 0.0f || dy != 0.0f) {
            rig_.orbit(dx, dy);
        }
    }
    if (panning_) {
        xform::Vec3 now{};
        if (floorPointUnderMouse(0.0f, now)) {
            rig_.target.x += panGrab_.x - now.x;
            rig_.target.z += panGrab_.z - now.z;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            panning_ = false;
        }
    }

    // Wheel: zoom about the point under the mouse (it stays put on screen).
    // Shift + wheel: turn the selected piece.
    if (overView && io.MouseWheel != 0.0f) {
        if (io.KeyShift && editor_.selected() != nullptr) {
            editor_.rotateSelected(io.MouseWheel > 0 ? editor_.rotationStep : -editor_.rotationStep);
        } else {
            xform::Vec3 before{}, after{};
            const bool haveBefore = floorPointUnderMouse(0.0f, before);
            rig_.framing = false;
            rig_.viewHeight = std::clamp(rig_.viewHeight * std::pow(0.88f, io.MouseWheel), 60.0f, 6000.0f);
            if (haveBefore && floorPointUnderMouse(0.0f, after)) {
                rig_.target.x += before.x - after.x;
                rig_.target.z += before.z - after.z;
            }
        }
    }
}

// ---- Keyboard -----------------------------------------------------------------------------

void App::handleKeyboard() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }
    const bool ctrl = io.KeyCtrl || io.KeySuper;
    const auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };
    const auto repeat = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, true); };

    if (ctrl && pressed(ImGuiKey_Z)) {
        if (io.KeyShift ? editor_.redo() : editor_.undo()) {
            setStatus(io.KeyShift ? "Redo" : "Undo");
        }
        return;
    }
    if (ctrl && pressed(ImGuiKey_Y)) {
        if (editor_.redo()) {
            setStatus("Redo");
        }
        return;
    }
    if (ctrl && pressed(ImGuiKey_S)) {
        openSavePopup_ = true;
        return;
    }
    if (ctrl && pressed(ImGuiKey_O)) {
        openOpenPopup_ = true;
        return;
    }
    if (ctrl && pressed(ImGuiKey_D)) {
        if (editor_.duplicateSelected() >= 0) {
            setStatus("Duplicated");
        }
        return;
    }
    if (io.WantCaptureKeyboard) {
        return; // a panel has keyboard focus
    }

    if (pressed(ImGuiKey_Tab)) {
        rig_.set3D(!rig_.is3D());
        setStatus(rig_.is3D() ? "3D view: drag empty floor (or right-drag) to rotate, Q/E to turn" : "2D plan");
    }

    // Camera rotation: Q/E turn, Page Up/Down tilt, and the arrow keys when
    // no piece is selected (with a selection they move the piece).
    if (repeat(ImGuiKey_Q)) turnCamera(15.0f, 0.0f);
    if (repeat(ImGuiKey_E)) turnCamera(-15.0f, 0.0f);
    if (repeat(ImGuiKey_PageUp)) turnCamera(0.0f, 10.0f);
    if (repeat(ImGuiKey_PageDown)) turnCamera(0.0f, -10.0f);
    if (editor_.selected() == nullptr) {
        if (repeat(ImGuiKey_LeftArrow)) turnCamera(15.0f, 0.0f);
        if (repeat(ImGuiKey_RightArrow)) turnCamera(-15.0f, 0.0f);
        if (repeat(ImGuiKey_UpArrow)) turnCamera(0.0f, 10.0f);
        if (repeat(ImGuiKey_DownArrow)) turnCamera(0.0f, -10.0f);
    }
    if (pressed(ImGuiKey_F)) {
        frameRoom();
    }
    if (pressed(ImGuiKey_G)) {
        editor_.snapToGrid = !editor_.snapToGrid;
        setStatus(editor_.snapToGrid ? "Snap to grid on" : "Snap to grid off");
    }
    if (pressed(ImGuiKey_F1)) {
        showHelp_ = !showHelp_;
    }
    if (pressed(ImGuiKey_F12)) {
        screenshotPending_ = true; // taken once the frame is drawn
    }
    if (pressed(ImGuiKey_Escape)) {
        editor_.select(-1);
    }
    if (editor_.selected() == nullptr) {
        return;
    }
    if (pressed(ImGuiKey_Delete) || pressed(ImGuiKey_Backspace)) {
        editor_.removeSelected();
        setStatus("Deleted");
        return;
    }
    if (repeat(ImGuiKey_R)) {
        editor_.rotateSelected(io.KeyShift ? -editor_.rotationStep : editor_.rotationStep);
    }
    // Arrow keys move the piece along the SCREEN directions, whatever way
    // the 3D camera faces: screen right/up projected onto the floor.
    const float step = io.KeyShift ? 1.0f : std::max(editor_.gridSize, 1.0f);
    float mx = 0, my = 0;
    if (repeat(ImGuiKey_LeftArrow)) mx -= 1;
    if (repeat(ImGuiKey_RightArrow)) mx += 1;
    if (repeat(ImGuiKey_UpArrow)) my += 1;
    if (repeat(ImGuiKey_DownArrow)) my -= 1;
    if (mx != 0 || my != 0) {
        const editor::CameraState s = cameraState();
        // Screen right and screen up flattened onto the floor (X, Z).
        float rx = s.right.x, rz = s.right.z;
        float ux = s.up.x, uz = s.up.z;
        if (std::hypot(ux, uz) < 1e-3f) { // looking exactly level: use forward
            ux = s.forward.x;
            uz = s.forward.z;
        }
        const float rl = std::max(std::hypot(rx, rz), 1e-6f);
        const float ul = std::max(std::hypot(ux, uz), 1e-6f);
        rx /= rl, rz /= rl, ux /= ul, uz /= ul;
        // Snap the direction to the nearest room axis so a nudge moves
        // exactly one grid step along x or along y.
        float wx = mx * rx + my * ux;
        float wz = mx * rz + my * uz;
        if (std::fabs(wx) >= std::fabs(wz)) {
            wx = wx > 0 ? 1.0f : -1.0f, wz = 0;
        } else {
            wz = wz > 0 ? 1.0f : -1.0f, wx = 0;
        }
        editor_.nudgeSelected(wx * step, wz * step);
    }
}

// Rotating from the plan first switches to 3D: the plan always looks straight
// down, so there is nothing to rotate there.
void App::turnCamera(float turnDegrees, float tiltDegrees) {
    if (!rig_.is3D()) {
        rig_.set3D(true);
    }
    rig_.orbitBy(turnDegrees, tiltDegrees);
}

// ---- Labels -----------------------------------------------------------------------------

// Names and sizes drawn over the scene with ImGui's background draw list, at
// the screen position of a 3D point: world -> clip -> NDC -> window pixels,
// the pipeline of camera3d.h done once per label.
void App::drawLabels() {
    if (!showLabels_) {
        return;
    }
    const editor::CameraState s = cameraState();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->PushClipRect(ImVec2(viewX_, viewY_), ImVec2(viewX_ + viewW_, viewY_ + viewH_), true);

    const auto toScreen = [&](xform::Vec3 p, ImVec2& out) {
        float x = 0, y = 0;
        if (!editor::projectToNdc(s, p, x, y) || std::fabs(x) > 1.2f || std::fabs(y) > 1.2f) {
            return false;
        }
        out = ImVec2(viewX_ + (x + 1.0f) / 2.0f * viewW_, viewY_ + (1.0f - y) / 2.0f * viewH_);
        return true;
    };
    const auto text = [&](ImVec2 at, const char* label, ImU32 color, bool centred) {
        const ImVec2 size = ImGui::CalcTextSize(label);
        if (centred) {
            at.x -= size.x / 2.0f;
            at.y -= size.y / 2.0f;
        }
        // Keep the whole label inside the viewport.
        at.x = std::clamp(at.x, viewX_ + 4.0f, std::max(viewX_ + 4.0f, viewX_ + viewW_ - size.x - 4.0f));
        at.y = std::clamp(at.y, viewY_ + 2.0f, std::max(viewY_ + 2.0f, viewY_ + viewH_ - size.y - 2.0f));
        draw->AddRectFilled(ImVec2(at.x - 3, at.y - 1), ImVec2(at.x + size.x + 3, at.y + size.y + 1),
                            IM_COL32(255, 255, 255, 170), 3.0f);
        draw->AddText(at, color, label);
    };

    const editor::Layout& layout = editor_.layout();
    const bool plan = rig_.blend <= 0.0f;
    for (const editor::Item& item : layout.items) {
        const bool selected = item.id == editor_.selectedId();
        if (!plan && !selected && item.id != hoverId_) {
            continue; // in 3D only the selected and hovered pieces are named
        }
        const raster::Polygon footprint = editor::itemFootprint(item);
        raster::Point c = raster::computeCentroid(footprint);
        if (editor::typeOf(item).floorLevel) {
            // Rugs usually lie under something: label them at their lower edge.
            c.y = raster::computeBoundingBox(footprint).ymax - 14.0f;
        }
        const float top = plan ? 0.0f : editor::typeOf(item).height * item.heightScale * s.blend + 12.0f;
        ImVec2 at;
        if (!toScreen({c.x, top, c.y}, at)) {
            continue;
        }
        text(at, item.name.c_str(), selected ? IM_COL32(170, 80, 0, 255) : IM_COL32(40, 40, 45, 255), true);
        if (selected) {
            // Size of the selected piece (its footprint's bounding box in its
            // own axes, times the pose scale).
            const raster::Rect box = raster::computeBoundingBox(editor::typeOf(item).footprint);
            char size[64];
            std::snprintf(size, sizeof(size), "%.0f x %.0f cm", (box.xmax - box.xmin) * std::fabs(item.pose.scaleX),
                          (box.ymax - box.ymin) * std::fabs(item.pose.scaleY));
            text(ImVec2(at.x, at.y + ImGui::GetTextLineHeight() + 2.0f), size, IM_COL32(90, 90, 95, 255), true);
        }
    }

    // Room dimensions along the top and left walls, in the plan.
    if (plan) {
        const editor::Room& room = layout.room;
        char label[32];
        ImVec2 at;
        std::snprintf(label, sizeof(label), "%.2f m", room.width / 100.0f);
        if (toScreen({room.width / 2.0f, 0.0f, -room.wallThickness - 14.0f}, at)) {
            text(at, label, IM_COL32(30, 30, 35, 255), true);
        }
        std::snprintf(label, sizeof(label), "%.2f m", room.depth / 100.0f);
        if (toScreen({-room.wallThickness - 28.0f, 0.0f, room.depth / 2.0f}, at)) {
            text(at, label, IM_COL32(30, 30, 35, 255), true);
        }
    }
    draw->PopClipRect();
}

} // namespace app
