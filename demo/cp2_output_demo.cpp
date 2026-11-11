// RoomCraft Checkpoint 2, Part 5 demo: animation output and text.
//
// Writes to demo_output/cp2/ (or to the folder given as the first argument):
//
//   output_first_animation.gif  the room from Checkpoint 1 with an armchair
//                               sliding across it and a frame counter
//   output_font.bmp             the whole built-in font at scale 1, 2 and 3
//   output_palette.gif          an animation with far more than 256 colours,
//                               to show the fixed-palette fallback
//
// Every frame is drawn from scratch on a Canvas with the Checkpoint 1
// algorithms and handed to output::AnimationWriter.

#include "output/animation_writer.h"
#include "output/text.h"
#include "raster/canvas.h"
#include "raster/fill.h"
#include "raster/line.h"
#include "raster/polygon.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#ifndef ROOMCRAFT_DEMO_OUTPUT_DIR
#define ROOMCRAFT_DEMO_OUTPUT_DIR "demo_output"
#endif

using raster::Canvas;
using raster::Color;
using raster::Point;
using raster::Polygon;

namespace {

// ---- Palette and room geometry (as in demo/roomcraft_demo.cpp) --------------
const Color kWhite{255, 255, 255};
const Color kText{40, 40, 40};
const Color kOutline{40, 40, 40};
const Color kWall{60, 60, 70};
const Color kFloor{238, 226, 205};
const Color kGrid{214, 198, 172};
const Color kSofa{120, 150, 110};
const Color kTrail{180, 170, 150};
const Color kAccent{200, 60, 60};

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 480;
constexpr float kRoomLeft = 40.0f;
constexpr float kRoomTop = 40.0f;
constexpr float kRoomWidth = 560.0f;
constexpr float kRoomHeight = 400.0f;

int failures = 0;

void report(bool ok, const std::string& path) {
    if (ok) {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        std::printf("  wrote %s (%.0f KB)\n", path.c_str(), error ? 0.0 : static_cast<double>(size) / 1024.0);
    } else {
        std::printf("  ERROR: could not write %s\n", path.c_str());
        ++failures;
    }
}

// Floor, 50 cm grid (clipped to the room with Cohen-Sutherland) and walls.
void drawRoom(Canvas& canvas) {
    const Polygon room = raster::makeRectangle(kRoomLeft, kRoomTop, kRoomWidth, kRoomHeight);
    raster::fillPolygonScanline(canvas, room, kFloor);

    const raster::Rect rect{kRoomLeft, kRoomTop, kRoomLeft + kRoomWidth, kRoomTop + kRoomHeight};
    for (float x = kRoomLeft; x <= kRoomLeft + kRoomWidth; x += 50.0f) {
        raster::drawLineClipped(canvas, {x, 0}, {x, static_cast<float>(kCanvasHeight)}, rect, kGrid);
    }
    for (float y = kRoomTop; y <= kRoomTop + kRoomHeight; y += 50.0f) {
        raster::drawLineClipped(canvas, {0, y}, {static_cast<float>(kCanvasWidth), y}, rect, kGrid);
    }
    for (std::size_t i = 0; i < room.vertices.size(); ++i) {
        const Point a = room.vertices[i];
        const Point b = room.vertices[(i + 1) % room.vertices.size()];
        raster::drawThickLine(canvas, raster::roundToPixel(a.x), raster::roundToPixel(a.y),
                              raster::roundToPixel(b.x), raster::roundToPixel(b.y), 8, kWall);
    }
}

// Armchair seen from above (back and two arms), centred on (0, 0).
Polygon makeArmchair() {
    Polygon chair;
    chair.vertices = {{-35, -35}, {35, -35}, {35, 35}, {22, 35}, {22, -12}, {-22, -12}, {-22, 35}, {-35, 35}};
    return chair;
}

// ---- 1. A first animation ----------------------------------------------------
//
// The whole of frame-by-frame animation is in this loop: for each frame,
// work out where things are at that moment, clear the canvas, draw
// everything, and hand the finished picture to the writer.
void firstAnimation(const std::string& dir) {
    constexpr int kFrames = 60;
    constexpr int kFramesPerSecond = 20;

    output::AnimationWriter writer(dir, "output_first_animation", kCanvasWidth, kCanvasHeight, kFramesPerSecond);
    Canvas canvas(kCanvasWidth, kCanvasHeight);
    bool ok = true;

    for (int frame = 0; frame < kFrames; ++frame) {
        // Position at this moment: across the room and back, slowing down at
        // each end. t goes 0 -> 1 -> 0 over the animation.
        const double phase = 2.0 * raster::kPi * frame / kFrames;
        const float t = static_cast<float>(0.5 - 0.5 * std::cos(phase));
        const float x = 100.0f + t * 440.0f;
        const float y = 250.0f;

        canvas.clear(kWhite.r, kWhite.g, kWhite.b);
        drawRoom(canvas);

        // The path the chair travels along.
        raster::drawDashedLine(canvas, 100, 250, 540, 250, 6, 6, kTrail);

        Polygon chair = makeArmchair();
        raster::translate(chair, x, y);
        raster::fillPolygonScanline(canvas, chair, kSofa);
        raster::drawPolygonOutline(canvas, chair, kOutline);

        // Text: a title, the frame counter and the chair's position.
        output::drawText(canvas, 40, 10, "RoomCraft - first animation", kText, 2);
        const std::string counter = "frame " + std::to_string(frame + 1) + "/" + std::to_string(kFrames);
        output::drawText(canvas, kCanvasWidth - 40 - output::textWidth(counter, 2), 10, counter, kAccent, 2);
        const std::string position = "x = " + std::to_string(static_cast<int>(x - kRoomLeft)) + " cm";
        output::drawText(canvas, static_cast<int>(x) - output::textWidth(position) / 2, 300, position, kText);
        output::drawText(canvas, 40, 455, "5.6 m x 4.0 m, 1 pixel = 1 cm, " + std::to_string(kFramesPerSecond) + " frames/s", kText);

        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;

    report(ok, writer.path());
    std::printf("    %d frames, %.2f s each, %d colours, exact palette: %s\n", writer.frameCount(),
                writer.delayCentiseconds() / 100.0, writer.colorCount(), writer.usesFixedPalette() ? "no" : "yes");
}

// ---- 2. The font ---------------------------------------------------------------
void fontSheet(const std::string& dir) {
    Canvas canvas(kCanvasWidth, 400);
    canvas.clear(kWhite.r, kWhite.g, kWhite.b);

    // All 95 characters, in the order of their character codes, 32 per row.
    std::string rows[3];
    for (int code = 32; code <= 126; ++code) {
        rows[(code - 32) / 32].push_back(static_cast<char>(code));
    }

    int y = 12;
    for (int scale = 1; scale <= 3; ++scale) {
        output::drawText(canvas, 16, y, "scale " + std::to_string(scale), kAccent, 1);
        y += 12;
        for (const std::string& row : rows) {
            output::drawText(canvas, 16, y, row, kText, scale);
            y += (output::textHeight(scale) + 3 * scale);
        }
        y += 10;
    }

    // Sample text, a right-aligned line placed with textWidth, and what an
    // unknown character looks like.
    output::drawText(canvas, 16, y, "The quick brown fox jumps over the lazy dog. 0123456789", kText, 1);
    y += 14;
    output::drawText(canvas, 16, y, "Bed 140 x 200 cm, area 2.80 m2 (12.5% of the floor)", kText, 2);
    y += 22;
    const std::string right = "right-aligned with textWidth()";
    output::drawText(canvas, kCanvasWidth - 16 - output::textWidth(right, 2), y, right, kText, 2);
    raster::drawLineBresenham(canvas, kCanvasWidth - 16, y - 4, kCanvasWidth - 16, y + 18, kAccent);
    y += 26;
    output::drawText(canvas, 16, y, std::string("unknown characters: \x01 \t \xE9 are drawn as boxes"), kText, 2);

    const std::string path = dir + "/output_font.bmp";
    report(canvas.saveBMP(path), path);
}

// ---- 3. More than 256 colours ---------------------------------------------------
//
// A smooth, moving colour gradient: each frame has tens of thousands of
// different colours. The writer notices this in the first frame and uses the
// fixed 8 x 8 x 4 palette, mapping every pixel to the nearest entry. The
// smooth gradient comes out in visible bands: that is the price of 256
// colours. The flat-coloured strip and the text along the bottom stay sharp.
void paletteFallback(const std::string& dir) {
    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    constexpr int kFrames = 40;

    output::AnimationWriter writer(dir, "output_palette", kWidth, kHeight, 20);
    Canvas canvas(kWidth, kHeight);
    bool ok = true;
    int colorsInFirstFrame = 0;

    for (int frame = 0; frame < kFrames; ++frame) {
        const double shift = 2.0 * raster::kPi * frame / kFrames;
        for (int y = 0; y < 200; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const double u = static_cast<double>(x) / kWidth;
                const double v = static_cast<double>(y) / 200.0;
                const auto wave = [](double angle) {
                    return static_cast<uint8_t>(127.5 + 127.5 * std::sin(angle));
                };
                canvas.setPixel(x, y, Color{wave(2.0 * raster::kPi * u + shift),
                                            wave(2.0 * raster::kPi * v - shift),
                                            wave(2.0 * raster::kPi * (u + v) + 2.0 * shift)});
            }
        }
        // Flat colours and text below the gradient.
        for (int y = 200; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                canvas.setPixel(x, y, kWhite);
            }
        }
        output::drawText(canvas, 8, 206, "more than 256 colours:", kText, 2);
        output::drawText(canvas, 8, 224, "fixed 8 x 8 x 4 palette, nearest entry", kText, 1);

        if (frame == 0) {
            // Count the distinct colours of the first frame, for the report.
            std::vector<bool> seen(1u << 24, false);
            for (int y = 0; y < kHeight; ++y) {
                for (int x = 0; x < kWidth; ++x) {
                    const Color c = canvas.getPixel(x, y);
                    const std::size_t key = (static_cast<std::size_t>(c.r) << 16) | (static_cast<std::size_t>(c.g) << 8) | c.b;
                    if (!seen[key]) {
                        seen[key] = true;
                        ++colorsInFirstFrame;
                    }
                }
            }
        }
        ok = writer.addFrame(canvas) && ok;
    }
    ok = writer.finish() && ok;

    report(ok, writer.path());
    std::printf("    first frame has %d different colours; fixed palette used: %s; colours approximated: %s\n",
                colorsInFirstFrame, writer.usesFixedPalette() ? "yes" : "no", writer.approximatesColors() ? "yes" : "no");
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : std::string(ROOMCRAFT_DEMO_OUTPUT_DIR) + "/cp2";
    std::error_code error;
    std::filesystem::create_directories(dir, error);
    std::printf("RoomCraft Checkpoint 2 output demo - writing to %s\n", dir.c_str());

    std::printf("1. A first animation\n");
    firstAnimation(dir);
    std::printf("2. The font\n");
    fontSheet(dir);
    std::printf("3. More than 256 colours\n");
    paletteFallback(dir);

    if (failures > 0) {
        std::printf("%d file(s) could not be written\n", failures);
        return 1;
    }
    std::printf("Done.\n");
    return 0;
}
