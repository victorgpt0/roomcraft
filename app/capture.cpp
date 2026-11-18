// Screenshots and GIF recordings of the app window, and the scripted
// capture used to make the documentation images (--capture).
//
// OpenGL renders into the GPU's framebuffer; glReadPixels copies it back.
// From there it is Checkpoint 1 and 2 code: the pixels go into a
// raster::Canvas, which writes BMP files, and output::AnimationWriter
// (our own GIF encoder with LZW) writes animations.

#include "app.h"

#include "editor/layout.h"
#include "editor/picking.h"
#include "gfx/gl.h"
#include "raster/canvas.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h> // FindWindowByName, for the interaction check only

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <system_error>

namespace app {

namespace {

std::string timestamp() {
    const std::time_t now = std::time(nullptr);
    char text[32];
    std::strftime(text, sizeof(text), "%Y%m%d_%H%M%S", std::localtime(&now));
    return text;
}

// Shrinks an RGB image (rows bottom-up, as OpenGL returns them) by `factor`
// in each direction, averaging each factor x factor block of pixels (a box
// filter), into a canvas with the rows top-down.
raster::Canvas toCanvas(const std::vector<unsigned char>& rgb, int width, int height, int factor) {
    factor = std::max(factor, 1);
    const int w = std::max(width / factor, 1);
    const int h = std::max(height / factor, 1);
    raster::Canvas canvas(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int r = 0, g = 0, b = 0, n = 0;
            for (int dy = 0; dy < factor; ++dy) {
                const int sourceRow = height - 1 - (y * factor + dy); // flip: OpenGL row 0 is the bottom
                if (sourceRow < 0) {
                    continue;
                }
                for (int dx = 0; dx < factor; ++dx) {
                    const int sx = x * factor + dx;
                    if (sx >= width) {
                        continue;
                    }
                    const std::size_t i = (static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(width) +
                                           static_cast<std::size_t>(sx)) * 3;
                    r += rgb[i];
                    g += rgb[i + 1];
                    b += rgb[i + 2];
                    ++n;
                }
            }
            n = std::max(n, 1);
            canvas.setPixel(x, y, static_cast<uint8_t>(r / n), static_cast<uint8_t>(g / n), static_cast<uint8_t>(b / n));
        }
    }
    return canvas;
}

// Reduces a canvas to at most 252 colours for the GIF: 6 levels of red, 7 of
// green (the eye is most sensitive to green) and 6 of blue, with ORDERED
// DITHERING. Each pixel's threshold comes from a 4 x 4 Bayer matrix, so a
// colour between two levels becomes a fine, regular mix of both instead of
// a hard band. The GIF writer then finds at most 252 colours and stores
// every one exactly.
void ditherForGif(raster::Canvas& canvas) {
    static const int kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    const auto quantise = [](int value, int levels, float threshold) {
        const float step = 255.0f / static_cast<float>(levels - 1);
        const int q = std::clamp(static_cast<int>(static_cast<float>(value) / step + threshold), 0, levels - 1);
        return static_cast<uint8_t>(std::lround(static_cast<float>(q) * step));
    };
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const float threshold = (static_cast<float>(kBayer[y % 4][x % 4]) + 0.5f) / 16.0f;
            const raster::Color c = canvas.getPixel(x, y);
            canvas.setPixel(x, y, quantise(c.r, 6, threshold), quantise(c.g, 7, threshold), quantise(c.b, 6, threshold));
        }
    }
}

} // namespace

bool App::readFramebuffer(std::vector<unsigned char>& rgb, int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
    if (width <= 0 || height <= 0) {
        return false;
    }
    rgb.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); // rows are not padded to 4 bytes
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    return glGetError() == GL_NO_ERROR;
}

