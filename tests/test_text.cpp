// Tests for output::drawText and the built-in 5x7 bitmap font.

#include "output/text.h"
#include "raster/canvas.h"

#include "test_helpers.h"

#include <string>

using raster::Canvas;
using raster::Color;

namespace {

const Color kInk{10, 20, 30};

bool isInk(const Canvas& canvas, int x, int y) {
    const Color c = canvas.getPixel(x, y);
    return c.r == kInk.r && c.g == kInk.g && c.b == kInk.b;
}

int countInk(const Canvas& canvas) {
    int count = 0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            count += isInk(canvas, x, y) ? 1 : 0;
        }
    }
    return count;
}

Canvas blank(int width, int height) {
    Canvas canvas(width, height);
    canvas.clear(255, 255, 255);
    return canvas;
}

// Compares the 5x7 cell block at (x, y), drawn at `scale`, with a picture
// given as 7 strings of '#' and '.'. Returns the number of wrong pixels.
int compareGlyph(const Canvas& canvas, int x, int y, int scale, const char* const rows[7]) {
    int wrong = 0;
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            const bool expected = rows[row][column] == '#';
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    if (isInk(canvas, x + column * scale + dx, y + row * scale + dy) != expected) {
                        ++wrong;
                    }
                }
            }
        }
    }
    return wrong;
}

const char* const kGlyphA[7] = {
    ".###.",
    "#...#",
    "#...#",
    "#...#",
    "#####",
    "#...#",
    "#...#",
};
const char* const kGlyphSeven[7] = {
    "#####",
    "....#",
    "...#.",
    "..#..",
    ".#...",
    ".#...",
    ".#...",
};
const char* const kGlyphLowerG[7] = { // the tail stays inside the 7 rows
    ".....",
    ".####",
    "#...#",
    "#...#",
    ".####",
    "....#",
    ".###.",
};
const char* const kGlyphBox[7] = {
    "#####",
    "#...#",
    "#...#",
    "#...#",
    "#...#",
    "#...#",
    "#####",
};
const char* const kGlyphEmpty[7] = {".....", ".....", ".....", ".....", ".....", ".....", "....."};

void testExactGlyphs() {
    // 'A' at (3, 2): every one of the 35 cells is right, and nothing is
    // drawn outside the cell block.
    Canvas canvas = blank(20, 16);
    output::drawText(canvas, 3, 2, "A", kInk);
    CHECK(compareGlyph(canvas, 3, 2, 1, kGlyphA) == 0);
    CHECK(countInk(canvas) == 18); // 'A' has 18 cells on
    CHECK(isInk(canvas, 4, 2));    // top row starts one cell in
    CHECK(!isInk(canvas, 3, 2));   // top-left corner is empty
    CHECK(isInk(canvas, 3, 8));    // bottom-left foot
    CHECK(!isInk(canvas, 3, 9));   // nothing below row 7

    canvas = blank(20, 16);
    output::drawText(canvas, 0, 0, "7", kInk);
    CHECK(compareGlyph(canvas, 0, 0, 1, kGlyphSeven) == 0);

    canvas = blank(20, 16);
    output::drawText(canvas, 5, 5, "g", kInk);
    CHECK(compareGlyph(canvas, 5, 5, 1, kGlyphLowerG) == 0);

    // The colour is the one asked for.
    canvas = blank(20, 16);
    output::drawText(canvas, 0, 0, "A", Color{200, 0, 100});
    const Color c = canvas.getPixel(1, 0);
    CHECK(c.r == 200 && c.g == 0 && c.b == 100);

    // A space draws nothing but still takes up a character.
    canvas = blank(30, 16);
    output::drawText(canvas, 0, 0, " A", kInk);
    CHECK(compareGlyph(canvas, 0, 0, 1, kGlyphEmpty) == 0);
    CHECK(compareGlyph(canvas, 6, 0, 1, kGlyphA) == 0);

    // Characters advance by 6 pixels: 5 for the glyph and 1 empty column.
    canvas = blank(40, 16);
    output::drawText(canvas, 1, 1, "A7A", kInk);
    CHECK(compareGlyph(canvas, 1, 1, 1, kGlyphA) == 0);
    CHECK(compareGlyph(canvas, 7, 1, 1, kGlyphSeven) == 0);
    CHECK(compareGlyph(canvas, 13, 1, 1, kGlyphA) == 0);
    for (int y = 0; y < 16; ++y) {
        CHECK(!isInk(canvas, 6, y));  // the gap columns are empty
        CHECK(!isInk(canvas, 12, y));
    }
}

