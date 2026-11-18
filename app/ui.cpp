// The panels of the app, drawn with Dear ImGui every frame.
//
// ImGui is an "immediate mode" UI: there are no widget objects to create
// and keep in sync. Every frame we simply call ImGui::Button("Delete"),
// and it returns true on the frame it is clicked. The panels below read the
// editor's state and call the editor's operations directly.

#include "app.h"

#include "editor/catalogue.h"
#include "editor/layout.h"
#include "gfx/gl.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace app {

namespace {

// The user's own layouts, next to wherever the app is run from (git-ignored),
// and the examples that come with the project.
const char* kLayoutFolder = "my_layouts";
#ifndef ROOMCRAFT_EXAMPLES_DIR
#define ROOMCRAFT_EXAMPLES_DIR "layouts"
#endif
const ImVec4 kAccent{0.95f, 0.55f, 0.12f, 1.0f};
const ImVec4 kRed{0.95f, 0.30f, 0.28f, 1.0f};
const ImVec4 kAmber{0.98f, 0.75f, 0.20f, 1.0f};
const ImVec4 kGreen{0.45f, 0.82f, 0.45f, 1.0f};
const ImVec4 kMuted{0.62f, 0.64f, 0.68f, 1.0f};

// Fixed side panels: no title bar, cannot be moved or resized, but they keep
// their scroll bar for when the window is short.
constexpr ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

ImVec4 toImVec4(raster::Color c) {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f);
}

void toFloat3(raster::Color c, float out[3]) {
    out[0] = c.r / 255.0f;
    out[1] = c.g / 255.0f;
    out[2] = c.b / 255.0f;
}

raster::Color fromFloat3(const float c[3]) {
    const auto channel = [](float v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return raster::Color{channel(c[0]), channel(c[1]), channel(c[2])};
}

void sectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void helpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Full paths of the .rclayout files in a folder, sorted.
std::vector<std::string> listLayouts(const std::string& folder) {
    std::vector<std::string> paths;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
        if (entry.path().extension() == ".rclayout") {
            paths.push_back(entry.path().string());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

} // namespace

// ---- Theme ----------------------------------------------------------------------------------

void App::applyTheme(float scale) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.FramePadding = ImVec2(7, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.WindowBorderSize = 0.0f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.16f, 1.0f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.95f, 0.55f, 0.12f, 0.30f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.95f, 0.55f, 0.12f, 0.45f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.95f, 0.55f, 0.12f, 0.60f);
    c[ImGuiCol_Button] = ImVec4(0.22f, 0.24f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.95f, 0.55f, 0.12f, 0.55f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.95f, 0.55f, 0.12f, 0.80f);
    c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.24f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.27f, 0.31f, 1.0f);
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.68f, 0.30f, 1.0f);
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_Separator] = ImVec4(0.26f, 0.27f, 0.30f, 1.0f);
    style.ScaleAllSizes(scale);

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font;
    font.SizePixels = std::round(13.0f * scale * 1.15f);
    font.OversampleH = 2;
    io.Fonts->Clear();
    io.Fonts->AddFontDefault(&font);
}

// ---- Whole UI ------------------------------------------------------------------------------

void App::drawUi() {
    drawMenuBar();
    drawStatusBar();
    drawLeftPanel();
    drawRightPanel();
    drawFileDialogs();
    if (showHelp_) {
        drawHelpWindow();
    }
    drawCameraOverlay();
    if (showSoftware_) {
        drawSoftwareWindow();
    }
    if (showImGuiDemo_) {
        ImGui::ShowDemoWindow(&showImGuiDemo_);
    }
}

