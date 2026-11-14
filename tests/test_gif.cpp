// Tests for the GIF encoder (output/gif.h) and output::AnimationWriter.
//
// The encoder is checked by READING THE FILES BACK with a small GIF/LZW
// decoder written here, independently of the encoder: if the decoder
// recovers exactly the pixels that were drawn, the file structure and the
// compression are right.
//
// Scratch files go to a folder in the system temp directory and are removed
// at the end.

#include "output/animation_writer.h"
#include "output/gif.h"
#include "raster/canvas.h"

#include "test_helpers.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

using raster::Canvas;
using raster::Color;

namespace {

// ---------------------------------------------------------------------------
// A minimal GIF decoder (test only)
// ---------------------------------------------------------------------------

// LZW decoding: rebuilds the encoder's code table from the codes alone.
// Each table entry is (code of the prefix sequence, last index).
// Returns false if the stream is malformed.
bool lzwDecode(const std::vector<uint8_t>& data, int minCodeSize, std::vector<uint8_t>& out) {
    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    std::vector<int> prefix(4096, -1);
    std::vector<uint8_t> suffix(4096, 0);

    int codeSize = minCodeSize + 1;
    int avail = clearCode + 2; // next code the table will hand out
    int previous = -1;

    std::size_t bytePos = 0;
    uint32_t buffer = 0;
    int bitsHeld = 0;
    std::vector<uint8_t> sequence;

    for (;;) {
        while (bitsHeld < codeSize) {
            if (bytePos >= data.size()) {
                return false; // ran out of data before the END code
            }
            buffer |= static_cast<uint32_t>(data[bytePos++]) << bitsHeld;
            bitsHeld += 8;
        }
        const int code = static_cast<int>(buffer & ((1u << codeSize) - 1u));
        buffer >>= codeSize;
        bitsHeld -= codeSize;

        if (code == clearCode) {
            codeSize = minCodeSize + 1;
            avail = clearCode + 2;
            previous = -1;
            continue;
        }
        if (code == endCode) {
            return true;
        }
        if (previous == -1) {
            if (code >= clearCode) {
                return false; // the first code after CLEAR must be a single index
            }
            out.push_back(static_cast<uint8_t>(code));
            previous = code;
            continue;
        }
        if (code > avail) {
            return false; // a code the table cannot contain yet
        }

        // Walk the prefix chain backwards to get the sequence. If the code is
        // the one about to be defined (code == avail), it is the previous
        // sequence followed by its own first index.
        sequence.clear();
        int walk = code == avail ? previous : code;
        while (walk >= clearCode) {
            sequence.push_back(suffix[static_cast<std::size_t>(walk)]);
            walk = prefix[static_cast<std::size_t>(walk)];
        }
        const uint8_t first = static_cast<uint8_t>(walk);
        sequence.push_back(first);
        out.insert(out.end(), sequence.rbegin(), sequence.rend());
        if (code == avail) {
            out.push_back(first);
        }

        if (avail < 4096) {
            prefix[static_cast<std::size_t>(avail)] = previous;
            suffix[static_cast<std::size_t>(avail)] = first;
            ++avail;
            if (avail == (1 << codeSize) && codeSize < 12) {
                ++codeSize;
            }
        }
        previous = code;
    }
}

struct DecodedFrame {
    int delayCentiseconds = -1; // -1: no graphic control extension before this frame
    int disposal = -1;
    bool transparent = false;
    int left = 0, top = 0, width = 0, height = 0;
    bool localTable = false;
    bool interlaced = false;
    int minCodeSize = 0;
    int largestSubBlock = 0;
    bool lzwOk = false;
    std::vector<uint8_t> indices;
};

struct DecodedGif {
    bool ok = false;      // the whole file parsed, ending exactly at the trailer
    std::string error;
    std::string signature;
    int width = 0, height = 0;
    bool globalTable = false;
    int globalTableSize = 0;
    std::vector<Color> palette;
    bool loops = false;   // NETSCAPE2.0 extension present
    int loopCount = -1;
    std::vector<DecodedFrame> frames;