void testUnknownCharacters() {
    // Characters outside space..~ are drawn as a box, not skipped.
    Canvas canvas = blank(40, 16);
    output::drawText(canvas, 0, 0, std::string("A\tA"), kInk);
    CHECK(compareGlyph(canvas, 0, 0, 1, kGlyphA) == 0);
    CHECK(compareGlyph(canvas, 6, 0, 1, kGlyphBox) == 0);
    CHECK(compareGlyph(canvas, 12, 0, 1, kGlyphA) == 0);

    // Bytes above 127 (for example each byte of a UTF-8 character) too.
    canvas = blank(40, 16);
    output::drawText(canvas, 0, 0, std::string("\xC3\xA9"), kInk); // e-acute in UTF-8: 2 bytes
    CHECK(compareGlyph(canvas, 0, 0, 1, kGlyphBox) == 0);
    CHECK(compareGlyph(canvas, 6, 0, 1, kGlyphBox) == 0);
    CHECK(output::textWidth("\xC3\xA9") == 11);

    canvas = blank(40, 16);
    output::drawText(canvas, 0, 0, std::string(1, '\x7F'), kInk); // DEL, just past '~'
    CHECK(compareGlyph(canvas, 0, 0, 1, kGlyphBox) == 0);
}

void testEveryGlyphIsDistinct() {
    // Every required character has a glyph: it draws something (except
    // space), stays inside its 5x7 cell, and differs from every other one.
    const std::string required =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,:-+x/%()";
    std::string all;
    for (char c = ' '; c <= '~'; ++c) {
        all.push_back(c);
    }
    CHECK(all.size() == 95);
    for (const char c : required) {
        CHECK(all.find(c) != std::string::npos);
    }

    std::string pictures[95];
    for (std::size_t i = 0; i < all.size(); ++i) {
        Canvas canvas = blank(9, 11);
        output::drawText(canvas, 2, 2, std::string(1, all[i]), kInk);
        int inside = 0;
        for (int y = 0; y < 11; ++y) {
            for (int x = 0; x < 9; ++x) {
                const bool ink = isInk(canvas, x, y);
                const bool inCell = x >= 2 && x < 7 && y >= 2 && y < 9;
                if (ink && !inCell) {
                    CHECK_MSG(false, std::string("glyph draws outside its cell: ") + all[i]);
                }
                if (inCell) {
                    pictures[i].push_back(ink ? '#' : '.');
                    inside += ink ? 1 : 0;
                }
            }
        }
        if (all[i] == ' ') {
            CHECK(inside == 0);
        } else {
            CHECK_MSG(inside > 0, std::string("empty glyph: ") + all[i]);
        }
    }
    int identicalPairs = 0;
    for (std::size_t i = 0; i < all.size(); ++i) {
        for (std::size_t j = i + 1; j < all.size(); ++j) {
            if (pictures[i] == pictures[j]) {
                ++identicalPairs;
                CHECK_MSG(false, std::string("identical glyphs: ") + all[i] + " and " + all[j]);
            }
        }
    }
    CHECK(identicalPairs == 0);

    // No real glyph looks like the "missing" box.
    Canvas canvas = blank(9, 11);
    output::drawText(canvas, 2, 2, std::string(1, '\x01'), kInk);
    std::string box;
    for (int y = 2; y < 9; ++y) {
        for (int x = 2; x < 7; ++x) {
            box.push_back(isInk(canvas, x, y) ? '#' : '.');
        }
    }
    for (std::size_t i = 0; i < all.size(); ++i) {
        CHECK(pictures[i] != box);
    }
}

void testTextWidth() {
    CHECK(output::textWidth("") == 0);
    CHECK(output::textWidth("A") == 5);
    CHECK(output::textWidth("AB") == 11);         // 5 + 1 + 5
    CHECK(output::textWidth("frame 12") == 47);   // 8 * 6 - 1
    CHECK(output::textWidth("A", 2) == 10);
    CHECK(output::textWidth("AB", 3) == 33);
    CHECK(output::textWidth("AB", 0) == 11);      // scale below 1 is treated as 1
    CHECK(output::textWidth("AB", -4) == 11);
    CHECK(output::textHeight() == 7);
    CHECK(output::textHeight(3) == 21);
    CHECK(output::textHeight(0) == 7);

    // textWidth matches what drawText really draws: the rightmost inked
    // column of "HELLO, x 100%" is at x + width - 1, the leftmost at x.
    for (int scale = 1; scale <= 4; ++scale) {
        const std::string text = "HELLO, x 100%";
        Canvas canvas = blank(400, 40);
        output::drawText(canvas, 7, 3, text, kInk, scale);
        int left = 1000, right = -1, top = 1000, bottom = -1;
        for (int y = 0; y < canvas.height(); ++y) {
            for (int x = 0; x < canvas.width(); ++x) {
                if (isInk(canvas, x, y)) {
                    left = x < left ? x : left;
                    right = x > right ? x : right;
                    top = y < top ? y : top;
                    bottom = y > bottom ? y : bottom;
                }
            }
        }
        CHECK(left == 7);                                          // 'H' fills its left column
        CHECK(right == 7 + output::textWidth(text, scale) - 1);    // '%' fills its right column
        CHECK(top == 3);
        CHECK(bottom == 3 + output::textHeight(scale) - 1);
    }
}