bool App::saveScreenshot(const std::string& requestedPath) {
    std::string path = requestedPath;
    if (path.empty()) {
        std::error_code error;
        std::filesystem::create_directories("screenshots", error);
        path = "screenshots/roomcraft_" + timestamp() + ".bmp";
    }
    std::vector<unsigned char> rgb;
    int width = 0, height = 0;
    // The frame being shown was already swapped to the front; render a
    // fresh one into the back buffer first if this was called mid-frame.
    // (Called after ImGui rendered and before the swap, the back buffer
    // holds exactly the picture on screen.)
    const bool ok = readFramebuffer(rgb, width, height) && toCanvas(rgb, width, height, 1).saveBMP(path);
    setStatus(ok ? "Screenshot saved: " + path : "Could not save " + path);
    return ok;
}

namespace {

// The part of the framebuffer to record, in OpenGL pixels (origin bottom-left).
struct PixelRect {
    int x, y, width, height;
};

// Copies a rectangle out of a bottom-up RGB image.
std::vector<unsigned char> crop(const std::vector<unsigned char>& rgb, int width, const PixelRect& r) {
    std::vector<unsigned char> out(static_cast<std::size_t>(r.width) * static_cast<std::size_t>(r.height) * 3);
    for (int row = 0; row < r.height; ++row) {
        const std::size_t from = (static_cast<std::size_t>(r.y + row) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(r.x)) * 3;
        std::copy(rgb.begin() + static_cast<std::ptrdiff_t>(from),
                  rgb.begin() + static_cast<std::ptrdiff_t>(from + static_cast<std::size_t>(r.width) * 3),
                  out.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) * static_cast<std::size_t>(r.width) * 3));
    }
    return out;
}

} // namespace

void App::startRecording(const std::string& name) {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    recordWidth_ = width;
    recordHeight_ = height;
    int frameWidth = width, frameHeight = height;
    if (recordViewportOnly_) {
        int winWidth = 1, winHeight = 1;
        glfwGetWindowSize(window_, &winWidth, &winHeight);
        frameWidth = static_cast<int>(viewW_ * static_cast<float>(width) / static_cast<float>(winWidth));
        frameHeight = static_cast<int>(viewH_ * static_cast<float>(height) / static_cast<float>(winHeight));
    }
    const std::string fileName = name.empty() ? "roomcraft_" + timestamp() : name;
    recorder_ = std::make_unique<output::AnimationWriter>(
        recordDirectory_, fileName, std::max(frameWidth / recordDownscale_, 1), std::max(frameHeight / recordDownscale_, 1),
        static_cast<int>(std::lround(1.0f / recordInterval_)));
    // Lit 3D frames have thousands of colours; each frame is dithered to a
    // 252-colour palette first (ditherForGif), which the writer then stores
    // exactly.
    recordAccumulator_ = recordInterval_; // capture the very next frame
    recordSeconds_ = 0.0f;
    setStatus("Recording " + recorder_->path() + " ...");
}

void App::stopRecording() {
    if (!recorder_) {
        return;
    }
    const bool ok = recorder_->finish();
    const std::string path = recorder_->path();
    const int frames = recorder_->frameCount();
    recorder_.reset();
    setStatus(ok ? "Saved " + path + " (" + std::to_string(frames) + " frames)" : "Could not write " + path);
}