    Color pixel(std::size_t frame, int x, int y) const {
        const DecodedFrame& f = frames[frame];
        return palette[f.indices[static_cast<std::size_t>(y) * static_cast<std::size_t>(f.width) +
                                 static_cast<std::size_t>(x)]];
    }
};

DecodedGif decodeGif(const std::string& path) {
    DecodedGif gif;
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        gif.error = "cannot open file";
        return gif;
    }
    const std::vector<uint8_t> b((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::size_t pos = 0;
    const auto need = [&](std::size_t count) { return pos + count <= b.size(); };
    const auto u16 = [&](std::size_t at) { return static_cast<int>(b[at]) | (static_cast<int>(b[at + 1]) << 8); };

    if (!need(13)) {
        gif.error = "file too short";
        return gif;
    }
    gif.signature.assign(b.begin(), b.begin() + 6);
    gif.width = u16(6);
    gif.height = u16(8);
    const int packed = b[10];
    gif.globalTable = (packed & 0x80) != 0;
    gif.globalTableSize = 1 << ((packed & 0x07) + 1);
    pos = 13;
    if (gif.globalTable) {
        if (!need(static_cast<std::size_t>(gif.globalTableSize) * 3)) {
            gif.error = "truncated colour table";
            return gif;
        }
        for (int i = 0; i < gif.globalTableSize; ++i) {
            gif.palette.push_back(Color{b[pos], b[pos + 1], b[pos + 2]});
            pos += 3;
        }
    }

    // Reads data sub-blocks up to the 0 terminator.
    const auto readSubBlocks = [&](std::vector<uint8_t>& data, int& largest) {
        for (;;) {
            if (!need(1)) {
                return false;
            }
            const int length = b[pos++];
            if (length == 0) {
                return true;
            }
            if (!need(static_cast<std::size_t>(length))) {
                return false;
            }
            largest = length > largest ? length : largest;
            data.insert(data.end(), b.begin() + static_cast<std::ptrdiff_t>(pos),
                        b.begin() + static_cast<std::ptrdiff_t>(pos + static_cast<std::size_t>(length)));
            pos += static_cast<std::size_t>(length);
        }
    };

    DecodedFrame pending; // collects the graphic control extension for the next image
    for (;;) {
        if (!need(1)) {
            gif.error = "no trailer";
            return gif;
        }
        const int block = b[pos++];
        if (block == 0x3B) { // trailer
            if (pos != b.size()) {
                gif.error = "bytes after the trailer";
                return gif;
            }
            gif.ok = true;
            return gif;
        }
        if (block == 0x21) { // extension
            if (!need(1)) {
                gif.error = "truncated extension";
                return gif;
            }
            const int label = b[pos++];
            std::vector<uint8_t> data;
            int largest = 0;
            const std::size_t dataStart = pos;
            if (!readSubBlocks(data, largest)) {
                gif.error = "truncated extension data";
                return gif;
            }
            if (label == 0xF9 && data.size() == 4 && b[dataStart] == 4) {
                pending.disposal = (data[0] >> 2) & 0x07;
                pending.transparent = (data[0] & 0x01) != 0;
                pending.delayCentiseconds = static_cast<int>(data[1]) | (static_cast<int>(data[2]) << 8);
            } else if (label == 0xFF && data.size() == 14 &&
                       std::string(data.begin(), data.begin() + 11) == "NETSCAPE2.0" && data[11] == 1) {
                gif.loops = true;
                gif.loopCount = static_cast<int>(data[12]) | (static_cast<int>(data[13]) << 8);
            }
            continue;
        }
        if (block == 0x2C) { // image
            if (!need(10)) {
                gif.error = "truncated image descriptor";
                return gif;
            }
            DecodedFrame frame = pending;
            pending = DecodedFrame{};
            frame.left = u16(pos);
            frame.top = u16(pos + 2);
            frame.width = u16(pos + 4);
            frame.height = u16(pos + 6);
            frame.localTable = (b[pos + 8] & 0x80) != 0;
            frame.interlaced = (b[pos + 8] & 0x40) != 0;
            frame.minCodeSize = b[pos + 9];
            pos += 10;
            if (frame.localTable) {
                gif.error = "local colour table (not expected from our encoder)";
                return gif;
            }
            std::vector<uint8_t> data;
            if (!readSubBlocks(data, frame.largestSubBlock)) {
                gif.error = "truncated image data";
                return gif;
            }
            frame.lzwOk = lzwDecode(data, frame.minCodeSize, frame.indices);
            gif.frames.push_back(frame);
            continue;
        }
        gif.error = "unknown block type";
        return gif;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string scratchDirectory() {
    return (std::filesystem::temp_directory_path() / "roomcraft_test_gif").string();
}

bool sameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int distanceSquared(Color a, Color b) {
    const int dr = static_cast<int>(a.r) - b.r;
    const int dg = static_cast<int>(a.g) - b.g;
    const int db = static_cast<int>(a.b) - b.b;
    return dr * dr + dg * dg + db * db;
}

// Number of pixels of `canvas` that differ from frame `frame` of `gif`.
int countDifferences(const Canvas& canvas, const DecodedGif& gif, std::size_t frame) {
    int differences = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            if (!sameColor(canvas.getPixel(x, y), gif.pixel(frame, x, y))) {
                ++differences;
            }
        }
    }
    return differences;
}

// Number of pixels whose decoded colour is NOT a nearest palette colour of
// the original (checked by brute force against every palette entry in use).
int countNotNearest(const Canvas& canvas, const DecodedGif& gif, std::size_t frame, int paletteEntries) {
    int wrong = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const Color original = canvas.getPixel(x, y);
            int best = distanceSquared(original, gif.palette[0]);
            for (int i = 1; i < paletteEntries; ++i) {
                const int d = distanceSquared(original, gif.palette[static_cast<std::size_t>(i)]);
                best = d < best ? d : best;
            }
            if (distanceSquared(original, gif.pixel(frame, x, y)) != best) {
                ++wrong;
            }
        }
    }
    return wrong;
}

