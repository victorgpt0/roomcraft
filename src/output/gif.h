#pragma once

#include "raster/types.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// GIF89a file writing and LZW compression, written from scratch.
//
// A GIF image does not store red/green/blue for each pixel. It stores a
// PALETTE (the "colour table") of up to 256 colours once, and then one small
// number per pixel: the index of that pixel's colour in the palette. Those
// indices are compressed with LZW. An animated GIF is simply several such
// images in one file, each with a delay saying how long to show it.
//
// This file is the low-level part: it knows the file format and the
// compression, and works on palette indices. Turning a Canvas of RGB pixels
// into indices (building the palette) is done by AnimationWriter.
//
// RoomCraft use: Checkpoint 2 is about animation, and every animated demo is
// written as a GIF through this code.

namespace output {

// LZW-compresses a row-major list of palette indices into the GIF code
// stream (the raw bytes, before they are cut into sub-blocks).
//
// minCodeSize is the number of bits needed for a palette index: 8 for a
// 256-colour palette. GIF allows 2 to 8; values outside are clamped. Every
// index must be below 2^minCodeSize.
//
// See gif.cpp for how the algorithm works.
std::vector<uint8_t> lzwCompress(const std::vector<uint8_t>& indices, int minCodeSize);

// Writes one animated GIF file, frame by frame.
//
// File layout (see gif.cpp for the bytes of each block):
//
//   Header                    "GIF89a"
//   Logical Screen Descriptor width, height, "a 256-colour global table follows"
//   Global Colour Table       256 x (r, g, b), shared by every frame
//   Application Extension     "NETSCAPE2.0": loop the animation forever
//   for each frame:
//     Graphic Control Ext.    how long to show this frame
//     Image Descriptor        position and size of the frame
//     Image Data              LZW minimum code size, then the compressed
//                             indices in sub-blocks of at most 255 bytes
//   Trailer                   0x3B
//
// Usage: open(), then writeFrame() for each frame, then close().
// writePalette() may be called at any time between open() and close().
class GifWriter {
public:
    static constexpr int kPaletteSize = 256;

    // Creates the file and writes everything up to the first frame. The
    // colour table is written as 256 black entries; fill it in with
    // writePalette(). The delay is in HUNDREDTHS of a second (GIF's unit).
    // Returns false if the size is not in 1..65535 or the file cannot be
    // created.
    bool open(const std::string& path, int width, int height, int delayCentiseconds);

    bool isOpen() const { return file_.is_open(); }

    // Appends one frame. `indices` holds width * height palette indices,
    // row by row from the top. Returns false if the count is wrong or
    // writing fails.
    bool writeFrame(const std::vector<uint8_t>& indices);

    // Overwrites the global colour table in place, near the start of the
    // file, and returns to the end. Entries beyond palette.size() are black;
    // entries beyond 256 are ignored. This is what lets frames be written
    // before the complete palette is known.
    bool writePalette(const std::vector<raster::Color>& palette);

    // Writes the trailer and closes the file. Returns false if anything
    // failed to be written.
    bool close();

private:
    std::ofstream file_;
    int width_ = 0;
    int height_ = 0;
    int delayCentiseconds_ = 0;
};

} // namespace output
