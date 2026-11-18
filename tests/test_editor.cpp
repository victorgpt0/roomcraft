// Tests for the Editor: adding, selecting, dragging, snapping, undo/redo.

#include "editor/editor.h"
#include "editor/layout.h"
#include "raster/polygon.h"

#include "test_helpers.h"

#include <cmath>
#include <filesystem>
#include <string>

namespace {

editor::Layout emptyRoom() {
    editor::Layout layout;
    return layout;
}

void testAddAndFreeSpot() {
    editor::Editor e(emptyRoom());
    const unsigned r0 = e.revision();
    const int a = e.addItem("desk", {280, 210});
    CHECK(a > 0);
    CHECK(e.revision() > r0);
    CHECK(e.selectedId() == a);
    CHECK(e.layout().items.size() == 1);
    CHECK_NEAR(e.selected()->pose.position.x, 280, 0);

    // A second desk at the same spot goes to the nearest FREE spot instead.
    const int b = e.addItem("desk", {280, 210});
    CHECK(b != a);
    const editor::Item* second = e.find(b);
    CHECK(second != nullptr);
    CHECK(second->pose.position.x != 280.0f || second->pose.position.y != 210.0f);
    CHECK(e.problemCount() == 0);
    // ...and it is near: within a desk's size.
    const float moved = std::hypot(second->pose.position.x - 280.0f, second->pose.position.y - 210.0f);
    CHECK(moved <= 80.0f);

    // A rug may go under things.
    const int rug = e.addItem("rug", {280, 210});
    CHECK_NEAR(e.find(rug)->pose.position.x, 280, 0);

    // Near a wall: pushed inside.
    const int w = e.addItem("wardrobe", {5, 5});
    CHECK(e.status()[static_cast<std::size_t>(e.indexOf(w))].fitsInRoom);

    CHECK(e.addItem("spaceship", {0, 0}) == -1);
    CHECK(e.layout().items.size() == 4);
}

void testSelectionAndCommands() {
    editor::Editor e(emptyRoom());
    const int id = e.addItem("bed_double", {280, 210});
    e.select(-1);
    CHECK(e.selected() == nullptr);
    e.select(9999);
    CHECK(e.selectedId() == -1);
    e.select(id);
    CHECK(e.selected() != nullptr);

    // Rotation in steps, wrapping at 360 and never negative.
    e.rotateSelected(90);
    CHECK_NEAR(e.selected()->pose.angleDegrees, 90, 0);
    for (int i = 0; i < 24; ++i) {
        e.rotateSelected(15);
    }
    CHECK_NEAR(e.selected()->pose.angleDegrees, 90, 0); // 24 * 15 = 360: exactly back
    e.rotateSelected(-180);
    CHECK_NEAR(e.selected()->pose.angleDegrees, 270, 0);

    // Duplicate: a new id, offset, selected.
    const int copy = e.duplicateSelected();
    CHECK(copy != id && e.selectedId() == copy);
    CHECK(e.layout().items.size() == 2);

    // Bring to front moves it last.
    e.select(id);
    e.bringToFront();
    CHECK(e.layout().items.back().id == id);

    // Nudge, clamped by the walls.
    e.select(copy);
    const float x = e.selected()->pose.position.x;
    e.nudgeSelected(5, 0);
    CHECK_NEAR(e.selected()->pose.position.x, x + 5, 1e-3);
    e.nudgeSelected(-10000, 0);
    CHECK_NEAR(raster::computeBoundingBox(editor::itemFootprint(*e.selected())).xmin, 0.0, 1e-3);

    // Remove.
    CHECK(e.removeSelected());
    CHECK(e.selectedId() == -1);
    CHECK(e.find(copy) == nullptr);
    CHECK(!e.removeSelected());
    CHECK(e.duplicateSelected() == -1);
}

void testDragAndSnap() {
    editor::Editor e(emptyRoom());
    const int id = e.addItem("chair", {200, 200});
    e.gridSize = 10.0f;

    // Grab the chair 7 cm right of its centre and move the mouse: the grab
    // point follows the mouse, the centre snaps to the 10 cm grid.
    e.beginDrag(id, {207, 200});
    CHECK(e.dragging());
    e.dragTo({330, 263});
    // centre = mouse - 7 = (323, 263) -> snapped (320, 260)
    CHECK_NEAR(e.selected()->pose.position.x, 320, 0);
    CHECK_NEAR(e.selected()->pose.position.y, 260, 0);
    e.snapToGrid = false;
    e.dragTo({330, 263});
    CHECK_NEAR(e.selected()->pose.position.x, 323, 1e-4);
    CHECK_NEAR(e.selected()->pose.position.y, 263, 1e-4);

    // Dragging through the wall is stopped at the wall.
    e.dragTo({2000, 263});
    CHECK_NEAR(raster::computeBoundingBox(editor::itemFootprint(*e.selected())).xmax, e.layout().room.width, 1e-3);
    e.keepInsideRoom = false;
    e.dragTo({2000, 263});
    CHECK(!e.status()[0].fitsInRoom);
    e.endDrag();
    CHECK(!e.dragging());

    // The whole drag is ONE undo step.
    CHECK(e.undo());
    CHECK_NEAR(e.selected()->pose.position.x, 200, 0);
    CHECK_NEAR(e.selected()->pose.position.y, 200, 0);
    // The step before that is the add.
    CHECK(e.undo());
    CHECK(e.layout().items.empty());
    CHECK(!e.canUndo());

    // A click without movement adds no undo step and keeps redo.
    CHECK(e.redo());
    CHECK(e.canRedo());
    e.beginDrag(id, {200, 200});
    e.endDrag();
    CHECK(e.canRedo());
    CHECK(e.undo());
    CHECK(e.layout().items.empty());
}

void testUndoRedo() {
    editor::Editor e(emptyRoom());
    CHECK(!e.canUndo() && !e.canRedo());
    CHECK(!e.undo() && !e.redo());

    const int a = e.addItem("desk", {150, 150});
    e.rotateSelected(90);
    const std::string afterRotate = editor::serialize(e.layout());

    CHECK(e.undo());
    CHECK_NEAR(e.find(a)->pose.angleDegrees, 0, 0);
    CHECK(e.redo());
    CHECK(editor::serialize(e.layout()) == afterRotate);
    CHECK(e.selectedId() == a);

    // A new change clears redo.
    CHECK(e.undo());
    e.nudgeSelected(10, 0);
    CHECK(!e.canRedo());

    // Inspector gesture: many updates, one step.
    editor::Item item = *e.find(a);
    e.beginGesture();
    for (int i = 0; i < 50; ++i) {
        item.pose.scaleX = 1.0f + 0.01f * static_cast<float>(i);
        e.updateItem(item);
    }
    e.endGesture();
    CHECK_NEAR(e.find(a)->pose.scaleX, 1.49, 1e-4);
    CHECK(e.undo());
    CHECK_NEAR(e.find(a)->pose.scaleX, 1.0, 0);

    // Room change and replaceLayout are undoable too.
    editor::Room room = e.layout().room;
    room.width = 800;
    e.setRoom(room);
    CHECK_NEAR(e.layout().room.width, 800, 0);
    e.replaceLayout(editor::sampleLayout());
    CHECK(e.selectedId() == -1);
    CHECK(e.undo());
    CHECK_NEAR(e.layout().room.width, 800, 0);
    CHECK(e.undo());
    CHECK_NEAR(e.layout().room.width, 560, 0);

    // The revision changes on every change and on undo/redo.
    const unsigned r = e.revision();
    e.redo();
    CHECK(e.revision() != r);
}

void testStatusCache() {
    editor::Editor e(emptyRoom());
    const int a = e.addItem("desk", {150, 150});
    e.addItem("desk", {400, 150});
    CHECK(e.problemCount() == 0);
    // Put the first desk onto the second: both overlap.
    editor::Item item = *e.find(a);
    item.pose.position = {400, 150};
    e.updateItem(item);
    CHECK(e.problemCount() == 2);
    CHECK(e.status()[0].overlaps && e.status()[1].overlaps);
    e.undo();
    CHECK(e.problemCount() == 0);
}

void testFiles() {
    editor::Editor e;
    const std::string path = (std::filesystem::temp_directory_path() / "roomcraft_test_editor.rclayout").string();
    CHECK(e.save(path));
    editor::Editor other(emptyRoom());
    std::string message;
    CHECK(other.load(path, &message));
    CHECK(message.find("Opened") == 0);
    CHECK(editor::serialize(other.layout()) == editor::serialize(e.layout()));
    CHECK(other.canUndo()); // opening a file can be undone
    std::filesystem::remove(path);
    CHECK(!other.load(path, &message));
    CHECK(message.find("Could not") == 0);
}

} // namespace

int main() {
    testAddAndFreeSpot();
    testSelectionAndCommands();
    testDragAndSnap();
    testUndoRedo();
    testStatusCache();
    testFiles();
    return test::summary("test_editor");
}