void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    menuHeight_ = ImGui::GetWindowSize().y;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New empty room")) {
            editor::Layout empty;
            empty.room = editor_.layout().room;
            editor_.replaceLayout(empty);
            currentFile_.clear();
            setStatus("New empty room (Ctrl+Z to undo)");
        }
        if (ImGui::MenuItem("Sample room")) {
            editor_.replaceLayout(editor::sampleLayout());
            currentFile_.clear();
            frameRoom();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Open...", "Ctrl+O")) {
            openOpenPopup_ = true;
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            openSavePopup_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Screenshot (BMP)", "F12")) {
            saveScreenshot("");
        }
        if (ImGui::MenuItem(recorder_ ? "Stop recording" : "Record GIF", nullptr, recorder_ != nullptr)) {
            recorder_ ? stopRecording() : startRecording();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editor_.canUndo())) {
            editor_.undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editor_.canRedo())) {
            editor_.redo();
        }
        ImGui::Separator();
        const bool any = editor_.selected() != nullptr;
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, any)) {
            editor_.duplicateSelected();
        }
        if (ImGui::MenuItem("Rotate", "R", false, any)) {
            editor_.rotateSelected(editor_.rotationStep);
        }
        if (ImGui::MenuItem("Delete", "Del", false, any)) {
            editor_.removeSelected();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("2D plan", "Tab", !rig_.is3D())) {
            rig_.set3D(false);
        }
        if (ImGui::MenuItem("3D view", "Tab", rig_.is3D())) {
            rig_.set3D(true);
        }
        if (ImGui::MenuItem("Frame the room", "F")) {
            frameRoom();
        }
        ImGui::Separator();
        ImGui::MenuItem("Labels", nullptr, &showLabels_);
        ImGui::MenuItem("Grid", nullptr, &renderOptions_.grid);
        ImGui::MenuItem("Software renderer (CP1/CP2)", nullptr, &showSoftware_);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("Controls", "F1", &showHelp_);
        ImGui::MenuItem("ImGui demo window", nullptr, &showImGuiDemo_);
        ImGui::EndMenu();
    }

    // View switch in the middle of the menu bar.
    const float width = 190.0f * uiScale_;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + 20.0f, (ImGui::GetWindowWidth() - width) / 2.0f));
    const auto modeButton = [this](const char* label, bool active, bool to3D) {
        ImGui::PushStyleColor(ImGuiCol_Button, active ? kAccent : ImGui::GetStyleColorVec4(ImGuiCol_Button));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.1f, 0.1f, 0.1f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_Text));
        if (ImGui::SmallButton(label)) {
            rig_.set3D(to3D);
        }
        ImGui::PopStyleColor(2);
    };
    modeButton("  2D plan  ", !rig_.is3D(), false);
    modeButton("  3D view  ", rig_.is3D(), true);
    if (recorder_) {
        ImGui::SameLine();
        ImGui::TextColored(kRed, "  REC %.1f s", recordSeconds_);
    }
    ImGui::EndMainMenuBar();
}

void App::drawStatusBar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    statusHeight_ = ImGui::GetFrameHeight() + 6.0f * uiScale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - statusHeight_));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, statusHeight_));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * uiScale_, 3.0f * uiScale_));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::AlignTextToFramePadding();
    char right[200];
    const int problems = editor_.problemCount();
    std::snprintf(right, sizeof(right), "%s  |  %zu pieces  |  floor used %.0f%%  |  %s  |  %.0f fps",
                  rig_.is3D() ? "3D" : "2D", editor_.layout().items.size(),
                  editor::floorCoverage(editor_.layout()) * 100.0f,
                  problems == 0 ? "no problems" : (std::to_string(problems) + " problem(s)").c_str(),
                  ImGui::GetIO().Framerate);
    const float w = ImGui::CalcTextSize(right).x;
    const float rightX = ImGui::GetWindowWidth() - w - 12.0f * uiScale_;

    // Left: the latest message (or a reminder of the controls), shortened
    // from the front so it never runs into the statistics on the right.
    std::string left;
    if (!status_.empty() && glfwGetTime() - statusTime_ < 8.0) {
        left = status_;
    } else {
        left = rig_.is3D() ? std::string("Drag a piece: move   Drag empty floor / right-drag: rotate view   Q/E: turn   "
                                         "Wheel: zoom   Tab: 2D   F1: help")
                           : std::string("Drag: move   Right-drag: pan   Wheel: zoom   R: rotate piece   "
                                         "Tab: 3D   F1: help");
    }
    const float room = rightX - ImGui::GetCursorPosX() - 24.0f * uiScale_;
    while (left.size() > 4 && ImGui::CalcTextSize(left.c_str()).x > room) {
        left = "..." + left.substr(std::min<std::size_t>(left.size(), 8));
    }
    ImGui::TextColored(left == status_ ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : kMuted, "%s", left.c_str());
    ImGui::SameLine(std::max(rightX, ImGui::GetCursorPosX() + 20.0f));
    ImGui::TextColored(problems == 0 ? kMuted : kAmber, "%s", right);
    ImGui::End();
    ImGui::PopStyleVar();
}