void App::recordFrame(float dt) {
    if (!recorder_) {
        return;
    }
    recordSeconds_ += dt;
    recordAccumulator_ += dt;
    if (recordAccumulator_ < recordInterval_) {
        return;
    }
    recordAccumulator_ = std::fmod(recordAccumulator_, recordInterval_);
    std::vector<unsigned char> rgb;
    int width = 0, height = 0;
    if (!readFramebuffer(rgb, width, height) || width != recordWidth_ || height != recordHeight_) {
        stopRecording();
        setStatus("Recording stopped (the window was resized)");
        return;
    }
    if (recordViewportOnly_) {
        int winWidth = 1, winHeight = 1;
        glfwGetWindowSize(window_, &winWidth, &winHeight);
        const float sx = static_cast<float>(width) / static_cast<float>(winWidth);
        const float sy = static_cast<float>(height) / static_cast<float>(winHeight);
        PixelRect r{static_cast<int>(viewX_ * sx), 0, static_cast<int>(viewW_ * sx), static_cast<int>(viewH_ * sy)};
        r.y = height - static_cast<int>(viewY_ * sy) - r.height;
        r.x = std::clamp(r.x, 0, width - 1);
        r.y = std::clamp(r.y, 0, height - 1);
        r.width = std::min(r.width, width - r.x);
        r.height = std::min(r.height, height - r.y);
        rgb = crop(rgb, width, r);
        width = r.width;
        height = r.height;
    }
    raster::Canvas frame = toCanvas(rgb, width, height, recordDownscale_);
    ditherForGif(frame);
    recorder_->addFrame(frame);
    if (recordSeconds_ > 30.0f) {
        stopRecording();
    }
}

// ---- Scripted capture -----------------------------------------------------------------------
//
// Drives the app through a fixed tour on a fixed 60 Hz clock and saves what
// the window shows: the screenshots and GIF in docs/images/cp3 come from
// here, so they can be regenerated after any change. Returns false when
// finished.

