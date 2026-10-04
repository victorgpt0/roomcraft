#include "raster/canvas.h"

#include <fstream>

namespace raster {

Canvas::Canvas(int width, int height)
    : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height * 3, 0) {}

bool Canvas::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

size_t Canvas::index(int x, int y) const {
    return (static_cast<size_t>(y) * width_ + x) * 3;
}

void Canvas::clear(uint8_t r, uint8_t g, uint8_t b) {
    for (size_t i = 0; i < pixels_.size(); i += 3) {
        pixels_[i] = r;
        pixels_[i + 1] = g;
        pixels_[i + 2] = b;
    }
}

void Canvas::setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!inBounds(x, y)) {
        return;
    }
    size_t i = index(x, y);
    pixels_[i] = r;
    pixels_[i + 1] = g;
    pixels_[i + 2] = b;
}

void Canvas::setPixel(int x, int y, Color color) {
    setPixel(x, y, color.r, color.g, color.b);
}

Color Canvas::getPixel(int x, int y) const {
    if (!inBounds(x, y)) {
        return Color{0, 0, 0};
    }
    size_t i = index(x, y);
    return Color{pixels_[i], pixels_[i + 1], pixels_[i + 2]};
}

bool Canvas::savePPM(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file << "P6\n" << width_ << " " << height_ << "\n255\n";
    file.write(reinterpret_cast<const char*>(pixels_.data()), static_cast<std::streamsize>(pixels_.size()));
    return file.good();
}

} // namespace raster
