#include "output/gif.h"

#include <unordered_map>

namespace output {

namespace {

// GIF stores multi-byte numbers little-endian (lowest byte first).
void putU16(std::vector<uint8_t>& bytes, int value) {
    bytes.push_back(static_cast<uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void putText(std::vector<uint8_t>& bytes, const char* text) {
    for (const char* c = text; *c != '\0'; ++c) {
        bytes.push_back(static_cast<uint8_t>(*c));
    }
}

bool writeBytes(std::ofstream& file, const std::vector<uint8_t>& bytes) {
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

// The fixed position of the global colour table: it comes straight after the
// 6-byte header and the 7-byte logical screen descriptor.
constexpr std::streamoff kPaletteOffset = 13;

// LZW codes are not whole bytes: they are 3 to 12 bits long and are packed
// one after another, lowest bit first. This collects the bits and hands out
// complete bytes.
class BitPacker {
public:
    explicit BitPacker(std::vector<uint8_t>& out) : out_(out) {}

    void put(int code, int bitCount) {
        buffer_ |= static_cast<uint32_t>(code) << bitsHeld_;
        bitsHeld_ += bitCount;
        while (bitsHeld_ >= 8) {
            out_.push_back(static_cast<uint8_t>(buffer_ & 0xFFu));
            buffer_ >>= 8;
            bitsHeld_ -= 8;
        }
    }

    // Writes the last, partly filled byte (the unused high bits are 0).
    void flush() {
        if (bitsHeld_ > 0) {
            out_.push_back(static_cast<uint8_t>(buffer_ & 0xFFu));
            buffer_ = 0;
            bitsHeld_ = 0;
        }
    }

private:
    std::vector<uint8_t>& out_;
    uint32_t buffer_ = 0; // at most 7 + 12 bits are held at once
    int bitsHeld_ = 0;
};

} // namespace

// LZW (Lempel-Ziv-Welch) compression
// ----------------------------------
// The idea: pictures repeat themselves. A row of floor pixels is the same
// index hundreds of times. LZW replaces SEQUENCES of indices that it has
// seen before by a single number, called a code.
//
// The code table (the "dictionary"):
//   - It starts with one code per palette index: code 5 means "the single
//     pixel with index 5". With 8-bit indices these are codes 0..255.
//   - Two special codes follow: CLEAR (256) = "forget the table, start
//     again", and END (257) = "no more data".
//   - Every new sequence gets the next free code: 258, 259, 260, ...
//
// The loop. Keep a `prefix`: the longest sequence read so far that IS
// already in the table. Read the next pixel `c`:
//   - if (prefix + c) is in the table, it becomes the new prefix: keep
//     reading, the match is getting longer;
//   - if not, output the code of prefix, ADD (prefix + c) to the table as a
//     new code, and start again with prefix = c.
// So the table learns one longer sequence each time a code is written, and
// a run of identical pixels is matched in pieces of length 1, 2, 3, 4, ...
//
// The table is never stored in the file. The decoder rebuilds exactly the
// same table from the codes it reads, because it can make the same
// "add (previous sequence + first pixel of this one)" step.
//
// A sequence is stored as the pair (code of its prefix, last pixel), so the
// table is a map from that pair to the new code; no pixel lists are copied.
//
// Growing code size: codes start at minCodeSize + 1 bits (9 bits for a
// 256-colour image: enough for 0..511). When the table has handed out code
// 511, the next code needs 10 bits, then 11, then 12. Small images therefore
// use short codes.
//
// Table full: GIF codes are at most 12 bits, so the table holds 4096 codes.
// When the last one has been handed out, we write CLEAR and start a fresh
// table. That also lets the table adapt when the picture changes character
// further down.
std::vector<uint8_t> lzwCompress(const std::vector<uint8_t>& indices, int minCodeSize) {
    if (minCodeSize < 2) {
        minCodeSize = 2;
    }
    if (minCodeSize > 8) {
        minCodeSize = 8;
    }
    constexpr int kMaxCodes = 4096; // 12-bit codes: 0..4095

    const int clearCode = 1 << minCodeSize;
    const int endCode = clearCode + 1;
    const int firstFreeCode = clearCode + 2;

    std::vector<uint8_t> out;
    BitPacker bits(out);

    // (prefix code << 8 | next pixel) -> code of that longer sequence.
    std::unordered_map<uint32_t, int> table;
    int nextCode = firstFreeCode;
    int codeSize = minCodeSize + 1;

    // A stream always begins with CLEAR, telling the decoder to start with
    // an empty table.
    bits.put(clearCode, codeSize);

    if (!indices.empty()) {
        int prefix = indices[0]; // a single pixel is always in the table

        for (std::size_t i = 1; i < indices.size(); ++i) {
            const uint8_t c = indices[i];
            const uint32_t key = (static_cast<uint32_t>(prefix) << 8) | c;
            const auto found = table.find(key);
            if (found != table.end()) {
                prefix = found->second; // seen before: extend the match
                continue;
            }

            bits.put(prefix, codeSize);

            if (nextCode < kMaxCodes) {
                table[key] = nextCode++;
                // The decoder builds its table one step behind us, so it
                // switches to longer codes exactly when the code we have
                // just handed out no longer fits in codeSize bits.
                if (nextCode > (1 << codeSize) && codeSize < 12) {
                    ++codeSize;
                }
            } else {
                // Table full: tell the decoder to forget everything.
                bits.put(clearCode, codeSize);
                table.clear();
                nextCode = firstFreeCode;
                codeSize = minCodeSize + 1;
            }
            prefix = c;
        }

        bits.put(prefix, codeSize); // the last match
    }

    bits.put(endCode, codeSize);
    bits.flush();
    return out;
}

bool GifWriter::open(const std::string& path, int width, int height, int delayCentiseconds) {
    if (file_.is_open()) {
        return false;
    }
    if (width < 1 || width > 65535 || height < 1 || height > 65535) {
        return false; // sizes are stored as 16-bit numbers
    }
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_) {
        return false;
    }
    width_ = width;
    height_ = height;
    delayCentiseconds_ = delayCentiseconds < 0 ? 0 : (delayCentiseconds > 65535 ? 65535 : delayCentiseconds);

    std::vector<uint8_t> bytes;

    // Header: signature and version.
    putText(bytes, "GIF89a");

    // Logical Screen Descriptor: the size of the whole animation.
    putU16(bytes, width_);
    putU16(bytes, height_);
    // Packed byte 1111 0111:
    //   bit 7     = 1   a global colour table follows
    //   bits 6-4  = 111 colour resolution: 8 bits per channel
    //   bit 3     = 0   the table is not sorted by importance
    //   bits 2-0  = 111 table size = 2^(7+1) = 256 entries
    bytes.push_back(0xF7);
    bytes.push_back(0); // background colour index
    bytes.push_back(0); // pixel aspect ratio: 0 = square pixels

    // Global Colour Table: 256 x (r, g, b). Black for now; writePalette()
    // overwrites it once the colours are known.
    bytes.insert(bytes.end(), kPaletteSize * 3, 0);

    // Application Extension "NETSCAPE2.0": makes the animation loop. Without
    // it viewers play the frames once and stop on the last one.
    bytes.push_back(0x21); // extension introducer
    bytes.push_back(0xFF); // application extension
    bytes.push_back(11);   // length of the name
    putText(bytes, "NETSCAPE2.0");
    bytes.push_back(3);    // length of the data sub-block
    bytes.push_back(1);    // sub-block id: loop count follows
    putU16(bytes, 0);      // loop count: 0 = forever
    bytes.push_back(0);    // end of the extension

    if (!writeBytes(file_, bytes)) {
        file_.close();
        return false;
    }
    return true;
}

bool GifWriter::writeFrame(const std::vector<uint8_t>& indices) {
    if (!file_.is_open()) {
        return false;
    }
    if (indices.size() != static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_)) {
        return false;
    }

    std::vector<uint8_t> bytes;

    // Graphic Control Extension: applies to the image that follows.
    bytes.push_back(0x21); // extension introducer
    bytes.push_back(0xF9); // graphic control
    bytes.push_back(4);    // 4 bytes of data
    // Packed byte 0000 0100: disposal method 1 ("leave the frame in place";
    // every frame covers the whole screen anyway), no user input, no
    // transparent colour.
    bytes.push_back(0x04);
    putU16(bytes, delayCentiseconds_); // how long to show this frame, in 1/100 s
    bytes.push_back(0);    // transparent colour index (unused)
    bytes.push_back(0);    // end of the extension

    // Image Descriptor: this frame covers the whole screen.
    bytes.push_back(0x2C);
    putU16(bytes, 0);       // left
    putU16(bytes, 0);       // top
    putU16(bytes, width_);
    putU16(bytes, height_);
    bytes.push_back(0);     // no local colour table (use the global one), not interlaced

    // Image Data: the LZW minimum code size, then the code stream cut into
    // sub-blocks. Each sub-block starts with its length (1..255), and a
    // length of 0 ends the image. This lets a reader skip an image without
    // decompressing it.
    constexpr int kMinCodeSize = 8; // palette indices are 8 bits (256 colours)
    bytes.push_back(kMinCodeSize);
    const std::vector<uint8_t> codes = lzwCompress(indices, kMinCodeSize);
    for (std::size_t offset = 0; offset < codes.size(); offset += 255) {
        const std::size_t length = codes.size() - offset < 255 ? codes.size() - offset : 255;
        bytes.push_back(static_cast<uint8_t>(length));
        bytes.insert(bytes.end(), codes.begin() + static_cast<std::ptrdiff_t>(offset),
                     codes.begin() + static_cast<std::ptrdiff_t>(offset + length));
    }
    bytes.push_back(0); // block terminator

    return writeBytes(file_, bytes);
}

bool GifWriter::writePalette(const std::vector<raster::Color>& palette) {
    if (!file_.is_open()) {
        return false;
    }
    std::vector<uint8_t> bytes(kPaletteSize * 3, 0);
    for (std::size_t i = 0; i < palette.size() && i < static_cast<std::size_t>(kPaletteSize); ++i) {
        bytes[i * 3] = palette[i].r;
        bytes[i * 3 + 1] = palette[i].g;
        bytes[i * 3 + 2] = palette[i].b;
    }
    const std::ofstream::pos_type end = file_.tellp();
    file_.seekp(kPaletteOffset);
    const bool ok = writeBytes(file_, bytes);
    file_.seekp(end);
    return ok && file_.good();
}

bool GifWriter::close() {
    if (!file_.is_open()) {
        return false;
    }
    file_.put(static_cast<char>(0x3B)); // Trailer: end of the GIF
    const bool ok = file_.good();
    file_.close();
    return ok && !file_.fail();
}

} // namespace output