bool App::captureStep(int f) {
    const std::string& dir = options_.captureDirectory;
    const auto findItem = [this](const char* typeId) {
        for (const editor::Item& item : editor_.layout().items) {
            if (item.typeId == typeId) {
                return item.id;
            }
        }
        return -1;
    };
    const auto shot = [&](const char* name) {
        if (!saveScreenshot(dir + "/" + name)) {
            captureFailed_ = true;
        }
        std::printf("  %s/%s\n", dir.c_str(), name);
    };

    if (f == 0) {
        std::error_code error;
        std::filesystem::create_directories(dir, error);
        std::printf("RoomCraft scripted capture -> %s\n", dir.c_str());
        frameRoom(false);
        editor_.select(findItem("sofa_l"));
    }
    if (f == 15) shot("cp3_plan.bmp");
    if (f == 16) rig_.set3D(true);
    if (f == 16 + 24) shot("cp3_transition.bmp"); // half way through the 0.8 s transition
    if (f == 90) shot("cp3_3d.bmp");
    if (f == 91) {
        rig_.azimuth3D = -38.0f;
        rig_.elevation3D = 32.0f;
        editor_.select(findItem("bed_double"));
        light_.sunElevation = 14.0f;
        light_.sunAzimuth = 235.0f;
        light_.sunIntensity = 0.7f;
        light_.sunColor[0] = 1.0f, light_.sunColor[1] = 0.62f, light_.sunColor[2] = 0.35f;
        light_.ambient = 0.28f;
        light_.lampOn = true;
        light_.lampIntensity = 0.6f;
    }
    if (f == 100) shot("cp3_evening.bmp");
    if (f == 101) {
        light_ = LightingSettings();
        rig_.azimuth3D = 30.0f;
        rig_.elevation3D = 38.0f;
        rig_.set3D(false);
    }
    if (f == 160) {
        // Layout checks: a wardrobe pushed through the wall, a chair into
        // the sofa.
        editor_.keepInsideRoom = false;
        const int wardrobe = findItem("wardrobe");
        editor::Item item = *editor_.find(wardrobe);
        item.pose.position.x = -12.0f;
        editor_.updateItem(item);
        const int plant = findItem("plant");
        item = *editor_.find(plant);
        item.pose.position = {460, 400};
        editor_.updateItem(item);
        editor_.select(wardrobe);
    }
    if (f == 170) shot("cp3_checks.bmp");
    if (f == 171) {
        editor_.undo();
        editor_.undo();
        editor_.keepInsideRoom = true;
        editor_.select(findItem("coffee_table"));
        showSoftware_ = true;
    }
    if (f == 180) shot("cp3_software.bmp");
    // ---- Interaction check -------------------------------------------------
    // Real input events, injected into ImGui's queue exactly as the GLFW
    // callbacks would, then the result is checked. Exercises the whole chain:
    // mouse -> ray -> pick -> Editor drag, keyboard shortcuts, orbit, zoom.
    ImGuiIO& io = ImGui::GetIO();
    static int tableId = -1;
    static raster::Point tableStart{};
    static ImVec2 grab{};
    static float azimuthBefore = 0.0f, heightBefore = 0.0f;
    const auto check = [this](bool ok, const char* what) {
        std::printf("  %s: %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok) {
            captureFailed_ = true;
        }
    };
    if (f == 181) {
        showSoftware_ = false;
        editor_.select(-1);
        tableId = findItem("coffee_table");
        tableStart = editor_.find(tableId)->pose.position;
        // Screen position of the table's centre: world -> NDC -> window pixels.
        float x = 0, y = 0;
        editor::projectToNdc(cameraState(), {tableStart.x, 0.0f, tableStart.y}, x, y);
        grab = ImVec2(viewX_ + (x + 1.0f) / 2.0f * viewW_, viewY_ + (1.0f - y) / 2.0f * viewH_);
        io.AddMousePosEvent(grab.x, grab.y);
    }
    if (f == 183) io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    if (f >= 185 && f <= 194) io.AddMousePosEvent(grab.x + 10.0f * static_cast<float>(f - 184), grab.y);
    if (f == 196) io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    if (f == 198) {
        const editor::Item* table = editor_.find(tableId);
        // 100 pixels in the plan = 100 * (visible cm / viewport pixels),
        // then snapped to the 10 cm grid.
        const float expected = 100.0f * rig_.viewHeight / viewH_;
        const float moved = table->pose.position.x - tableStart.x;
        check(editor_.selectedId() == tableId, "click selects the coffee table");
        check(std::fabs(moved - expected) <= 10.0f && std::fabs(table->pose.position.y - tableStart.y) <= 10.0f,
              "dragging 100 px moves it by the matching distance in cm");
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_Z, true);
    }
    if (f == 199) {
        io.AddKeyEvent(ImGuiKey_Z, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
    }
    if (f == 201) {
        const editor::Item* table = editor_.find(tableId);
        check(table->pose.position.x == tableStart.x && table->pose.position.y == tableStart.y,
              "Ctrl+Z undoes the whole drag in one step");
        io.AddKeyEvent(ImGuiKey_Tab, true);
    }
    if (f == 202) io.AddKeyEvent(ImGuiKey_Tab, false);
    if (f == 260) {
        check(rig_.is3D() && rig_.blend == 1.0f, "Tab switches to 3D and the transition completes");
        azimuthBefore = rig_.azimuth3D;
        io.AddMousePosEvent(viewX_ + viewW_ / 2.0f, viewY_ + viewH_ / 2.0f);
    }
    if (f == 262) io.AddMouseButtonEvent(ImGuiMouseButton_Right, true);
    if (f >= 264 && f <= 273) {
        io.AddMousePosEvent(viewX_ + viewW_ / 2.0f + 12.0f * static_cast<float>(f - 263), viewY_ + viewH_ / 2.0f);
    }
    if (f == 275) io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
    if (f == 277) {
        check(std::fabs((rig_.azimuth3D - azimuthBefore) - (-120.0f * 0.3f)) < 0.5f,
              "right-drag of 120 px orbits the camera by 36 degrees");
        heightBefore = rig_.viewHeight;
        io.AddMouseWheelEvent(0.0f, 2.0f);
    }
    static ImVec2 floorPixel{};
    static unsigned revisionBefore = 0;
    if (f == 279) {
        check(std::fabs(rig_.viewHeight - heightBefore * 0.88f * 0.88f) < 1.0f, "two wheel clicks zoom in by 0.88^2");
        // Find a pixel showing empty floor: its ray hits no piece and meets
        // the floor inside the room.
        const editor::CameraState s = cameraState();
        bool found = false;
        for (float py = 0.55f; py < 0.95f && !found; py += 0.05f) {
            for (float px = 0.1f; px < 0.9f && !found; px += 0.05f) {
                const editor::Ray ray = editor::rayThroughNdc(s, px * 2.0f - 1.0f, 1.0f - py * 2.0f);
                xform::Vec3 hit{};
                const editor::Room& room = editor_.layout().room;
                if (editor::pickItem(editor_.layout(), ray).index < 0 && editor::intersectHorizontalPlane(ray, 0.0f, hit) &&
                    hit.x > 20.0f && hit.x < room.width - 20.0f && hit.z > 20.0f && hit.z < room.depth - 20.0f) {
                    floorPixel = ImVec2(viewX_ + px * viewW_, viewY_ + py * viewH_);
                    found = true;
                }
            }
        }
        check(found, "found empty floor to drag on");
        editor_.select(-1);
        io.AddMousePosEvent(floorPixel.x, floorPixel.y);
    }
    if (f == 281) {
        azimuthBefore = rig_.azimuth3D;
        revisionBefore = editor_.revision();
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    }
    if (f >= 283 && f <= 292) io.AddMousePosEvent(floorPixel.x - 12.0f * static_cast<float>(f - 282), floorPixel.y);
    if (f == 294) io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    if (f == 296) {
        std::printf("  (left-drag turned the view by %.2f degrees)\n", rig_.azimuth3D - azimuthBefore);
        check(std::fabs((rig_.azimuth3D - azimuthBefore) - 120.0f * 0.3f) < 0.5f,
              "left-drag of 120 px on empty floor rotates the 3D view by 36 degrees");
        check(editor_.revision() == revisionBefore && editor_.selectedId() == -1,
              "... and moves no furniture and selects nothing");
        azimuthBefore = rig_.azimuth3D;
        io.AddKeyEvent(ImGuiKey_Q, true);
    }
    if (f == 297) io.AddKeyEvent(ImGuiKey_Q, false);
    if (f == 330) {
        check(std::fabs((rig_.azimuth3D - azimuthBefore) - 15.0f) < 0.01f && !rig_.orbitGliding,
              "Q turns the view 15 degrees, gliding there");
        check(ImGui::FindWindowByName("##camera") != nullptr, "the on-screen rotate control is shown");
        azimuthBefore = rig_.azimuth3D;
        rig_.autoRotate = true;
    }
    if (f == 360) {
        check(std::fabs((rig_.azimuth3D - azimuthBefore) - rig_.autoRotateSpeed * 0.5f) < 0.05f,
              "auto-rotate turns at its set speed");
        rig_.autoRotate = false;
        rig_.azimuth3D = rig_.azimuthGoal = 30.0f;
        rig_.set3D(false);
        frameRoom(false);
    }

    // ---- GIF: plan -> 3D, a third of an orbit, back to the plan -------------
    const int r = 420; // recording starts here
    if (f == r) {
        editor_.select(-1);
        recordDownscale_ = 2;
        recordInterval_ = 0.1f;
        recordDirectory_ = dir;
        recordViewportOnly_ = true;
        startRecording("cp3_tour");
    }
    if (f == r + 20) rig_.set3D(true);
    if (f > r + 80 && f <= r + 260) rig_.azimuth3D = 30.0f - (static_cast<float>(f - r - 80) / 180.0f) * 120.0f;
    if (f == r + 280) rig_.set3D(false);
    if (f >= r && f < r + 360) recordFrame(1.0f / 60.0f);
    if (f == r + 360) {
        stopRecording();
        std::printf("  %s/cp3_tour.gif\n", dir.c_str());
        return false;
    }
    return true;
}

} // namespace app