// Checks everything about a frame that must hold for every file we write.
void checkFrameStructure(const DecodedGif& gif, std::size_t frame, int width, int height, int delay) {
    if (!CHECK(frame < gif.frames.size())) {
        return;
    }
    const DecodedFrame& f = gif.frames[frame];
    CHECK(f.lzwOk);
    CHECK(f.delayCentiseconds == delay);
    CHECK(f.disposal == 1);
    CHECK(!f.transparent);
    CHECK(f.left == 0 && f.top == 0);
    CHECK(f.width == width && f.height == height);
    CHECK(!f.localTable);
    CHECK(!f.interlaced);
    CHECK(f.minCodeSize == 8);
    CHECK(f.largestSubBlock >= 1 && f.largestSubBlock <= 255);
    CHECK(f.indices.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
}

void checkFileStructure(const DecodedGif& gif, int width, int height, std::size_t frames) {
    CHECK_MSG(gif.ok, gif.error);
    CHECK(gif.signature == "GIF89a");
    CHECK(gif.width == width && gif.height == height);
    CHECK(gif.globalTable);
    CHECK(gif.globalTableSize == 256);
    CHECK(gif.loops);
    CHECK(gif.loopCount == 0); // 0 = loop forever
    CHECK(gif.frames.size() == frames);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// The LZW compressor alone: compress, decode with the test decoder, compare.
void testLzwRoundTrip() {
    std::mt19937 random(12345);

    const auto roundTrip = [](const std::vector<uint8_t>& indices, int minCodeSize) {
        const std::vector<uint8_t> compressed = output::lzwCompress(indices, minCodeSize);
        std::vector<uint8_t> decoded;
        const bool ok = lzwDecode(compressed, minCodeSize, decoded);
        return ok && decoded == indices;
    };

    // Nothing, one pixel, two pixels.
    CHECK(roundTrip({}, 8));
    CHECK(roundTrip({7}, 8));
    CHECK(roundTrip({7, 7}, 8));
    CHECK(roundTrip({0, 255}, 8));

    // One flat colour: the matches grow 1, 2, 3, ... pixels long. This is
    // the case where the decoder meets a code it has not finished defining.
    const std::vector<uint8_t> flat(100000, 42);
    CHECK(roundTrip(flat, 8));
    // 100,000 identical pixels need only about sqrt(2 * 100000) = 447 codes.
    CHECK(output::lzwCompress(flat, 8).size() < 700);

    // Random pixels: nothing repeats, a new code is defined at almost every
    // pixel, so the 4096-entry table fills up and is cleared many times.
    std::vector<uint8_t> noise(200000);
    for (uint8_t& value : noise) {
        value = static_cast<uint8_t>(random() & 0xFF);
    }
    CHECK(roundTrip(noise, 8));

    // Exactly around the points where the code size grows and the table
    // fills: every length from 1 to 6000 pixels of a non-repeating pattern
    // would be slow, so step through lengths near the boundaries.
    for (int length : {253, 254, 255, 256, 257, 258, 509, 510, 511, 512, 513, 1021, 1022, 1023, 1024,
                       2045, 2046, 2047, 2048, 3837, 3838, 3839, 3840, 4093, 4094, 4095, 4096, 4097, 8191, 8192}) {
        std::vector<uint8_t> data(static_cast<std::size_t>(length));
        for (uint8_t& value : data) {
            value = static_cast<uint8_t>(random() & 0xFF);
        }
        CHECK_MSG(roundTrip(data, 8), "random data, length " + std::to_string(length));
    }

    // Repeating patterns of different periods (long matches, few codes).
    for (int period : {2, 3, 7, 64, 255, 256, 1000}) {
        std::vector<uint8_t> pattern(50000);
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            pattern[i] = static_cast<uint8_t>((i % static_cast<std::size_t>(period)) & 0xFF);
        }
        CHECK_MSG(roundTrip(pattern, 8), "pattern period " + std::to_string(period));
    }

    // Smaller palettes use smaller starting code sizes (GIF allows 2..8).
    for (int minCodeSize = 2; minCodeSize <= 8; ++minCodeSize) {
        std::vector<uint8_t> data(30000);
        for (uint8_t& value : data) {
            value = static_cast<uint8_t>(random() & ((1u << minCodeSize) - 1u));
        }
        CHECK_MSG(roundTrip(data, minCodeSize), "min code size " + std::to_string(minCodeSize));
    }

    // Compression actually compresses a picture-like input: long runs.
    std::vector<uint8_t> runs;
    for (int row = 0; row < 200; ++row) {
        runs.insert(runs.end(), 150, 1);
        runs.insert(runs.end(), 100, static_cast<uint8_t>(row % 4));
        runs.insert(runs.end(), 150, 1);
    }
    CHECK(roundTrip(runs, 8));
    CHECK(output::lzwCompress(runs, 8).size() < runs.size() / 10);
}

// Several known frames written and read back.
void testAnimationRoundTrip() {
    const int width = 37;
    const int height = 23;
    std::mt19937 random(2024);

    const Color colors[5] = {{255, 255, 255}, {0, 0, 0}, {220, 30, 30}, {96, 125, 160}, {238, 226, 205}};

    Canvas speckled(width, height); // random choice among 5 colours
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            speckled.setPixel(x, y, colors[random() % 5]);
        }
    }
    Canvas flat(width, height); // one flat colour
    flat.clear(96, 125, 160);
    Canvas stripes(width, height); // a regular pattern with a new colour
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            stripes.setPixel(x, y, (x / 3 + y) % 2 == 0 ? Color{10, 200, 90} : colors[0]);
        }
    }
    Canvas noise(width, height); // every pixel random among 200 colours
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int k = static_cast<int>(random() % 200);
            noise.setPixel(x, y, Color{static_cast<uint8_t>(k), static_cast<uint8_t>(255 - k), 77});
        }
    }
    const Canvas* frames[] = {&speckled, &flat, &stripes, &noise, &speckled};

    std::string path;
    {
        output::AnimationWriter writer(scratchDirectory(), "round_trip", width, height, 20);
        path = writer.path();
        CHECK(writer.frameCount() == 0);
        CHECK(writer.delayCentiseconds() == 5); // 20 fps = 0.05 s per frame
        for (const Canvas* frame : frames) {
            CHECK(writer.addFrame(*frame));
        }
        CHECK(writer.frameCount() == 5);
        CHECK(writer.colorCount() == 5 + 1 + 200); // 5 shared, 1 stripe green, 200 noise
        CHECK(!writer.usesFixedPalette());
        CHECK(!writer.approximatesColors());
        CHECK(writer.finish());
        CHECK(writer.finish());             // calling it again is harmless
        CHECK(!writer.addFrame(flat));      // no frames after finish
        CHECK(writer.frameCount() == 5);
    }
    CHECK(path == scratchDirectory() + "/round_trip.gif");

    const DecodedGif gif = decodeGif(path);
    checkFileStructure(gif, width, height, 5);
    for (std::size_t i = 0; i < gif.frames.size(); ++i) {
        checkFrameStructure(gif, i, width, height, 5);
        if (gif.frames[i].indices.size() == static_cast<std::size_t>(width * height)) {
            // Every decoded pixel equals the pixel that was drawn.
            CHECK(countDifferences(*frames[i], gif, i) == 0);
        }
    }
    // Unused palette entries are black.
    CHECK(sameColor(gif.palette[255], Color{0, 0, 0}));

    // The file starts with the signature and ends with the trailer byte.
    std::ifstream file(path, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(bytes.size() > 13 + 768);
    CHECK(std::string(bytes.begin(), bytes.begin() + 6) == "GIF89a");
    CHECK(static_cast<unsigned char>(bytes.back()) == 0x3B);
}

