#pragma once

#include "output/gif.h"
#include "raster/canvas.h"
#include "raster/types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Frame-by-frame animation output.
//
// An animation is a sequence of still pictures shown quickly one after
// another. Each picture (frame) is drawn completely on a Canvas with the
// Checkpoint 1 algorithms, handed to addFrame(), and then the canvas is
// cleared and the next frame is drawn with everything moved a little.
//
//   output::AnimationWriter gif("demo_output/cp2", "door", 640, 480, 20);
//   for (int frame = 0; frame < 60; ++frame) {
//       canvas.clear(255, 255, 255);
//       ... draw the scene for this moment ...
//       gif.addFrame(canvas);
//   }
//   gif.finish();
//
// Each frame is compressed and written to the file as soon as it is added;
// frames are not kept in memory, so long animations cost no extra memory.
//
// Colours
// -------
// A GIF holds at most 256 different colours for the whole animation. The
// rasterizer draws flat colours, so normally far fewer are used, and every
// colour is stored exactly: each new colour seen gets the next free palette
// entry. If that is not enough:
//   - the FIRST frame has more than 256 colours (a gradient, say): the whole
//     animation uses a fixed palette that covers all of colour space evenly
//     (8 levels of red x 8 of green x 4 of blue), and every pixel is mapped
//     to the nearest entry;
//   - a LATER frame brings the total over 256: the palette is frozen as it
//     is, and each further new colour is mapped to the nearest colour
//     already in it. (The earlier frames are already in the file, so their
//     palette cannot be replaced.)
// Either way there is ONE palette for the whole file, the same for every
// frame. Call useFixedPalette() before the first frame if you know later
// frames will be full of colours that the first frame does not show.

namespace output {

class AnimationWriter {
public:
    // Creates <directory>/<name>.gif (looping). The directory is created if
    // it does not exist. framesPerSecond <= 0 is treated as 10.
    //
    // GIF measures frame time in hundredths of a second, so the real speed is
    // the nearest value that fits: 10 fps = 0.10 s, 20 fps = 0.05 s, 30 fps
    // becomes 0.03 s (33 fps). Delays under 0.02 s are raised to 0.02 s
    // (50 fps), because browsers slow anything faster down to 10 fps.
    AnimationWriter(const std::string& directory, const std::string& name,
                    int width, int height, int framesPerSecond);

    // Finishes the file if finish() was not called.
    ~AnimationWriter();

    AnimationWriter(const AnimationWriter&) = delete;
    AnimationWriter& operator=(const AnimationWriter&) = delete;

    // Compresses the canvas and appends it as the next frame. Returns false
    // if the canvas size differs from the animation's, if the file could not
    // be created or written, or after finish().
    bool addFrame(const raster::Canvas& canvas);

    // Writes the final palette and the trailer and closes the file. Returns
    // true if the whole file was written. Calling it again returns the same
    // result and does nothing.
    bool finish();

    // Use the fixed 8x8x4 palette from the start. Only has an effect before
    // the first frame is added.
    void useFixedPalette();

    const std::string& path() const { return path_; }
    int frameCount() const { return frameCount_; }
    // Frame time actually stored in the file, in hundredths of a second.
    int delayCentiseconds() const { return delayCentiseconds_; }
    // Number of palette entries in use (256 with the fixed palette).
    int colorCount() const { return static_cast<int>(palette_.size()); }
    bool usesFixedPalette() const { return fixedPalette_; }
    // True once some colour has been replaced by its nearest palette colour.
    bool approximatesColors() const { return approximates_; }

private:
    bool convertFrame(const raster::Canvas& canvas, bool allowRestart);
    int nearestPaletteIndex(raster::Color color) const;

    GifWriter gif_;
    std::string path_;
    int width_;
    int height_;
    int delayCentiseconds_;
    int frameCount_ = 0;
    bool ok_ = false;
    bool finished_ = false;
    bool finishResult_ = false;

    std::vector<raster::Color> palette_;             // entry -> colour
    std::unordered_map<uint32_t, uint8_t> indexOf_;  // colour -> entry (exact or nearest)
    bool fixedPalette_ = false;
    bool approximates_ = false;
    bool paletteChanged_ = false;
    std::vector<uint8_t> indices_;                   // the current frame as palette indices
};

} // namespace output
