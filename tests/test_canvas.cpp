#include "raster/canvas.h"

#include "test_helpers.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool constructorThrows(int width, int height) {
    try {
        raster::Canvas canvas(width, height);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

bool sameColor(raster::Color a, raster::Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

} // namespace

int main() {
    const std::string outDir = TESTS_OUTPUT_DIR;

    // --- Constructor rejects non-positive sizes ------------------------------
    CHECK(constructorThrows(0, 10));
    CHECK(constructorThrows(10, 0));
    CHECK(constructorThrows(-5, 10));
    CHECK(constructorThrows(10, -1));
    CHECK(!constructorThrows(1, 1));

    // --- setPixel / getPixel / clear -----------------------------------------
    raster::Canvas canvas(3, 2);
    CHECK(canvas.width() == 3);
    CHECK(canvas.height() == 2);
    canvas.clear(10, 20, 30);
    CHECK(sameColor(canvas.getPixel(2, 1), raster::Color{10, 20, 30}));

    canvas.setPixel(0, 1, raster::Color{255, 0, 0});  // bottom-left
    canvas.setPixel(2, 0, raster::Color{0, 0, 255});  // top-right
    CHECK(sameColor(canvas.getPixel(0, 1), raster::Color{255, 0, 0}));
    CHECK(sameColor(canvas.getPixel(2, 0), raster::Color{0, 0, 255}));

    // Out-of-bounds writes are ignored, reads return black.
    canvas.setPixel(-1, 0, raster::Color{1, 2, 3});
    canvas.setPixel(3, 0, raster::Color{1, 2, 3});
    canvas.setPixel(0, 2, raster::Color{1, 2, 3});
    CHECK(sameColor(canvas.getPixel(-1, 0), raster::Color{0, 0, 0}));
    CHECK(sameColor(canvas.getPixel(3, 5), raster::Color{0, 0, 0}));
    CHECK(canvas.inBounds(0, 0) && canvas.inBounds(2, 1));
    CHECK(!canvas.inBounds(3, 0) && !canvas.inBounds(0, 2) && !canvas.inBounds(-1, 0));

    // --- PPM: header followed by width*height*3 bytes ------------------------
    const std::string ppmPath = outDir + "/test_canvas.ppm";
    CHECK(canvas.savePPM(ppmPath));
    std::vector<uint8_t> ppm = readFile(ppmPath);
    const std::string ppmHeader = "P6\n3 2\n255\n";
    CHECK(ppm.size() == ppmHeader.size() + 3 * 2 * 3);
    if (ppm.size() >= ppmHeader.size()) {
        CHECK(std::string(ppm.begin(), ppm.begin() + static_cast<std::ptrdiff_t>(ppmHeader.size())) == ppmHeader);
    }

    // --- BMP: headers, bottom-up rows, BGR order, 4-byte row padding ---------
    // A 3-pixel row is 9 bytes, padded to 12. Two rows = 24 bytes + 54 header.
    const std::string bmpPath = outDir + "/test_canvas.bmp";
    CHECK(canvas.saveBMP(bmpPath));
    std::vector<uint8_t> bmp = readFile(bmpPath);
    CHECK(bmp.size() == 78u);
    if (bmp.size() == 78u) {
        CHECK(bmp[0] == 'B' && bmp[1] == 'M');
        CHECK(readU32(bmp, 2) == 78u);   // file size
        CHECK(readU32(bmp, 10) == 54u);  // pixel data offset
        CHECK(readU32(bmp, 14) == 40u);  // info header size
        CHECK(readU32(bmp, 18) == 3u);   // width
        CHECK(readU32(bmp, 22) == 2u);   // height
        CHECK(bmp[28] == 24);            // bits per pixel

        // First stored row is the BOTTOM row (y = 1); its first pixel is the
        // red one at (0, 1), stored as B, G, R.
        CHECK(bmp[54] == 0 && bmp[55] == 0 && bmp[56] == 255);
        // Second stored row is the top row (y = 0); pixel (2, 0) is blue.
        const size_t topRow = 54 + 12;
        CHECK(bmp[topRow + 6] == 255 && bmp[topRow + 7] == 0 && bmp[topRow + 8] == 0);
        // Padding bytes are zero.
        CHECK(bmp[54 + 9] == 0 && bmp[54 + 10] == 0 && bmp[54 + 11] == 0);
    }

    // --- Saving to a folder that does not exist fails cleanly ----------------
    CHECK(!canvas.savePPM(outDir + "/no_such_folder/x.ppm"));
    CHECK(!canvas.saveBMP(outDir + "/no_such_folder/x.bmp"));

    return test::summary("test_canvas");
}