// A frame much larger than the code table, so the table is cleared mid-frame,
// and with data longer than one 255-byte sub-block.
void testLargeFrames() {
    const int width = 300;
    const int height = 200;
    std::mt19937 random(99);

    Canvas noise(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int k = static_cast<int>(random() % 256);
            noise.setPixel(x, y, Color{static_cast<uint8_t>(k), static_cast<uint8_t>(k / 2), static_cast<uint8_t>(255 - k)});
        }
    }
    Canvas flat(width, height);
    flat.clear(0, 0, 255); // one of the 256 noise colours (k = 0), so the total stays at 256

    output::AnimationWriter writer(scratchDirectory(), "large", width, height, 10);
    CHECK(writer.addFrame(noise));
    CHECK(writer.addFrame(flat));
    CHECK(writer.addFrame(noise));
    CHECK(writer.finish());
    CHECK(writer.colorCount() == 256); // exactly 256: still exact, no approximation
    CHECK(!writer.usesFixedPalette());
    CHECK(!writer.approximatesColors());

    const DecodedGif gif = decodeGif(writer.path());
    checkFileStructure(gif, width, height, 3);
    for (std::size_t i = 0; i < gif.frames.size(); ++i) {
        checkFrameStructure(gif, i, width, height, 10);
    }
    if (gif.frames.size() == 3) {
        CHECK(countDifferences(noise, gif, 0) == 0);
        CHECK(countDifferences(flat, gif, 1) == 0);
        CHECK(countDifferences(noise, gif, 2) == 0);
        CHECK(gif.frames[0].largestSubBlock == 255); // long data is cut into full sub-blocks
    }

    // The flat frame compresses to almost nothing; the noise frame cannot.
    const auto fileSize = std::filesystem::file_size(writer.path());
    CHECK(fileSize > 2u * 60000u / 2u);
    CHECK(fileSize < 3u * 60000u * 2u);
}

