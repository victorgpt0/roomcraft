#pragma once

#include "editor/camera_rig.h"
#include "editor/editor.h"
#include "gfx/renderer.h"
#include "output/animation_writer.h"

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

// The RoomCraft interactive prototype (Checkpoint 3).
//
// One window. Around the edges, Dear ImGui panels; in the middle, the room
// drawn by gfx::Renderer, as a 2D plan or in 3D.
//
//   app.cpp            window, main loop, mouse and keyboard, labels
//   ui.cpp             the panels: menu, catalogue, room, lighting, inspector
//   capture.cpp        screenshots (BMP) and recordings (GIF) of the window,
//                      through the Checkpoint 1 canvas and the CP2 GIF writer
//   software_view.cpp  the same layout drawn by the Checkpoint 1/2 software
//                      rasterizer, shown in a panel for comparison
//
// Every change to the layout goes through editor::Editor, so undo/redo
// works the same for the mouse, the keyboard and the panels.

namespace app {

struct Options {
    std::string captureDirectory; // non-empty: run the scripted capture and exit
    int width = 1440;
    int height = 900;
};

struct LightingSettings {
    float sunAzimuth = 205.0f;   // degrees; the window is in the top (-Z) wall
    float sunElevation = 50.0f;
    float sunIntensity = 0.75f;
    float sunColor[3] = {1.0f, 0.96f, 0.88f};
    float ambient = 0.40f;
    bool lampOn = false;
    float lampIntensity = 1.0f;
    float lampColor[3] = {1.0f, 0.84f, 0.6f};
    float shininess = 28.0f;
    float specular = 0.18f;
};

class App {
public:
    explicit App(Options options);
    ~App();

    // Opens the window and runs until it is closed. Returns the exit code.
    int run();

private:
    // ---- app.cpp ----
    bool initWindow();
    void shutdown();
    void frame(float dt);
    void handleMouse();
    void handleKeyboard();
    void updateViewport();
    void drawLabels();
    editor::CameraState cameraState() const;
    bool mouseRay(editor::Ray& ray) const;
    bool floorPointUnderMouse(float height, xform::Vec3& hit) const;
    int pickUnderMouse(xform::Vec3* hitPoint) const;
    gfx::Lighting lighting() const;
    void setStatus(const std::string& message);
    void frameRoom(bool animate = true);

    // ---- ui.cpp ----
    void drawUi();
    void drawMenuBar();
    void drawLeftPanel();
    void drawRightPanel();
    void drawStatusBar();
    void drawHelpWindow();
    void drawCameraOverlay();
    void turnCamera(float turnDegrees, float tiltDegrees);
    void drawFileDialogs();
    void applyTheme(float scale);

    // ---- capture.cpp ----
    bool readFramebuffer(std::vector<unsigned char>& rgb, int& width, int& height) const;
    bool saveScreenshot(const std::string& path);
    void startRecording(const std::string& name = "");
    void stopRecording();
    void recordFrame(float dt);
    bool captureStep(int frameIndex);

    // ---- software_view.cpp ----
    void drawSoftwareWindow();
    void releaseSoftwareView();

    Options options_;
    GLFWwindow* window_ = nullptr;

    editor::Editor editor_;
    editor::CameraRig rig_;
    std::unique_ptr<gfx::Renderer> renderer_;
    gfx::RenderOptions renderOptions_;
    LightingSettings light_;

    // Layout of the screen (window coordinates, y down).
    float uiScale_ = 1.0f;
    float menuHeight_ = 0.0f;
    float leftWidth_ = 0.0f;
    float rightWidth_ = 0.0f;
    float statusHeight_ = 0.0f;
    float viewX_ = 0, viewY_ = 0, viewW_ = 1, viewH_ = 1;

    // Mouse interaction.
    int hoverId_ = -1;
    bool orbiting_ = false;
    int orbitButton_ = 1;          // which mouse button is orbiting (0 left, 1 right)
    bool leftOrbitPending_ = false; // left press on empty floor in 3D: a click or an orbit?
    float pressX_ = 0, pressY_ = 0;
    bool panning_ = false;
    xform::Vec3 panGrab_{};
    float dragPlaneHeight_ = 0.0f;
    float lastMouseX_ = 0, lastMouseY_ = 0;

    // Panels and windows.
    bool showLabels_ = true;
    bool showHelp_ = false;
    bool showSoftware_ = false;
    bool showImGuiDemo_ = false;
    bool openSavePopup_ = false;
    bool openOpenPopup_ = false;
    char fileName_[128] = "my_room";
    std::string currentFile_;
    std::string status_;
    double statusTime_ = 0.0;

    // Software view texture.
    unsigned softwareTexture_ = 0;
    unsigned softwareRevision_ = 0;
    int softwareSelected_ = -2;

    // Recording.
    std::unique_ptr<output::AnimationWriter> recorder_;
    float recordAccumulator_ = 0.0f;
    float recordSeconds_ = 0.0f;
    int recordWidth_ = 0, recordHeight_ = 0;
    int recordDownscale_ = 2;   // the GIF is the window shrunk by this factor
    float recordInterval_ = 0.05f; // seconds between GIF frames (20 fps)
    std::string recordDirectory_ = "recordings";
    bool recordViewportOnly_ = false; // crop to the room view (used for the documentation GIF)

    // Scripted capture (--capture).
    int captureFrame_ = 0;
    bool captureFailed_ = false;
};

} // namespace app
