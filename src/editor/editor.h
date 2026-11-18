#pragma once

#include "editor/layout.h"
#include "raster/types.h"

#include <string>
#include <vector>

// The editing operations behind the user interface, with undo and redo.
//
// Everything the user can change goes through this class, so the UI (ImGui
// panels, mouse, keyboard) stays a thin layer and the behaviour can be tested
// without a window.
//
// Undo and redo
// -------------
// Before every change the whole layout is copied onto the UNDO stack (a
// "snapshot"; a layout is a few kilobytes, so copying is simpler and safer
// than recording the inverse of every operation). Undo pops a snapshot and
// pushes the current layout onto the REDO stack; any new change clears the
// redo stack.
//
// A continuous gesture - dragging a piece, or dragging a slider in the
// inspector - must be ONE undo step, not one per frame. The UI opens a
// gesture when it starts (beginGesture: one snapshot) and closes it when the
// mouse is released (endGesture); changes in between take no snapshots.

namespace editor {

class Editor {
public:
    explicit Editor(Layout layout = sampleLayout());

    const Layout& layout() const { return layout_; }
    // Increases on every change, so views can rebuild their meshes only
    // when something changed.
    unsigned revision() const { return revision_; }

    // ---- Settings ----------------------------------------------------------------
    bool snapToGrid = true;
    float gridSize = 10.0f;     // cm; positions snap to multiples of this
    float rotationStep = 15.0f; // degrees per rotate command
    bool keepInsideRoom = true; // dragging cannot push a piece through a wall

    // ---- Selection -----------------------------------------------------------------
    int selectedId() const { return selectedId_; }
    void select(int id);         // -1 = nothing
    int indexOf(int id) const;   // -1 if absent
    const Item* selected() const;
    const Item* find(int id) const;

    // ---- Changes (each one undoable) -------------------------------------------------
    // Adds a piece of a catalogue type near `preferred` (room coordinates),
    // at the nearest free spot where it fits without overlapping. Selects it
    // and returns its id (-1 for an unknown type).
    int addItem(const std::string& typeId, raster::Point preferred);
    bool removeSelected();
    int duplicateSelected();     // returns the copy's id, -1 if nothing selected
    void rotateSelected(float degrees);
    void nudgeSelected(float dx, float dy);
    void bringToFront();         // last in the list: drawn on top in the plan
    // Replaces an item (matched by id) - used by the inspector. Part of the
    // current gesture if one is open, otherwise its own undo step.
    void updateItem(const Item& item);
    void setRoom(const Room& room);
    void replaceLayout(const Layout& layout); // New / Open

    // ---- Gestures ------------------------------------------------------------------------
    void beginGesture();         // one snapshot; ignored if one is open
    void endGesture();
    bool gestureOpen() const { return gestureOpen_; }

    // Mouse drag: grab the item at `grabPoint` (room coordinates), move it
    // so the grabbed point follows the mouse, release.
    void beginDrag(int id, raster::Point grabPoint);
    void dragTo(raster::Point point);
    void endDrag();
    bool dragging() const { return dragId_ >= 0; }

    // ---- Undo / redo ------------------------------------------------------------------------
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();

    // ---- Checks (cached per revision) ----------------------------------------------------------
    const std::vector<ItemStatus>& status() const;
    int problemCount() const; // pieces outside the room or overlapping

    // ---- Files ---------------------------------------------------------------------------------
    bool save(const std::string& path) const;
    bool load(const std::string& path, std::string* message = nullptr);

    // Applies grid snapping to a coordinate.
    float snap(float value) const;

private:
    struct Snapshot {
        Layout layout;
        int selectedId;
    };

    void checkpoint();            // snapshot unless a gesture is open
    void changed();               // revision++
    Item* mutableItem(int id);
    bool spotIsFree(const Item& candidate) const;

    Layout layout_;
    unsigned revision_ = 1;
    int selectedId_ = -1;
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    bool gestureOpen_ = false;
    std::vector<Snapshot> gestureRedo_;

    int dragId_ = -1;
    raster::Point dragOffset_{}; // item position - grab point

    mutable unsigned statusRevision_ = 0;
    mutable std::vector<ItemStatus> status_;

    static constexpr std::size_t kMaxUndo = 200;
};

} // namespace editor