// Degenerate sizes: one pixel wide, one pixel high, a single pixel.
void testTinyImages() {
    const int sizes[][2] = {{1, 50}, {50, 1}, {1, 1}, {2, 2}};
    for (const auto& size : sizes) {
        const int width = size[0];
        const int height = size[1];
        Canvas a(width, height);
        Canvas b(width, height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                a.setPixel(x, y, (x + y) % 3 == 0 ? Color{255, 0, 0} : Color{0, 0, 255});
                b.setPixel(x, y, Color{0, static_cast<uint8_t>(5 * (x + y)), 0});
            }
        }
        const std::string name = "tiny_" + std::to_string(width) + "x" + std::to_string(height);
        output::AnimationWriter writer(scratchDirectory(), name, width, height, 10);
        CHECK(writer.addFrame(a));
        CHECK(writer.addFrame(b));
        CHECK(writer.finish());

        const DecodedGif gif = decodeGif(writer.path());
        checkFileStructure(gif, width, height, 2);
        checkFrameStructure(gif, 0, width, height, 10);
        checkFrameStructure(gif, 1, width, height, 10);
        if (gif.frames.size() == 2) {
            CHECK_MSG(countDifferences(a, gif, 0) == 0, name);
            CHECK_MSG(countDifferences(b, gif, 1) == 0, name);
        }
    }
}