void testScaling() {
    // At scale 3 every font cell is a 3x3 block: same picture, 9x the ink.
    Canvas small = blank(60, 30);
    output::drawText(small, 0, 0, "A7g", kInk, 1);
    Canvas big = blank(60, 30);
    output::drawText(big, 0, 0, "A7g", kInk, 3);
    CHECK(countInk(big) == 9 * countInk(small));
    CHECK(compareGlyph(big, 0, 0, 3, kGlyphA) == 0);
    CHECK(compareGlyph(big, 18, 0, 3, kGlyphSeven) == 0); // advance is 6 * 3
    CHECK(compareGlyph(big, 36, 0, 3, kGlyphLowerG) == 0);

    // Scale 2 at an offset.
    Canvas two = blank(60, 30);
    output::drawText(two, 5, 4, "A", kInk, 2);
    CHECK(compareGlyph(two, 5, 4, 2, kGlyphA) == 0);
    CHECK(countInk(two) == 4 * 18);

    // Scale 0 and negative scales behave like scale 1.
    Canvas zero = blank(60, 30);
    output::drawText(zero, 0, 0, "A", kInk, 0);
    CHECK(compareGlyph(zero, 0, 0, 1, kGlyphA) == 0);
    CHECK(countInk(zero) == 18);
    Canvas negative = blank(60, 30);
    output::drawText(negative, 0, 0, "A", kInk, -2);
    CHECK(countInk(negative) == 18);
}

void testClipping() {
    // Text running off every edge must not crash, and the part on the
    // canvas must be the same pixels as when it is drawn on a bigger one.
    const std::string text = "RoomCraft 5.6 m x 4.0 m (100%)";
    const int positions[][2] = {{-40, 5}, {5, -3}, {30, 14}, {-200, -200}, {500, 500}, {-7, 17}, {55, -6}};
    for (const auto& p : positions) {
        for (int scale = 1; scale <= 3; ++scale) {
            Canvas tiny = blank(60, 20);
            output::drawText(tiny, p[0], p[1], text, kInk, scale);

            // Reference: the same text on a canvas with a 600-pixel margin.
            Canvas reference = blank(60 + 1200, 20 + 1200);
            output::drawText(reference, p[0] + 600, p[1] + 600, text, kInk, scale);
            int mismatches = 0;
            for (int y = 0; y < 20; ++y) {
                for (int x = 0; x < 60; ++x) {
                    if (isInk(tiny, x, y) != isInk(reference, x + 600, y + 600)) {
                        ++mismatches;
                    }
                }
            }
            CHECK(mismatches == 0);
        }
    }

    // Completely off the canvas: nothing is drawn.
    Canvas canvas = blank(20, 20);
    output::drawText(canvas, 100, 100, "A", kInk);
    output::drawText(canvas, -100, 0, "A", kInk);
    output::drawText(canvas, 0, -100, "A", kInk, 5);
    CHECK(countInk(canvas) == 0);

    // An empty string and a huge scale are fine too.
    output::drawText(canvas, 0, 0, "", kInk);
    CHECK(countInk(canvas) == 0);
    output::drawText(canvas, 0, 0, "A", kInk, 1000); // one cell covers the canvas
    CHECK(countInk(canvas) == 0); // the top-left cell of 'A' is empty
    output::drawText(canvas, -1000, 0, "A", kInk, 1000); // now the second cell, which is on
    CHECK(countInk(canvas) == 400);
}

} // namespace

int main() {
    testExactGlyphs();
    testUnknownCharacters();
    testEveryGlyphIsDistinct();
    testTextWidth();
    testScaling();
    testClipping();
    return test::summary("test_text");
}
