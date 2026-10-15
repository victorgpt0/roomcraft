#include "raster/polygon.h"
#include "roomcraft/furniture.h"

#include "test_helpers.h"

#include <vector>

namespace {

// A 5.6 m x 4.0 m room (1 px = 1 cm), as in the demo floor plan.
const raster::Polygon kRoom = raster::makeRectangle(40, 40, 560, 400);

roomcraft::Furniture make(const char* name, raster::Polygon footprint) {
    return roomcraft::Furniture{name, footprint, raster::Color{200, 200, 200}};
}

roomcraft::Furniture makeLSofa() {
    raster::Polygon l;
    l.vertices = {{380, 60}, {580, 60}, {580, 220}, {530, 220}, {530, 110}, {380, 110}};
    return make("L-sofa", l);
}

void testHitTest() {
    const roomcraft::Furniture bed = make("bed", raster::makeRectangle(60, 60, 140, 200));
    CHECK(roomcraft::hitTest(bed, {100, 150}));
    CHECK(!roomcraft::hitTest(bed, {250, 150}));

    // Concave footprint: clicking in the inner corner of an L-shaped sofa
    // must NOT select it.
    const roomcraft::Furniture sofa = makeLSofa();
    CHECK(roomcraft::hitTest(sofa, {450, 80}));   // back of the sofa
    CHECK(roomcraft::hitTest(sofa, {555, 180}));  // side of the sofa
    CHECK(!roomcraft::hitTest(sofa, {450, 160})); // inner corner (empty floor)

    // Round table: footprint is a 48-sided polygon.
    const roomcraft::Furniture table = make("table", roomcraft::makeCircleFootprint({200, 360}, 45, 48));
    CHECK(roomcraft::hitTest(table, {200, 360}));
    CHECK(roomcraft::hitTest(table, {230, 390}));  // distance 42 < 45
    CHECK(!roomcraft::hitTest(table, {235, 395})); // distance 49 > 45
}

void testFindFurnitureAt() {
    // A rug with a coffee table on top of it: clicking the table picks the
    // table (drawn later = on top), clicking the rug elsewhere picks the rug.
    std::vector<roomcraft::Furniture> pieces = {
        make("rug", raster::makeRectangle(100, 100, 200, 100)),
        make("coffee table", raster::makeRectangle(150, 125, 100, 50)),
        make("bed", raster::makeRectangle(400, 100, 100, 150)),
    };
    CHECK(roomcraft::findFurnitureAt(pieces, {200, 150}) == 1);
    CHECK(roomcraft::findFurnitureAt(pieces, {110, 110}) == 0);
    CHECK(roomcraft::findFurnitureAt(pieces, {450, 200}) == 2);
    CHECK(roomcraft::findFurnitureAt(pieces, {10, 10}) == -1);
    CHECK(roomcraft::findFurnitureAt({}, {10, 10}) == -1);
}

void testFitsInRoom() {
    // Fully inside.
    CHECK(roomcraft::fitsInRoom(make("bed", raster::makeRectangle(60, 60, 140, 200)), kRoom));
    CHECK(roomcraft::fitsInRoom(makeLSofa(), kRoom));

    // Touching the walls exactly from inside still fits.
    CHECK(roomcraft::fitsInRoom(make("wardrobe", raster::makeRectangle(40, 40, 100, 60)), kRoom));
    CHECK(roomcraft::fitsInRoom(make("whole room", raster::makeRectangle(40, 40, 560, 400)), kRoom));

    // Sticking 20 cm through the right wall: does not fit; 3/4 of it is inside.
    const roomcraft::Furniture shelf = make("bookshelf", raster::makeRectangle(520, 250, 100, 40));
    CHECK(!roomcraft::fitsInRoom(shelf, kRoom));
    CHECK_NEAR(roomcraft::areaInsideRoom(shelf, kRoom), 80.0 * 40.0, 1e-2);

    // Completely outside.
    const roomcraft::Furniture outside = make("outside", raster::makeRectangle(700, 50, 50, 50));
    CHECK(!roomcraft::fitsInRoom(outside, kRoom));
    CHECK(roomcraft::areaInsideRoom(outside, kRoom) == 0.0f);

    // Rotating a piece near a corner can push a corner through the wall.
    raster::Polygon desk = raster::makeRectangle(45, 45, 120, 60);
    CHECK(roomcraft::fitsInRoom(make("desk", desk), kRoom));
    raster::rotate(desk, 30.0f, raster::computeCentroid(desk));
    CHECK(!roomcraft::fitsInRoom(make("rotated desk", desk), kRoom));

    // Degenerate footprint.
    CHECK(!roomcraft::fitsInRoom(make("nothing", raster::Polygon{}), kRoom));
}

void testCircleFootprint() {
    const raster::Polygon round = roomcraft::makeCircleFootprint({0, 0}, 50, 360);
    CHECK(round.vertices.size() == 360);
    // A regular polygon with many sides has almost the area of the circle
    // (pi r^2 = 7853.98; the 360-gon is smaller by about 0.005%).
    CHECK_NEAR(raster::computeArea(round), 7853.98, 1.0);
    CHECK(roomcraft::makeCircleFootprint({0, 0}, 10, 1).vertices.size() == 3);
}

} // namespace

int main() {
    testHitTest();
    testFindFurnitureAt();
    testFitsInRoom();
    testCircleFootprint();
    return test::summary("test_furniture");
}