// More than 256 colours in the FIRST frame: the fixed palette is used and
// every pixel becomes its nearest palette colour.
void testFixedPaletteFallback() {
    const int width = 96;
    const int height = 64;

    // A smooth gradient with thousands of colours.
    Canvas gradient(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            gradient.setPixel(x, y, Color{static_cast<uint8_t>(x * 255 / (width - 1)),
                                          static_cast<uint8_t>(y * 255 / (height - 1)),
                                          static_cast<uint8_t>((x + y) * 255 / (width + height - 2))});
        }
    }
    // A second frame with different colours, including pure black and white.
    Canvas second(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            second.setPixel(x, y, Color{static_cast<uint8_t>(255 - y * 4), static_cast<uint8_t>(x * 2),
                                        static_cast<uint8_t>((x * y) % 256)});
        }
    }
    second.setPixel(0, 0, Color{0, 0, 0});
    second.setPixel(1, 0, Color{255, 255, 255});

    output::AnimationWriter writer(scratchDirectory(), "fixed_palette", width, height, 10);
    CHECK(writer.addFrame(gradient));
    CHECK(writer.usesFixedPalette());
    CHECK(writer.approximatesColors());
    CHECK(writer.colorCount() == 256);
    CHECK(writer.addFrame(second));
    CHECK(writer.finish());

    const DecodedGif gif = decodeGif(writer.path());
    checkFileStructure(gif, width, height, 2);
    checkFrameStructure(gif, 0, width, height, 10);
    checkFrameStructure(gif, 1, width, height, 10);

    // The palette is the 8 x 8 x 4 grid: all 256 entries are different, and
    // black and white are both in it exactly.
    int duplicates = 0;
    for (std::size_t i = 0; i < gif.palette.size(); ++i) {
        for (std::size_t j = i + 1; j < gif.palette.size(); ++j) {
            if (sameColor(gif.palette[i], gif.palette[j])) {
                ++duplicates;
            }
        }
    }
    CHECK(gif.palette.size() == 256);
    CHECK(duplicates == 0);
    CHECK(sameColor(gif.palette[0], Color{0, 0, 0}));
    CHECK(sameColor(gif.palette[255], Color{255, 255, 255}));

    if (gif.frames.size() == 2) {
        // Every pixel maps to a nearest palette colour (brute-force check).
        CHECK(countNotNearest(gradient, gif, 0, 256) == 0);
        CHECK(countNotNearest(second, gif, 1, 256) == 0);
        // Black and white survive exactly.
        CHECK(sameColor(gif.pixel(1, 0, 0), Color{0, 0, 0}));
        CHECK(sameColor(gif.pixel(1, 1, 0), Color{255, 255, 255}));
        // The error is bounded by half a palette step per channel:
        // steps are 36.4 (red, green) and 85 (blue).
        int worst = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int d = distanceSquared(gradient.getPixel(x, y), gif.pixel(0, x, y));
                worst = d > worst ? d : worst;
            }
        }
        CHECK(worst <= 19 * 19 + 19 * 19 + 43 * 43);
        CHECK(worst > 0);
    }

    // useFixedPalette() before the first frame forces it even for few colours.
    Canvas plain(width, height);
    plain.clear(255, 255, 255);
    plain.setPixel(3, 3, Color{100, 100, 100}); // not on the grid
    output::AnimationWriter forced(scratchDirectory(), "forced_fixed", width, height, 10);
    forced.useFixedPalette();
    CHECK(forced.addFrame(plain));
    CHECK(forced.usesFixedPalette());
    CHECK(forced.finish());
    const DecodedGif forcedGif = decodeGif(forced.path());
    checkFileStructure(forcedGif, width, height, 1);
    if (forcedGif.frames.size() == 1) {
        CHECK(sameColor(forcedGif.pixel(0, 0, 0), Color{255, 255, 255}));
        CHECK(sameColor(forcedGif.pixel(0, 3, 3), Color{109, 109, 85})); // nearest grid colour
        CHECK(countNotNearest(plain, forcedGif, 0, 256) == 0);
    }
}

