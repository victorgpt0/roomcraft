#include "output/animation_writer.h"

#include <filesystem>
#include <system_error>

namespace output {

namespace {

uint32_t colorKey(raster::Color c) {
    return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
}

// Squared distance between two colours in RGB space. Comparing squared
// distances gives the same "which is nearer" answer as real distances,
// without a square root.
int colorDistanceSquared(raster::Color a, raster::Color b) {
    const int dr = static_cast<int>(a.r) - b.r;
    const int dg = static_cast<int>(a.g) - b.g;
    const int db = static_cast<int>(a.b) - b.b;
    return dr * dr + dg * dg + db * db;
}

// Frames per second -> GIF delay in hundredths of a second, rounded to the
// nearest, and at least 2 (see animation_writer.h).
int delayFromFramesPerSecond(int framesPerSecond) {
    if (framesPerSecond <= 0) {
        framesPerSecond = 10;
    }
    const int delay = (100 + framesPerSecond / 2) / framesPerSecond;
    return delay < 2 ? 2 : delay;
}

// The fixed palette: every combination of 8 red levels, 8 green levels and
// 4 blue levels = 256 colours spread evenly over all of colour space. Blue
// gets fewer levels because the eye is least sensitive to it. The levels are
// evenly spaced from 0 to 255, so black and white are both exactly in it.
// Entry number = red level * 32 + green level * 4 + blue level.
std::vector<raster::Color> makeFixedPalette() {
    std::vector<raster::Color> palette;
    palette.reserve(256);
    for (int r = 0; r < 8; ++r) {
        for (int g = 0; g < 8; ++g) {
            for (int b = 0; b < 4; ++b) {
                palette.push_back(raster::Color{static_cast<uint8_t>((r * 255 + 3) / 7),
                                                static_cast<uint8_t>((g * 255 + 3) / 7),
                                                static_cast<uint8_t>(b * 85)});
            }
        }
    }
    return palette;
}

} // namespace

AnimationWriter::AnimationWriter(const std::string& directory, const std::string& name,
                                 int width, int height, int framesPerSecond)
    : width_(width), height_(height), delayCentiseconds_(delayFromFramesPerSecond(framesPerSecond)) {
    path_ = directory.empty() ? name + ".gif" : directory + "/" + name + ".gif";
    if (!directory.empty()) {
        std::error_code error; // do not throw: a failure shows up as open() failing
        std::filesystem::create_directories(directory, error);
    }
    ok_ = gif_.open(path_, width, height, delayCentiseconds_);
    if (ok_) {
        indices_.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    }
}

AnimationWriter::~AnimationWriter() {
    finish();
}

void AnimationWriter::useFixedPalette() {
    if (frameCount_ > 0 || fixedPalette_) {
        return;
    }
    fixedPalette_ = true;
    palette_ = makeFixedPalette();
    indexOf_.clear();
    paletteChanged_ = true;
}

// Linear search for the palette entry closest to `color`. Only called once
// per distinct colour: the answer is remembered in indexOf_.
int AnimationWriter::nearestPaletteIndex(raster::Color color) const {
    int best = 0;
    int bestDistance = colorDistanceSquared(color, palette_[0]);
    for (std::size_t i = 1; i < palette_.size(); ++i) {
        const int distance = colorDistanceSquared(color, palette_[i]);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// Turns the canvas into palette indices (indices_), adding new colours to
// the palette while there is room.
//
// Returns false only in one case: this is the first frame, it turned out to
// have more than 256 colours, and allowRestart is set. The caller then
// switches to the fixed palette and converts the frame again from the start.
bool AnimationWriter::convertFrame(const raster::Canvas& canvas, bool allowRestart) {
    // Neighbouring pixels usually have the same colour, so remember the last
    // lookup and skip the map for runs.
    uint32_t lastKey = 0;
    uint8_t lastIndex = 0;
    bool haveLast = false;

    std::size_t i = 0;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x, ++i) {
            const raster::Color color = canvas.getPixel(x, y);
            const uint32_t key = colorKey(color);
            if (haveLast && key == lastKey) {
                indices_[i] = lastIndex;
                continue;
            }

            uint8_t index = 0;
            const auto found = indexOf_.find(key);
            if (found != indexOf_.end()) {
                index = found->second;
            } else if (!fixedPalette_ && palette_.size() < static_cast<std::size_t>(GifWriter::kPaletteSize)) {
                // A new colour and a free entry: store the colour exactly.
                index = static_cast<uint8_t>(palette_.size());
                palette_.push_back(color);
                indexOf_[key] = index;
                paletteChanged_ = true;
            } else {
                if (allowRestart) {
                    return false; // first frame, too many colours
                }
                // No room: use the nearest colour the palette does have.
                index = static_cast<uint8_t>(nearestPaletteIndex(color));
                indexOf_[key] = index;
                if (colorDistanceSquared(color, palette_[index]) != 0) {
                    approximates_ = true;
                }
            }

            indices_[i] = index;
            lastKey = key;
            lastIndex = index;
            haveLast = true;
        }
    }
    return true;
}

bool AnimationWriter::addFrame(const raster::Canvas& canvas) {
    if (!ok_ || finished_) {
        return false;
    }
    if (canvas.width() != width_ || canvas.height() != height_) {
        return false;
    }

    // Only the very first frame may still choose the fixed palette, because
    // nothing has been written with the exact palette yet.
    const bool firstFrameUndecided = frameCount_ == 0 && !fixedPalette_;
    if (!convertFrame(canvas, firstFrameUndecided)) {
        useFixedPalette();
        convertFrame(canvas, false);
    }

    if (!gif_.writeFrame(indices_)) {
        ok_ = false;
        return false;
    }
    ++frameCount_;

    // Keep the colour table in the file up to date after every frame, so the
    // file is readable even if the program stops before finish().
    if (paletteChanged_) {
        if (!gif_.writePalette(palette_)) {
            ok_ = false;
            return false;
        }
        paletteChanged_ = false;
    }
    return true;
}

bool AnimationWriter::finish() {
    if (finished_) {
        return finishResult_;
    }
    finished_ = true;
    if (!gif_.isOpen()) {
        finishResult_ = false;
        return false;
    }
    bool written = ok_;
    if (paletteChanged_) {
        written = gif_.writePalette(palette_) && written;
    }
    written = gif_.close() && written;
    finishResult_ = written;
    return finishResult_;
}

} // namespace output
