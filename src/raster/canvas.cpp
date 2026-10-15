#include "raster/canvas.h"

#include <fstream>
#include <stdexcept>

namespace raster {

namespace {

// Validates the size before the pixel buffer is allocated, so a bad size can
// never produce a zero-sized or negative-sized (wrapped-around) buffer.
int requirePositive(int value, const char* what) {
    if (value <= 0) {
        throw std::invalid_argument(std::string("Canvas ") + what + " must be > 0");
    }
    return value;
}

// BMP stores all multi-byte numbers little-endian (lowest byte first). Writing
// the bytes one by one keeps the output correct on any machine.
void writeU16(std::ofstream& file, uint16_t value) {
    const char bytes[2] = {
        static_cast<char>(value & 0xFFu),
        static_cast<char>((value >> 8) & 0xFFu),
    };
    file.write(bytes, 2);
}

void writeU32(std::ofstream& file, uint32_t value) {
    const char bytes[4] = {
        static_cast<char>(value & 0xFFu),
        static_cast<char>((value >> 8) & 0xFFu),
        static_cast<char>((value >> 16) & 0xFFu),
        static_cast<char>((value >> 24) & 0xFFu),
    };
    file.write(bytes, 4);
}

} // namespace

Canvas::Canvas(int width, int height)
    : width_(requirePositive(width, "width")),
      height_(requirePositive(height, "height")),
      pixels_(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0) {}

bool Canvas::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

size_t Canvas::index(int x, int y) const {
    return (static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)) * 3;
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

// BMP layout (24 bits per pixel, no compression):
//   14-byte file header   "BM", file size, reserved, offset to pixel data
//   40-byte info header   size, width, height, planes, bit depth, ...
//   pixel rows            stored BOTTOM row first, each pixel as B, G, R,
//                         each row padded with zeros to a multiple of 4 bytes
// Our buffer is top row first and R, G, B, so we flip rows and swap channels.
bool Canvas::saveBMP(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width_) * 3;
    const size_t padding = (4 - rowBytes % 4) % 4;
    const size_t stride = rowBytes + padding;
    const uint32_t headerSize = 14 + 40;
    const uint32_t imageSize = static_cast<uint32_t>(stride * static_cast<size_t>(height_));

    // File header
    file.put('B');
    file.put('M');
    writeU32(file, headerSize + imageSize); // total file size
    writeU16(file, 0);                      // reserved
    writeU16(file, 0);                      // reserved
    writeU32(file, headerSize);             // offset to pixel data

    // Info header (BITMAPINFOHEADER)
    writeU32(file, 40);                              // size of this header
    writeU32(file, static_cast<uint32_t>(width_));   // width
    writeU32(file, static_cast<uint32_t>(height_));  // height (positive = bottom-up rows)
    writeU16(file, 1);                               // colour planes
    writeU16(file, 24);                              // bits per pixel
    writeU32(file, 0);                               // compression: none (BI_RGB)
    writeU32(file, imageSize);                       // size of pixel data
    writeU32(file, 2835);                            // horizontal resolution (72 DPI in pixels/metre)
    writeU32(file, 2835);                            // vertical resolution
    writeU32(file, 0);                               // colours in palette (none)
    writeU32(file, 0);                               // important colours (all)

    std::vector<char> row(stride, 0);
    for (int y = height_ - 1; y >= 0; --y) {
        for (int x = 0; x < width_; ++x) {
            const size_t src = index(x, y);
            const size_t dst = static_cast<size_t>(x) * 3;
            row[dst] = static_cast<char>(pixels_[src + 2]);     // B
            row[dst + 1] = static_cast<char>(pixels_[src + 1]); // G
            row[dst + 2] = static_cast<char>(pixels_[src]);     // R
        }
        file.write(row.data(), static_cast<std::streamsize>(stride));
    }
    return file.good();
}

} // namespace raster