// The total goes over 256 colours in a LATER frame: the palette is frozen,
// earlier colours stay exact, further colours map to the nearest entry.
void testPaletteFreezesWhenFull() {
    const int width = 20;
    const int height = 15; // 300 pixels

    // Frame 1: 200 different greys-with-a-tint, all exact.
    Canvas first(width, height);
    for (int i = 0; i < width * height; ++i) {
        const int k = i % 200;
        first.setPixel(i % width, i / width, Color{static_cast<uint8_t>(k), static_cast<uint8_t>(k), 40});
    }
    // Frame 2: 300 colours never seen before. 56 still fit; 244 do not.
    Canvas second(width, height);
    for (int i = 0; i < width * height; ++i) {
        second.setPixel(i % width, i / width,
                        Color{250, static_cast<uint8_t>(i % 256), static_cast<uint8_t>(100 + i / 3)});
    }

    output::AnimationWriter writer(scratchDirectory(), "frozen_palette", width, height, 10);
    CHECK(writer.addFrame(first));
    CHECK(writer.colorCount() == 200);
    CHECK(!writer.approximatesColors());
    CHECK(writer.addFrame(second));
    CHECK(writer.colorCount() == 256);
    CHECK(!writer.usesFixedPalette()); // too late for the fixed palette
    CHECK(writer.approximatesColors());
    CHECK(writer.addFrame(first));     // old colours are still exact afterwards
    CHECK(writer.finish());

    const DecodedGif gif = decodeGif(writer.path());
    checkFileStructure(gif, width, height, 3);
    if (gif.frames.size() == 3) {
        CHECK(countDifferences(first, gif, 0) == 0);
        CHECK(countDifferences(first, gif, 2) == 0);
        // The first 56 new colours got the last free entries and are exact...
        int exact = 0;
        for (int i = 0; i < width * height; ++i) {
            if (sameColor(second.getPixel(i % width, i / width), gif.pixel(1, i % width, i / width))) {
                ++exact;
            }
        }
        CHECK(exact == 56);
        // ...and every pixel, exact or not, is a nearest palette colour.
        CHECK(countNotNearest(second, gif, 1, 256) == 0);
    }
}

// Frame time: frames per second -> hundredths of a second.
void testDelay() {
    const int cases[][2] = {
        {10, 10},  // 0.10 s
        {20, 5},   // 0.05 s
        {25, 4},   // 0.04 s
        {30, 3},   // 0.033 s rounds to 0.03 s
        {50, 2},   // 0.02 s
        {60, 2},   // 0.0167 s rounds to 0.02 s
        {1000, 2}, // never below 0.02 s
        {1, 100},  // one second
        {0, 10},   // invalid: treated as 10 fps
        {-5, 10},
    };
    Canvas canvas(4, 4);
    canvas.clear(1, 2, 3);
    for (const auto& c : cases) {
        output::AnimationWriter writer(scratchDirectory(), "delay", 4, 4, c[0]);
        CHECK_MSG(writer.delayCentiseconds() == c[1], "fps " + std::to_string(c[0]));
        CHECK(writer.addFrame(canvas));
        CHECK(writer.addFrame(canvas));
        CHECK(writer.finish());
        const DecodedGif gif = decodeGif(writer.path());
        checkFileStructure(gif, 4, 4, 2);
        checkFrameStructure(gif, 0, 4, 4, c[1]);
        checkFrameStructure(gif, 1, 4, 4, c[1]);
    }
}

