// The layout drawn by the Checkpoint 1/2 software rasterizer, in a panel.
//
// Nothing here uses OpenGL to DRAW: the picture is made pixel by pixel on a
// raster::Canvas with our own scanline fill, Bresenham lines, Cohen-
// Sutherland and Sutherland-Hodgman clipping, the CP2 window-to-viewport
// camera and the CP2 bitmap font. OpenGL is only used to show the finished
// pixels, as a texture, next to the GPU-rendered view.

#include "app.h"

#include "editor/layout.h"
#include "gfx/gl.h"
#include "output/text.h"
#include "raster/canvas.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "view/camera2d.h"

#include <imgui.h>

#include <cstdint>
#include <vector>

namespace app {

namespace {

constexpr int kWidth = 520;
constexpr int kHeight = 380;

raster::Canvas renderSoftware(const editor::Editor& editor) {
    const editor::Layout& layout = editor.layout();
    const editor::Room& room = layout.room;
    raster::Canvas canvas(kWidth, kHeight);
    canvas.clear(232, 234, 238);

    // Window: the room plus its walls and a margin. Viewport: the canvas.
    const float margin = room.wallThickness + 20.0f;
    view::Camera2D camera;
    camera.window = {-margin, -margin, room.width + margin, room.depth + margin};
    camera.viewport = {0.0f, 22.0f, static_cast<float>(kWidth), static_cast<float>(kHeight)};
    view::matchViewportAspect(camera);

    // Walls: the outer rectangle in the wall colour, the floor on top.
    const float t = room.wallThickness;
    view::fillPolygonThroughCamera(canvas, raster::makeRectangle(-t, -t, room.width + 2 * t, room.depth + 2 * t), camera,
                                   {60, 60, 70});
    view::fillPolygonThroughCamera(canvas, editor::roomOutline(room), camera, room.floorColor);
    for (float x = 50.0f; x < room.width; x += 50.0f) {
        view::drawLineThroughCamera(canvas, {x, 0}, {x, room.depth}, camera, {196, 176, 146});
    }
    for (float y = 50.0f; y < room.depth; y += 50.0f) {
        view::drawLineThroughCamera(canvas, {0, y}, {room.width, y}, camera, {196, 176, 146});
    }

    // Furniture: the CP2 function, fed with CP1 Furniture built from the
    // items (local footprint + pose).
    std::vector<roomcraft::Furniture> pieces;
    const std::vector<editor::ItemStatus>& status = editor.status();
    for (std::size_t i = 0; i < layout.items.size(); ++i) {
        roomcraft::Furniture piece = editor::toFurniture(layout.items[i]);
        if (!status[i].fitsInRoom) {
            piece.color = {220, 50, 50};
        } else if (status[i].overlaps) {
            piece.color = {240, 180, 40};
        }
        pieces.push_back(piece);
    }
    view::drawFurnitureThroughCamera(canvas, pieces, camera);

    if (const editor::Item* selected = editor.selected()) {
        const raster::Polygon outline = editor::itemFootprint(*selected);
        view::drawPolygonOutlineThroughCamera(canvas, outline, camera, {255, 140, 0});
        // A second, slightly bigger outline makes it two pixels thick.
        const raster::Point c = raster::computeCentroid(outline);
        raster::Polygon bigger = outline;
        raster::scale(bigger, 1.0f + 1.5f / view::scaleX(camera) / 50.0f, 1.0f + 1.5f / view::scaleY(camera) / 50.0f, c);
        view::drawPolygonOutlineThroughCamera(canvas, bigger, camera, {255, 140, 0});
    }

    for (const editor::Item& item : layout.items) {
        const raster::Polygon footprint = editor::itemFootprint(item);
        raster::Point anchor = raster::computeCentroid(footprint);
        if (editor::typeOf(item).floorLevel) {
            anchor.y = raster::computeBoundingBox(footprint).ymax - 14.0f; // rugs: at their lower edge
        }
        const raster::Point c = view::roomToScreen(camera, anchor);
        const int width = output::textWidth(item.name);
        output::drawText(canvas, static_cast<int>(c.x) - width / 2, static_cast<int>(c.y) - 3, item.name, {30, 30, 35});
    }

    // Title bar.
    for (int y = 0; y < 22; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            canvas.setPixel(x, y, 40, 42, 48);
        }
    }
    output::drawText(canvas, 8, 7, "CP1/CP2 software rasterizer - no OpenGL drawing", {240, 240, 240});
    return canvas;
}

} // namespace

void App::drawSoftwareWindow() {
    // Re-render only when something changed.
    if (softwareTexture_ == 0 || softwareRevision_ != editor_.revision() || softwareSelected_ != editor_.selectedId()) {
        const raster::Canvas canvas = renderSoftware(editor_);
        std::vector<unsigned char> rgb(static_cast<std::size_t>(kWidth * kHeight * 3));
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const raster::Color c = canvas.getPixel(x, y);
                const std::size_t i = (static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)) * 3;
                rgb[i] = c.r;
                rgb[i + 1] = c.g;
                rgb[i + 2] = c.b;
            }
        }
        if (softwareTexture_ == 0) {
            GLuint texture = 0;
            glGenTextures(1, &texture);
            softwareTexture_ = texture;
        }
        glBindTexture(GL_TEXTURE_2D, softwareTexture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // show the real pixels when enlarged
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, kWidth, kHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        softwareRevision_ = editor_.revision();
        softwareSelected_ = editor_.selectedId();
    }

    ImGui::SetNextWindowPos(ImVec2(viewX_ + viewW_ - (kWidth + 24.0f) - 10.0f, viewY_ + 10.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(kWidth + 24.0f, kHeight + 70.0f), ImGuiCond_Appearing);
    if (ImGui::Begin("Software renderer (Checkpoints 1 and 2)", &showSoftware_)) {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float scale = std::min(available.x / kWidth, (available.y - ImGui::GetTextLineHeightWithSpacing()) / kHeight);
        ImGui::Image(static_cast<ImTextureID>(softwareTexture_), ImVec2(kWidth * scale, kHeight * scale));
        ImGui::TextDisabled("Scanline fill, Bresenham, clipping, Camera2D, bitmap font - drawn on the CPU");
    }
    ImGui::End();
}

void App::releaseSoftwareView() {
    if (softwareTexture_ != 0) {
        GLuint texture = softwareTexture_;
        glDeleteTextures(1, &texture);
        softwareTexture_ = 0;
    }
}

} // namespace app
