// Tests for the furniture catalogue and the layout model (checks, save/load).

#include "editor/catalogue.h"
#include "editor/layout.h"
#include "editor/triangulate.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <cmath>
#include <filesystem>
#include <set>
#include <string>

namespace {

void testCatalogue() {
    const auto& types = editor::catalogue();
    CHECK(types.size() >= 15);

    std::set<std::string> ids;
    for (const editor::FurnitureType& type : types) {
        CHECK_MSG(ids.insert(type.id).second, "duplicate id " + type.id);
        CHECK_MSG(type.id.find(' ') == std::string::npos, "space in id " + type.id); // ids are saved as one word
        CHECK(!type.name.empty() && !type.category.empty());
        CHECK_MSG(type.footprint.vertices.size() >= 3, type.id);
        CHECK_MSG(!type.parts.empty(), type.id);
        CHECK_MSG(type.height > 0.0f, type.id);
        CHECK(editor::findType(type.id) == &type);

        // The footprint is centred: its centroid is near the local origin,
        // so turning a piece turns it on the spot.
        const raster::Point c = raster::computeCentroid(type.footprint);
        CHECK_MSG(std::fabs(c.x) < 0.01f && std::fabs(c.y) < 0.01f, type.id + " not centred");

        // Every part stays within the footprint's bounding box (the 3D model
        // is not bigger than what the plan shows and the checks use), and
        // its heights are ordered and within the type's height.
        const raster::Rect box = raster::computeBoundingBox(type.footprint);
        for (const editor::Part& part : type.parts) {
            const raster::Rect pb = raster::computeBoundingBox(part.footprint);
            CHECK_MSG(pb.xmin >= box.xmin - 0.01f && pb.xmax <= box.xmax + 0.01f && pb.ymin >= box.ymin - 0.01f &&
                          pb.ymax <= box.ymax + 0.01f,
                      type.id + " part outside footprint");
            CHECK_MSG(part.top > part.bottom && part.bottom >= 0.0f, type.id + " part heights");
            CHECK_MSG(part.top <= type.height + 0.01f, type.id + " part taller than the type");
        }
    }
    CHECK(editor::findType("no_such_thing") == nullptr);

    // partColor: shaded item colour, or the part's own colour.
    editor::Part shaded;
    shaded.shade = 0.5f;
    const raster::Color c = editor::partColor(shaded, {200, 100, 40});
    CHECK(c.r == 100 && c.g == 50 && c.b == 20);
    shaded.shade = 2.0f;
    const raster::Color bright = editor::partColor(shaded, {200, 100, 40});
    CHECK(bright.r == 255 && bright.g == 200 && bright.b == 80); // clamped at 255
    editor::Part fixed;
    fixed.useItemColor = false;
    fixed.color = {1, 2, 3};
    const raster::Color f = editor::partColor(fixed, {200, 100, 40});
    CHECK(f.r == 1 && f.g == 2 && f.b == 3);
}

void testItemsAndChecks() {
    // An item's footprint is the type's footprint placed by its pose.
    editor::Item bed = editor::makeItem("bed_double", {300, 200}, 1);
    CHECK(bed.name == "Double bed");
    raster::Rect box = raster::computeBoundingBox(editor::itemFootprint(bed));
    CHECK_NEAR(box.xmin, 220, 1e-3);
    CHECK_NEAR(box.xmax, 380, 1e-3);
    CHECK_NEAR(box.ymin, 97.5, 1e-3);
    CHECK_NEAR(box.ymax, 302.5, 1e-3);
    bed.pose.angleDegrees = 90;
    box = raster::computeBoundingBox(editor::itemFootprint(bed));
    CHECK_NEAR(box.xmax - box.xmin, 205, 1e-2);
    CHECK_NEAR(box.ymax - box.ymin, 160, 1e-2);

    // The sample layout is valid: everything inside, nothing overlapping.
    const editor::Layout sample = editor::sampleLayout();
    CHECK(sample.items.size() >= 12);
    const std::vector<editor::ItemStatus> status = editor::checkLayout(sample);
    for (std::size_t i = 0; i < status.size(); ++i) {
        CHECK_MSG(status[i].fitsInRoom, "sample: " + sample.items[i].name + " outside the room");
        CHECK_MSG(!status[i].overlaps, "sample: " + sample.items[i].name + " overlaps");
    }
    const float coverage = editor::floorCoverage(sample);
    CHECK(coverage > 0.2f && coverage < 0.7f);

    // A chair pushed under a table top does not collide (the seat is lower
    // than the top); pushed further, its backrest hits the top.
    {
        editor::Item table = editor::makeItem("table_rect", {300, 200}, 1);
        editor::Item chair = editor::makeItem("chair", {300, 230}, 2); // backrest towards the table
        chair.pose.angleDegrees = 180;
        CHECK(editor::overlapArea(editor::itemFootprint(table), editor::itemFootprint(chair)) > 100.0f);
        // Backrest (y local -24..-20, turned 180 -> 250..254 world) is outside the top (155..245).
        CHECK_NEAR(editor::collisionArea(table, chair), 0.0, 1e-3);
        chair.pose.position.y = 200; // backrest now at 220..224: inside the top's area, 43..90 cm high
        CHECK(editor::collisionArea(table, chair) > 100.0f);
        // A taller table lifts its top (now 93.6..98.8 cm) above the 90 cm
        // backrest; the chair stands between the legs, so nothing collides.
        table.heightScale = 1.3f;
        CHECK_NEAR(editor::collisionArea(table, chair), 0.0, 1e-3);
    }

    // Two pieces in the same place overlap; a rug under them does not count.
    editor::Layout layout;
    layout.items.push_back(editor::makeItem("desk", {200, 200}, 1));
    layout.items.push_back(editor::makeItem("desk", {250, 210}, 2));
    layout.items.push_back(editor::makeItem("rug", {220, 200}, 3));
    layout.items.push_back(editor::makeItem("chair", {800, 200}, 4)); // outside (room is 560 wide)
    const std::vector<editor::ItemStatus> s = editor::checkLayout(layout);
    CHECK(s[0].overlaps && s[1].overlaps);
    CHECK_NEAR(s[0].overlapArea, 80.0 * 55.0, 1.0); // desks are 130 x 65: overlap (130-50) x (65-10)
    CHECK_NEAR(s[0].overlapArea, s[1].overlapArea, 1e-3);
    CHECK(!s[2].overlaps);
    CHECK(s[0].fitsInRoom && s[2].fitsInRoom);
    CHECK(!s[3].fitsInRoom && !s[3].overlaps);

    // clampInsideRoom moves a piece back through the nearest wall.
    editor::Room room;
    editor::Item chair = editor::makeItem("chair", {-30, 100}, 5);
    CHECK(editor::clampInsideRoom(chair, room));
    box = raster::computeBoundingBox(editor::itemFootprint(chair));
    CHECK_NEAR(box.xmin, 0.0, 1e-3);
    CHECK_NEAR(chair.pose.position.y, 100, 0);
    editor::Item corner = editor::makeItem("wardrobe", {550, 410}, 6);
    CHECK(editor::clampInsideRoom(corner, room));
    box = raster::computeBoundingBox(editor::itemFootprint(corner));
    CHECK_NEAR(box.xmax, room.width, 1e-3);
    CHECK_NEAR(box.ymax, room.depth, 1e-3);
    CHECK(!editor::clampInsideRoom(corner, room)); // already inside: no move
    editor::Room tiny;
    tiny.width = 100;
    tiny.depth = 100;
    editor::Item tooBig = editor::makeItem("bed_double", {10, 10}, 7);
    editor::clampInsideRoom(tooBig, tiny);
    CHECK_NEAR(tooBig.pose.position.x, 50, 1e-3); // centred
    CHECK_NEAR(tooBig.pose.position.y, 50, 1e-3);

    // Door and window follow the room size and stay inside their walls.
    for (const float width : {200.0f, 560.0f, 1200.0f}) {
        editor::Room r;
        r.width = width;
        for (const editor::Opening& o : editor::openings(r)) {
            CHECK(o.start >= 0.0f && o.end <= width && o.end > o.start);
            CHECK(o.top > o.bottom && o.top <= r.wallHeight);
        }
    }
}

void testSaveLoad() {
    editor::Layout layout = editor::sampleLayout();
    layout.room.width = 640.5f;
    layout.room.floorColor = {1, 2, 3};
    layout.items[0].name = "Guest bed with a long name";
    layout.items[0].pose.angleDegrees = 37.25f;
    layout.items[0].pose.scaleX = 0.9f;
    layout.items[0].heightScale = 1.2f;
    layout.items[0].color = {10, 20, 30};

    const std::string text = editor::serialize(layout);
    CHECK(text.rfind("roomcraft-layout 1\n", 0) == 0);

    editor::Layout loaded;
    std::string warning = "unchanged";
    CHECK(editor::deserialize(text, loaded, &warning));
    CHECK(warning.empty());
    CHECK(loaded.items.size() == layout.items.size());
    CHECK_NEAR(loaded.room.width, 640.5, 1e-3);
    CHECK(loaded.room.floorColor.r == 1 && loaded.room.floorColor.b == 3);
    for (std::size_t i = 0; i < layout.items.size() && i < loaded.items.size(); ++i) {
        const editor::Item& a = layout.items[i];
        const editor::Item& b = loaded.items[i];
        CHECK(a.typeId == b.typeId && a.name == b.name);
        CHECK_NEAR(a.pose.position.x, b.pose.position.x, 0.006);
        CHECK_NEAR(a.pose.position.y, b.pose.position.y, 0.006);
        CHECK_NEAR(a.pose.angleDegrees, b.pose.angleDegrees, 0.006);
        CHECK_NEAR(a.pose.scaleX, b.pose.scaleX, 0.006);
        CHECK_NEAR(a.heightScale, b.heightScale, 0.006);
        CHECK(a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b);
    }
    // Saving the loaded layout again gives the same text.
    CHECK(editor::serialize(loaded) == text);
    // Ids are unique and nextId is past them.
    std::set<int> ids;
    for (const editor::Item& item : loaded.items) {
        CHECK(ids.insert(item.id).second);
        CHECK(item.id < loaded.nextId);
    }

    // Through a file.
    const std::string path = (std::filesystem::temp_directory_path() / "roomcraft_test_layout.rclayout").string();
    CHECK(editor::saveLayout(layout, path));
    editor::Layout fromFile;
    CHECK(editor::loadLayout(path, fromFile));
    CHECK(editor::serialize(fromFile) == text);
    std::filesystem::remove(path);
    CHECK(!editor::loadLayout(path, fromFile)); // missing file

    // Windows line endings, comments and blank lines are accepted.
    std::string windows;
    for (const char ch : text) {
        if (ch == '\n') {
            windows += "\r\n";
        } else {
            windows += ch;
        }
    }
    windows.insert(windows.find("\r\nroom") + 2, "# a comment\r\n\r\n");
    editor::Layout fromWindows;
    CHECK(editor::deserialize(windows, fromWindows));
    CHECK(editor::serialize(fromWindows) == text);

    // Unknown furniture types are skipped with a warning.
    editor::Layout partial;
    CHECK(editor::deserialize("roomcraft-layout 1\nroom 500 400 250 1 2 3 4 5 6\n"
                              "item spaceship 10 10 0 1 1 1 1 1 1 Big\n"
                              "item chair 100 100 0 1 1 1 9 9 9\n",
                              partial, &warning));
    CHECK(partial.items.size() == 1);
    CHECK(partial.items[0].name == "Chair"); // empty name -> type name
    CHECK(warning.find("1 item") != std::string::npos);

    // Invalid files are rejected and leave the layout unchanged.
    editor::Layout untouched = editor::sampleLayout();
    const std::string before = editor::serialize(untouched);
    const char* invalid[] = {
        "",
        "not a layout\nroom 500 400 250 1 2 3 4 5 6\n",
        "roomcraft-layout 1\n",                                            // no room
        "roomcraft-layout 1\nroom 500 400\n",                              // truncated
        "roomcraft-layout 1\nroom 500 400 250 1 2 3 4 5 600\n",            // colour > 255
        "roomcraft-layout 1\nroom 10 400 250 1 2 3 4 5 6\n",               // room too small
        "roomcraft-layout 1\nroom 500 400 250 1 2 3 4 5 6\nitem chair 1 2\n",
        "roomcraft-layout 1\nroom 500 400 250 1 2 3 4 5 6\nsofa 1 2 3\n",  // unknown keyword
    };
    for (const char* text2 : invalid) {
        CHECK_MSG(!editor::deserialize(text2, untouched), std::string("accepted: ") + text2);
    }
    CHECK(editor::serialize(untouched) == before);
}

// Every example layout shipped in layouts/ loads, and is a good example:
// everything inside the room and nothing colliding.
void testExampleLayouts() {
    int found = 0;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(ROOMCRAFT_EXAMPLES_DIR, error)) {
        if (entry.path().extension() != ".rclayout") {
            continue;
        }
        ++found;
        const std::string name = entry.path().filename().string();
        editor::Layout layout;
        std::string warning;
        if (!CHECK_MSG(editor::loadLayout(entry.path().string(), layout, &warning), name)) {
            continue;
        }
        CHECK_MSG(warning.empty(), name + ": " + warning);
        CHECK_MSG(!layout.items.empty(), name);
        const std::vector<editor::ItemStatus> status = editor::checkLayout(layout);
        for (std::size_t i = 0; i < status.size(); ++i) {
            CHECK_MSG(status[i].fitsInRoom, name + ": " + layout.items[i].name + " outside the room");
            CHECK_MSG(!status[i].overlaps, name + ": " + layout.items[i].name + " collides");
        }
    }
    CHECK(found >= 2);
}

} // namespace

int main() {
    testCatalogue();
    testItemsAndChecks();
    testSaveLoad();
    testExampleLayouts();
    return test::summary("test_layout");
}