// Things that must fail cleanly.
void testErrors() {
    Canvas right(10, 8);
    right.clear(200, 100, 50);
    Canvas wrongWidth(11, 8);
    Canvas wrongHeight(10, 9);

    // A frame of the wrong size is refused and not counted; the file stays valid.
    {
        output::AnimationWriter writer(scratchDirectory(), "wrong_size", 10, 8, 10);
        CHECK(writer.addFrame(right));
        CHECK(!writer.addFrame(wrongWidth));
        CHECK(!writer.addFrame(wrongHeight));
        CHECK(writer.addFrame(right));
        CHECK(writer.frameCount() == 2);
        CHECK(writer.finish());
        const DecodedGif gif = decodeGif(writer.path());
        checkFileStructure(gif, 10, 8, 2);
    }

    // The directory is created when it does not exist, including parents.
    {
        const std::string nested = scratchDirectory() + "/made/by/the/writer";
        std::error_code error;
        std::filesystem::remove_all(scratchDirectory() + "/made", error);
        CHECK(!std::filesystem::exists(nested));
        output::AnimationWriter writer(nested, "nested", 10, 8, 10);
        CHECK(writer.addFrame(right));
        CHECK(writer.finish());
        CHECK(std::filesystem::exists(nested + "/nested.gif"));
        checkFileStructure(decodeGif(nested + "/nested.gif"), 10, 8, 1);
    }

    // A "directory" that is really a file: nothing can be written.
    {
        const std::string blocker = scratchDirectory() + "/blocker";
        { std::ofstream file(blocker); file << "not a directory"; }
        output::AnimationWriter writer(blocker, "impossible", 10, 8, 10);
        CHECK(!writer.addFrame(right));
        CHECK(writer.frameCount() == 0);
        CHECK(!writer.finish());
    }

    // Sizes that a GIF cannot store.
    {
        output::AnimationWriter zero(scratchDirectory(), "zero", 0, 8, 10);
        CHECK(!zero.addFrame(right));
        CHECK(!zero.finish());
        output::AnimationWriter huge(scratchDirectory(), "huge", 70000, 8, 10);
        CHECK(!huge.addFrame(right));
        CHECK(!huge.finish());
    }

    // The low-level writer refuses a frame with the wrong number of indices.
    {
        output::GifWriter gif;
        CHECK(!gif.writeFrame(std::vector<uint8_t>(80, 0))); // not open yet
        CHECK(!gif.close());
        CHECK(gif.open(scratchDirectory() + "/low_level.gif", 10, 8, 7));
        CHECK(!gif.open(scratchDirectory() + "/low_level_again.gif", 10, 8, 7)); // already open
        CHECK(!gif.writeFrame(std::vector<uint8_t>(79, 0)));
        CHECK(gif.writeFrame(std::vector<uint8_t>(80, 1)));
        CHECK(gif.writePalette({Color{9, 8, 7}, Color{1, 2, 3}}));
        CHECK(gif.writeFrame(std::vector<uint8_t>(80, 0))); // frames can follow a palette update
        CHECK(gif.close());
        const DecodedGif decoded = decodeGif(scratchDirectory() + "/low_level.gif");
        checkFileStructure(decoded, 10, 8, 2);
        checkFrameStructure(decoded, 0, 10, 8, 7);
        if (decoded.frames.size() == 2) {
            CHECK(sameColor(decoded.pixel(0, 0, 0), Color{1, 2, 3}));
            CHECK(sameColor(decoded.pixel(1, 0, 0), Color{9, 8, 7}));
        }
    }

    // A writer that is destroyed without finish() still leaves a complete file.
    {
        std::string path;
        {
            output::AnimationWriter writer(scratchDirectory(), "not_finished", 10, 8, 10);
            path = writer.path();
            CHECK(writer.addFrame(right));
        }
        const DecodedGif gif = decodeGif(path);
        checkFileStructure(gif, 10, 8, 1);
        if (gif.frames.size() == 1) {
            CHECK(countDifferences(right, gif, 0) == 0);
        }
    }

    // No frames at all: still a well-formed (empty) GIF.
    {
        output::AnimationWriter writer(scratchDirectory(), "empty", 10, 8, 10);
        CHECK(writer.finish());
        checkFileStructure(decodeGif(writer.path()), 10, 8, 0);
    }
}

} // namespace

int main() {
    std::error_code error;
    std::filesystem::remove_all(scratchDirectory(), error);

    testLzwRoundTrip();
    testAnimationRoundTrip();
    testLargeFrames();
    testTinyImages();
    testFixedPaletteFallback();
    testPaletteFreezesWhenFull();
    testDelay();
    testErrors();

    std::filesystem::remove_all(scratchDirectory(), error);
    return test::summary("test_gif");
}