// ---- Left panel: catalogue, room, view, lighting ------------------------------------------------

void App::drawLeftPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    leftWidth_ = 270.0f * uiScale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + menuHeight_));
    ImGui::SetNextWindowSize(ImVec2(leftWidth_, vp->Size.y - menuHeight_ - statusHeight_));
    ImGui::Begin("##left", nullptr, kPanelFlags);

    // Four tabs keep each page short enough to fit without scrolling.
    if (!ImGui::BeginTabBar("##lefttabs")) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabItem("Furniture")) {
    ImGui::TextColored(kMuted, "Click to add to the room");
    std::string category;
    const float swatch = ImGui::GetFrameHeight() * 0.55f;
    for (const editor::FurnitureType& type : editor::catalogue()) {
        if (type.category != category) {
            category = type.category;
            ImGui::Spacing();
            ImGui::TextDisabled("%s", category.c_str());
        }
        ImGui::PushID(type.id.c_str());
        // A button with a swatch of the type's default colour at its left.
        const ImVec2 corner = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        const std::string label = std::string("     ") + type.name;
        const bool clicked = ImGui::Button(label.c_str(), ImVec2(-1.0f, 0.0f));
        ImGui::PopStyleVar();
        const float pad = (ImGui::GetFrameHeight() - swatch) / 2.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(corner.x + pad + 2.0f, corner.y + pad),
                                                  ImVec2(corner.x + pad + 2.0f + swatch, corner.y + pad + swatch),
                                                  ImGui::GetColorU32(toImVec4(type.color)), 3.0f);
        if (clicked) {
            // Add at the centre of the view, kept inside the room.
            const editor::Room& room = editor_.layout().room;
            const raster::Point at{std::clamp(rig_.target.x, 0.0f, room.width), std::clamp(rig_.target.z, 0.0f, room.depth)};
            editor_.addItem(type.id, at);
            setStatus("Added " + type.name + " - drag it into place");
        }
        if (ImGui::BeginItemTooltip()) {
            const raster::Rect box = raster::computeBoundingBox(type.footprint);
            ImGui::Text("%s: %.0f x %.0f cm, %.0f cm high", type.name.c_str(), box.xmax - box.xmin, box.ymax - box.ymin,
                        type.height);
            ImGui::TextDisabled("%zu parts in its 3D model", type.parts.size());
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Room")) {
        editor::Room room = editor_.layout().room;
        bool changed = false;
        ImGui::Spacing();
        ImGui::PushItemWidth(-90.0f * uiScale_);
        changed |= ImGui::DragFloat("Width", &room.width, 2.0f, 200.0f, 1500.0f, "%.0f cm");
        changed |= ImGui::DragFloat("Depth", &room.depth, 2.0f, 200.0f, 1500.0f, "%.0f cm");
        changed |= ImGui::DragFloat("Wall height", &room.wallHeight, 1.0f, 200.0f, 400.0f, "%.0f cm");
        float floorColor[3], wallColor[3];
        toFloat3(room.floorColor, floorColor);
        toFloat3(room.wallColor, wallColor);
        if (ImGui::ColorEdit3("Floor", floorColor, ImGuiColorEditFlags_NoInputs)) {
            room.floorColor = fromFloat3(floorColor);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::ColorEdit3("Walls", wallColor, ImGuiColorEditFlags_NoInputs)) {
            room.wallColor = fromFloat3(wallColor);
            changed = true;
        }
        ImGui::PopItemWidth();
        if (changed) {
            room.width = std::clamp(room.width, 200.0f, 1500.0f);
            room.depth = std::clamp(room.depth, 200.0f, 1500.0f);
            room.wallHeight = std::clamp(room.wallHeight, 200.0f, 400.0f);
            editor_.beginGesture();
            editor_.setRoom(room);
        }
        ImGui::TextColored(kMuted, "Floor area %.1f m2", room.width * room.depth / 10000.0f);
        ImGui::Spacing();
        ImGui::TextWrapped("The room is a rectangle. The door (bottom wall) and the window (top wall) move with it.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("View")) {
        if (ImGui::Button("Frame the room (F)", ImVec2(-1.0f, 0.0f))) {
            frameRoom();
        }
        ImGui::Checkbox("Snap to grid", &editor_.snapToGrid);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        const float sizes[] = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f};
        const char* names[] = {"1 cm", "5 cm", "10 cm", "25 cm", "50 cm"};
        int current = 2;
        for (int i = 0; i < 5; ++i) {
            if (std::fabs(sizes[i] - editor_.gridSize) < 0.01f) {
                current = i;
            }
        }
        if (ImGui::Combo("##snap", &current, names, 5)) {
            editor_.gridSize = sizes[current];
        }
        ImGui::Checkbox("Keep furniture inside the walls", &editor_.keepInsideRoom);
        helpMarker("Dragging stops at the walls. Turn it off to see the fit check flag a piece in red.");
        ImGui::Checkbox("Grid", &renderOptions_.grid);
        ImGui::SameLine();
        ImGui::Checkbox("Labels", &showLabels_);
        ImGui::Checkbox("Cutaway walls", &renderOptions_.cutawayWalls);
        helpMarker("In 3D, walls between the camera and the room are cut down so you can see inside.");
        ImGui::SameLine();
        ImGui::Checkbox("Shadows", &renderOptions_.shadows);
        ImGui::Checkbox("Wireframe", &renderOptions_.wireframe);
        ImGui::SameLine();
        ImGui::Checkbox("Back-face culling", &renderOptions_.backFaceCulling);
        ImGui::SeparatorText("3D camera");
        ImGui::PushItemWidth(-110.0f * uiScale_);
        // The sliders show the angle the camera is heading to, so they do
        // not jitter while it glides.
        float turn = std::fmod(rig_.orbitGliding ? rig_.azimuthGoal : rig_.azimuth3D, 360.0f);
        if (turn < 0.0f) {
            turn += 360.0f;
        }
        float tilt = rig_.orbitGliding ? rig_.elevationGoal : rig_.elevation3D;
        const bool turned = ImGui::SliderFloat("Turn", &turn, 0.0f, 360.0f, "%.0f deg");
        const bool tilted = ImGui::SliderFloat("Tilt", &tilt, editor::CameraRig::kMinElevation,
                                               editor::CameraRig::kMaxElevation, "%.0f deg");
        if (turned || tilted) {
            rig_.set3D(true);
            rig_.autoRotate = false;
            rig_.orbitGliding = false;
            rig_.azimuth3D = rig_.azimuthGoal = turn;
            rig_.elevation3D = rig_.elevationGoal = tilt;
        }
        ImGui::Checkbox("Auto-rotate", &rig_.autoRotate);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat("##spin", &rig_.autoRotateSpeed, -60.0f, 60.0f, "%.0f deg/s");
        if (ImGui::Button("Reset view angle", ImVec2(-1.0f, 0.0f))) {
            rig_.set3D(true);
            rig_.resetOrbit();
        }
        ImGui::SliderFloat("Field of view", &rig_.fov3D, 20.0f, 90.0f, "%.0f deg");
        ImGui::SliderFloat("Transition", &rig_.transitionSeconds, 0.1f, 3.0f, "%.1f s");
        ImGui::PopItemWidth();
        ImGui::Checkbox("Software renderer window", &showSoftware_);
        helpMarker("The same layout drawn by our Checkpoint 1/2 software rasterizer (scanline fill, Bresenham, "
                   "window-to-viewport camera), for comparison with OpenGL.");
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Light")) {
        if (ImGui::Button("Day")) {
            light_ = LightingSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Evening")) {
            light_.sunElevation = 14.0f;
            light_.sunAzimuth = 235.0f;
            light_.sunIntensity = 0.7f;
            light_.sunColor[0] = 1.0f, light_.sunColor[1] = 0.62f, light_.sunColor[2] = 0.35f;
            light_.ambient = 0.28f;
            light_.lampOn = true;
            light_.lampIntensity = 0.6f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Night")) {
            light_.sunIntensity = 0.0f;
            light_.ambient = 0.12f;
            light_.lampOn = true;
            light_.lampIntensity = 1.3f;
        }
        ImGui::PushItemWidth(-100.0f * uiScale_);
        ImGui::TextDisabled("Sun (directional light)");
        ImGui::SliderFloat("Direction", &light_.sunAzimuth, 0.0f, 360.0f, "%.0f deg");
        ImGui::SliderFloat("Height", &light_.sunElevation, 5.0f, 90.0f, "%.0f deg");
        ImGui::SliderFloat("Strength", &light_.sunIntensity, 0.0f, 1.5f);
        ImGui::ColorEdit3("Sun colour", light_.sunColor, ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Ambient", &light_.ambient, 0.0f, 1.0f);
        ImGui::TextDisabled("Ceiling lamp (point light)");
        ImGui::Checkbox("Lamp on", &light_.lampOn);
        ImGui::SliderFloat("Lamp strength", &light_.lampIntensity, 0.0f, 2.5f);
        ImGui::ColorEdit3("Lamp colour", light_.lampColor, ImGuiColorEditFlags_NoInputs);
        ImGui::TextDisabled("Material");
        ImGui::SliderFloat("Shininess", &light_.shininess, 2.0f, 128.0f, "%.0f");
        ImGui::SliderFloat("Specular", &light_.specular, 0.0f, 1.0f);
        ImGui::PopItemWidth();
        ImGui::TextColored(kMuted, "Lighting shows in the 3D view.");
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    ImGui::End();
}

// ---- Right panel: inspector and checks --------------------------------------------------------

void App::drawRightPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    rightWidth_ = 300.0f * uiScale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - rightWidth_, vp->Pos.y + menuHeight_));
    ImGui::SetNextWindowSize(ImVec2(rightWidth_, vp->Size.y - menuHeight_ - statusHeight_));
    ImGui::Begin("##right", nullptr, kPanelFlags);

    sectionTitle("INSPECTOR");
    const editor::Item* selected = editor_.selected();
    if (selected == nullptr) {
        ImGui::TextWrapped("Click a piece of furniture to select it, or add one from the catalogue.");
        ImGui::Spacing();
        ImGui::TextColored(kMuted, "%zu pieces, %.0f%% of the floor used", editor_.layout().items.size(),
                           editor::floorCoverage(editor_.layout()) * 100.0f);
    } else {
        editor::Item item = *selected; // edit a copy, hand it back on change
        const editor::FurnitureType& type = editor::typeOf(item);
        const int index = editor_.indexOf(item.id);
        const editor::ItemStatus status = editor_.status()[static_cast<std::size_t>(index)];
        bool changed = false;

        ImGui::TextDisabled("%s  |  %s", type.name.c_str(), type.category.c_str());
        char name[96];
        std::snprintf(name, sizeof(name), "%s", item.name.c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##name", name, sizeof(name))) {
            item.name = name;
            changed = true;
        }

        ImGui::PushItemWidth(-80.0f * uiScale_);
        ImGui::TextDisabled("Translation");
        changed |= ImGui::DragFloat("x", &item.pose.position.x, 1.0f, -500.0f, 2000.0f, "%.0f cm");
        changed |= ImGui::DragFloat("y", &item.pose.position.y, 1.0f, -500.0f, 2000.0f, "%.0f cm");
        ImGui::TextDisabled("Rotation");
        changed |= ImGui::SliderFloat("angle", &item.pose.angleDegrees, 0.0f, 360.0f, "%.1f deg");
        ImGui::PopItemWidth();
        const float third = (ImGui::GetContentRegionAvail().x - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
        if (ImGui::Button("-90", ImVec2(third, 0))) {
            editor_.rotateSelected(-90.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("-15", ImVec2(third, 0))) {
            editor_.rotateSelected(-15.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("+90", ImVec2(third, 0))) {
            editor_.rotateSelected(90.0f);
        }

        ImGui::PushItemWidth(-80.0f * uiScale_);
        ImGui::TextDisabled("Scaling");
        float sx = std::fabs(item.pose.scaleX);
        const bool mirrored = item.pose.scaleX < 0.0f;
        if (ImGui::SliderFloat("width x", &sx, 0.3f, 2.5f, "%.2f")) {
            item.pose.scaleX = mirrored ? -sx : sx;
            changed = true;
        }
        changed |= ImGui::SliderFloat("depth x", &item.pose.scaleY, 0.3f, 2.5f, "%.2f");
        changed |= ImGui::SliderFloat("height x", &item.heightScale, 0.3f, 2.5f, "%.2f");
        ImGui::PopItemWidth();
        bool mirror = mirrored;
        if (ImGui::Checkbox("Mirrored (reflection)", &mirror)) {
            item.pose.scaleX = -item.pose.scaleX;
            changed = true;
        }
        helpMarker("A reflection across the piece's own vertical axis: its pose scaleX becomes negative. "
                   "Use it to turn a left-handed L-sofa into a right-handed one.");

        float color[3];
        toFloat3(item.color, color);
        ImGui::TextDisabled("Colour");
        if (ImGui::ColorEdit3("##colour", color)) {
            item.color = fromFloat3(color);
            changed = true;
        }
        if (ImGui::SmallButton("Reset colour")) {
            item.color = type.color;
            changed = true;
        }

        if (changed) {
            editor_.beginGesture(); // ends when the widget is released (App::frame)
            editor_.updateItem(item);
        }

        ImGui::Spacing();
        const raster::Rect box = raster::computeBoundingBox(type.footprint);
        ImGui::TextColored(kMuted, "Size %.0f x %.0f x %.0f cm", (box.xmax - box.xmin) * std::fabs(item.pose.scaleX),
                           (box.ymax - box.ymin) * std::fabs(item.pose.scaleY), type.height * item.heightScale);
        ImGui::TextColored(kMuted, "Floor area %.2f m2", raster::computeArea(editor::itemFootprint(item)) / 10000.0f);
        if (!status.fitsInRoom) {
            ImGui::TextColored(kRed, "Sticks through a wall");
        } else if (status.overlaps) {
            ImGui::TextColored(kAmber, "Collides with another piece (%.0f cm2)", status.overlapArea);
        } else {
            ImGui::TextColored(kGreen, "Fits - no collisions");
        }

        ImGui::Spacing();
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
        if (ImGui::Button("Duplicate", ImVec2(half, 0))) {
            editor_.duplicateSelected();
        }
        ImGui::SameLine();
        if (ImGui::Button("Bring to front", ImVec2(half, 0))) {
            editor_.bringToFront();
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.16f, 1.0f));
        if (ImGui::Button("Delete", ImVec2(-1.0f, 0))) {
            editor_.removeSelected();
        }
        ImGui::PopStyleColor();
    }

    sectionTitle("LAYOUT CHECKS");
    const editor::Layout& layout = editor_.layout();
    const std::vector<editor::ItemStatus>& status = editor_.status();
    int shown = 0;
    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        if (status[i].fitsInRoom && !status[i].overlaps) {
            continue;
        }
        ++shown;
        ImGui::PushID(static_cast<int>(i));
        const bool outside = !status[i].fitsInRoom;
        ImGui::PushStyleColor(ImGuiCol_Text, outside ? kRed : kAmber);
        const std::string label = layout.items[i].name + (outside ? " - through a wall" : " - collision");
        if (ImGui::Selectable(label.c_str(), layout.items[i].id == editor_.selectedId())) {
            editor_.select(layout.items[i].id);
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (shown == 0) {
        ImGui::TextColored(kGreen, "Everything fits. No collisions.");
    }
    ImGui::TextColored(kMuted, "Fit: Sutherland-Hodgman clipping (CP1).");
    ImGui::TextColored(kMuted, "Collisions: ear clipping + clipping,");
    ImGui::TextColored(kMuted, "per part, with height ranges.");

    sectionTitle("HISTORY");
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    ImGui::BeginDisabled(!editor_.canUndo());
    if (ImGui::Button("Undo", ImVec2(half, 0))) {
        editor_.undo();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!editor_.canRedo());
    if (ImGui::Button("Redo", ImVec2(half, 0))) {
        editor_.redo();
    }
    ImGui::EndDisabled();
    ImGui::End();
}

// ---- Camera control in the corner of the view -------------------------------------------------
//
// A small always-visible panel, so rotating the 3D camera does not depend on
// knowing the mouse shortcuts (or having a right mouse button). The arrow
// buttons repeat while held; each repeat adds a step, which the camera rig
// glides through smoothly.

void App::drawCameraOverlay() {
    const float margin = 10.0f * uiScale_;
    ImGui::SetNextWindowPos(ImVec2(viewX_ + viewW_ - margin, viewY_ + viewH_ - margin), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.82f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("##camera", nullptr, flags)) {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled(rig_.is3D() ? "Rotate view" : "Rotate (opens 3D)");

    const float cell = ImGui::GetFrameHeight();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    // Row 1: tilt up.
    ImGui::Dummy(ImVec2(cell, cell));
    ImGui::SameLine();
    if (ImGui::ArrowButton("##higher", ImGuiDir_Up)) {
        turnCamera(0.0f, 10.0f);
    }
    ImGui::SetItemTooltip("Look from higher (Page Up)");
    // Row 2: turn left, reset, turn right.
    if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
        turnCamera(15.0f, 0.0f);
    }
    ImGui::SetItemTooltip("Turn left (Q)");
    ImGui::SameLine();
    ImGui::PopItemFlag();
    if (ImGui::Button("o", ImVec2(cell, cell))) {
        if (!rig_.is3D()) {
            rig_.set3D(true);
        }
        rig_.resetOrbit();
    }
    ImGui::SetItemTooltip("Reset the view angle");
    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    ImGui::SameLine();
    if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
        turnCamera(-15.0f, 0.0f);
    }
    ImGui::SetItemTooltip("Turn right (E)");
    // Row 3: tilt down.
    ImGui::Dummy(ImVec2(cell, cell));
    ImGui::SameLine();
    if (ImGui::ArrowButton("##lower", ImGuiDir_Down)) {
        turnCamera(0.0f, -10.0f);
    }
    ImGui::SetItemTooltip("Look from lower (Page Down)");
    ImGui::PopItemFlag();

    // Turntable, to the right of the arrows.
    ImGui::SameLine(0.0f, gap * 3.0f);
    bool spin = rig_.autoRotate;
    if (ImGui::Checkbox("Auto-rotate", &spin)) {
        if (spin && !rig_.is3D()) {
            rig_.set3D(true);
        }
        rig_.autoRotate = spin;
    }
    ImGui::End();
}

// ---- Help and files ---------------------------------------------------------------------------

void App::drawHelpWindow() {
    ImGui::SetNextWindowSize(ImVec2(470.0f * uiScale_, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::Begin("Controls", &showHelp_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    const auto row = [](const char* keys, const char* action) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(kAccent, "%s", keys);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(action);
    };
    if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_RowBg)) {
        row("Left click", "Select a piece (click empty floor to deselect)");
        row("Left drag", "On a piece: move it (snaps to the grid)");
        row("", "On empty floor in 3D: rotate the view");
        row("Right drag", "3D: rotate the view   2D: pan");
        row("Q / E", "Turn the 3D view left / right");
        row("Page Up / Down", "Look from higher / lower");
        row("Arrows (nothing selected)", "Turn and tilt the 3D view");
        row("Corner buttons", "Rotate, reset the angle, auto-rotate");
        row("Middle drag", "Pan");
        row("Wheel", "Zoom towards the mouse");
        row("Shift + wheel", "Rotate the selected piece");
        row("R / Shift+R", "Rotate +/- 15 degrees");
        row("Arrows (piece selected)", "Nudge by one grid step (Shift: 1 cm)");
        row("Del / Backspace", "Delete");
        row("Ctrl+D", "Duplicate");
        row("Ctrl+Z / Ctrl+Y", "Undo / redo");
        row("Tab", "Switch between the 2D plan and 3D");
        row("F", "Frame the room");
        row("G", "Snap to grid on/off");
        row("Ctrl+S / Ctrl+O", "Save / open a layout");
        row("F12", "Screenshot");
        row("Esc", "Deselect");
        ImGui::EndTable();
    }
    ImGui::End();
}

void App::drawFileDialogs() {
    if (openSavePopup_) {
        ImGui::OpenPopup("Save layout");
        openSavePopup_ = false;
    }
    if (openOpenPopup_) {
        ImGui::OpenPopup("Open layout");
        openOpenPopup_ = false;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Save layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Saved in the \"%s\" folder as a .rclayout text file.", kLayoutFolder);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(320.0f * uiScale_);
        const bool enter = ImGui::InputText("Name", fileName_, sizeof(fileName_), ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("Save", ImVec2(120.0f * uiScale_, 0)) || enter) {
            std::string name = fileName_;
            for (char& ch : name) {
                if (ch == '/' || ch == '\\' || ch == ':' || ch == '.') {
                    ch = '_';
                }
            }
            if (!name.empty()) {
                std::error_code error;
                std::filesystem::create_directories(kLayoutFolder, error);
                const std::string path = std::string(kLayoutFolder) + "/" + name + ".rclayout";
                if (editor_.save(path)) {
                    currentFile_ = path;
                    setStatus("Saved " + path);
                } else {
                    setStatus("Could not save " + path);
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f * uiScale_, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Open layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto listing = [this](const char* title, const std::vector<std::string>& paths, bool isExample) {
            ImGui::TextDisabled("%s", title);
            if (paths.empty()) {
                ImGui::TextColored(kMuted, isExample ? "  (none found)" : "  (nothing saved yet)");
            }
            for (const std::string& path : paths) {
                const std::string name = std::filesystem::path(path).stem().string();
                if (ImGui::Selectable(("  " + name).c_str(), false, ImGuiSelectableFlags_None, ImVec2(320.0f * uiScale_, 0))) {
                    std::string message;
                    if (editor_.load(path, &message)) {
                        currentFile_ = isExample ? std::string() : path;
                        std::snprintf(fileName_, sizeof(fileName_), "%s", name.c_str());
                        frameRoom();
                    }
                    setStatus(message);
                    ImGui::CloseCurrentPopup();
                }
            }
        };
        listing("My layouts", listLayouts(kLayoutFolder), false);
        ImGui::Spacing();
        listing("Examples", listLayouts(ROOMCRAFT_EXAMPLES_DIR), true);
        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(120.0f * uiScale_, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace app
