#include "editor/editor.h"

#include "editor/triangulate.h"

#include <algorithm>
#include <cmath>

namespace editor {

Editor::Editor(Layout layout) : layout_(std::move(layout)) {}

// ---- Selection -----------------------------------------------------------------------

void Editor::select(int id) {
    selectedId_ = indexOf(id) >= 0 ? id : -1;
}

int Editor::indexOf(int id) const {
    for (std::size_t i = 0; i < layout_.items.size(); ++i) {
        if (layout_.items[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const Item* Editor::find(int id) const {
    const int index = indexOf(id);
    return index >= 0 ? &layout_.items[static_cast<std::size_t>(index)] : nullptr;
}

const Item* Editor::selected() const {
    return find(selectedId_);
}

Item* Editor::mutableItem(int id) {
    const int index = indexOf(id);
    return index >= 0 ? &layout_.items[static_cast<std::size_t>(index)] : nullptr;
}

// ---- Undo machinery ------------------------------------------------------------------

void Editor::checkpoint() {
    if (gestureOpen_) {
        return; // the gesture already saved the state before it started
    }
    undo_.push_back(Snapshot{layout_, selectedId_});
    if (undo_.size() > kMaxUndo) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
}

void Editor::changed() {
    ++revision_;
}

void Editor::beginGesture() {
    if (gestureOpen_) {
        return;
    }
    gestureRedo_ = redo_; // a gesture that changes nothing must not cost the redo history
    checkpoint();
    gestureOpen_ = true;
}

void Editor::endGesture() {
    if (!gestureOpen_) {
        return;
    }
    gestureOpen_ = false;
    // A click that only selected a piece, or a slider touched without
    // changing its value: drop the snapshot instead of leaving an undo step
    // that does nothing.
    if (!undo_.empty() && serialize(undo_.back().layout) == serialize(layout_)) {
        undo_.pop_back();
        redo_ = gestureRedo_;
    }
    gestureRedo_.clear();
}

bool Editor::undo() {
    if (undo_.empty() || dragging()) {
        return false;
    }
    gestureOpen_ = false;
    redo_.push_back(Snapshot{layout_, selectedId_});
    layout_ = undo_.back().layout;
    selectedId_ = undo_.back().selectedId;
    undo_.pop_back();
    select(selectedId_);
    changed();
    return true;
}

bool Editor::redo() {
    if (redo_.empty() || dragging()) {
        return false;
    }
    gestureOpen_ = false;
    undo_.push_back(Snapshot{layout_, selectedId_});
    layout_ = redo_.back().layout;
    selectedId_ = redo_.back().selectedId;
    redo_.pop_back();
    select(selectedId_);
    changed();
    return true;
}

// ---- Changes -------------------------------------------------------------------------

float Editor::snap(float value) const {
    if (!snapToGrid || gridSize <= 0.0f) {
        return value;
    }
    return std::round(value / gridSize) * gridSize;
}

bool Editor::spotIsFree(const Item& candidate) const {
    if (!roomcraft::fitsInRoom(toFurniture(candidate), roomOutline(layout_.room))) {
        return false;
    }
    for (const Item& other : layout_.items) {
        if (collisionArea(candidate, other) > 1.0f) {
            return false; // (rugs never collide, so they may go under things)
        }
    }
    return true;
}

int Editor::addItem(const std::string& typeId, raster::Point preferred) {
    if (findType(typeId) == nullptr) {
        return -1;
    }
    Item item = makeItem(typeId, {snap(preferred.x), snap(preferred.y)}, layout_.nextId);

    // Search outwards from the preferred point, ring by ring on a 10 cm
    // grid, for the nearest spot that is free. If the room is full, keep
    // the preferred point (the piece will be flagged as overlapping).
    if (!spotIsFree(item)) {
        const raster::Point start = item.pose.position;
        bool found = false;
        const float step = 10.0f;
        const int maxRing = static_cast<int>(std::max(layout_.room.width, layout_.room.depth) / step) + 1;
        for (int ring = 1; ring <= maxRing && !found; ++ring) {
            // Candidates on the square ring at this distance, nearest first.
            std::vector<raster::Point> candidates;
            for (int i = -ring; i <= ring; ++i) {
                for (int j = -ring; j <= ring; ++j) {
                    if (std::max(std::abs(i), std::abs(j)) == ring) {
                        candidates.push_back({start.x + static_cast<float>(i) * step, start.y + static_cast<float>(j) * step});
                    }
                }
            }
            std::sort(candidates.begin(), candidates.end(), [&start](raster::Point a, raster::Point b) {
                const float da = (a.x - start.x) * (a.x - start.x) + (a.y - start.y) * (a.y - start.y);
                const float db = (b.x - start.x) * (b.x - start.x) + (b.y - start.y) * (b.y - start.y);
                return da < db;
            });
            for (const raster::Point& p : candidates) {
                item.pose.position = p;
                if (spotIsFree(item)) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            item.pose.position = start;
            clampInsideRoom(item, layout_.room);
        }
    }

    checkpoint();
    layout_.nextId++;
    layout_.items.push_back(item);
    selectedId_ = item.id;
    changed();
    return item.id;
}

bool Editor::removeSelected() {
    const int index = indexOf(selectedId_);
    if (index < 0) {
        return false;
    }
    checkpoint();
    layout_.items.erase(layout_.items.begin() + index);
    selectedId_ = -1;
    changed();
    return true;
}

int Editor::duplicateSelected() {
    const Item* source = selected();
    if (source == nullptr) {
        return -1;
    }
    Item copy = *source;
    copy.id = layout_.nextId;
    copy.pose.position.x += 30.0f;
    copy.pose.position.y += 30.0f;
    if (keepInsideRoom) {
        clampInsideRoom(copy, layout_.room);
    }
    checkpoint();
    layout_.nextId++;
    layout_.items.push_back(copy);
    selectedId_ = copy.id;
    changed();
    return copy.id;
}

void Editor::rotateSelected(float degrees) {
    Item* item = mutableItem(selectedId_);
    if (item == nullptr) {
        return;
    }
    checkpoint();
    float angle = std::fmod(item->pose.angleDegrees + degrees, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    // Keep a rotation that lands within a hair of a whole step exact, so
    // repeated 15-degree turns do not drift.
    const float steps = std::round(angle / 0.5f) * 0.5f;
    item->pose.angleDegrees = std::fabs(steps - angle) < 1e-3f ? steps : angle;
    if (keepInsideRoom) {
        clampInsideRoom(*item, layout_.room);
    }
    changed();
}

void Editor::nudgeSelected(float dx, float dy) {
    Item* item = mutableItem(selectedId_);
    if (item == nullptr) {
        return;
    }
    checkpoint();
    item->pose.position.x += dx;
    item->pose.position.y += dy;
    if (keepInsideRoom) {
        clampInsideRoom(*item, layout_.room);
    }
    changed();
}

void Editor::bringToFront() {
    const int index = indexOf(selectedId_);
    if (index < 0 || index == static_cast<int>(layout_.items.size()) - 1) {
        return;
    }
    checkpoint();
    const Item item = layout_.items[static_cast<std::size_t>(index)];
    layout_.items.erase(layout_.items.begin() + index);
    layout_.items.push_back(item);
    changed();
}

void Editor::updateItem(const Item& updated) {
    Item* item = mutableItem(updated.id);
    if (item == nullptr) {
        return;
    }
    checkpoint();
    *item = updated;
    changed();
}

void Editor::setRoom(const Room& room) {
    checkpoint();
    layout_.room = room;
    changed();
}

void Editor::replaceLayout(const Layout& layout) {
    checkpoint();
    layout_ = layout;
    selectedId_ = -1;
    changed();
}

// ---- Dragging ---------------------------------------------------------------------------

void Editor::beginDrag(int id, raster::Point grabPoint) {
    const Item* item = find(id);
    if (item == nullptr) {
        return;
    }
    beginGesture();
    selectedId_ = id;
    dragId_ = id;
    dragOffset_ = {item->pose.position.x - grabPoint.x, item->pose.position.y - grabPoint.y};
}

void Editor::dragTo(raster::Point point) {
    Item* item = mutableItem(dragId_);
    if (item == nullptr) {
        return;
    }
    const raster::Point before = item->pose.position;
    item->pose.position = {snap(point.x + dragOffset_.x), snap(point.y + dragOffset_.y)};
    if (keepInsideRoom) {
        clampInsideRoom(*item, layout_.room);
    }
    if (item->pose.position.x != before.x || item->pose.position.y != before.y) {
        changed();
    }
}

void Editor::endDrag() {
    dragId_ = -1;
    endGesture();
}

// ---- Checks and files -----------------------------------------------------------------------

const std::vector<ItemStatus>& Editor::status() const {
    if (statusRevision_ != revision_) {
        status_ = checkLayout(layout_);
        statusRevision_ = revision_;
    }
    return status_;
}

int Editor::problemCount() const {
    int count = 0;
    for (const ItemStatus& s : status()) {
        if (!s.fitsInRoom || s.overlaps) {
            ++count;
        }
    }
    return count;
}

bool Editor::save(const std::string& path) const {
    return saveLayout(layout_, path);
}

bool Editor::load(const std::string& path, std::string* message) {
    Layout loaded;
    std::string warning;
    if (!loadLayout(path, loaded, &warning)) {
        if (message != nullptr) {
            *message = "Could not read " + path;
        }
        return false;
    }
    replaceLayout(loaded);
    if (message != nullptr) {
        *message = warning.empty() ? "Opened " + path : "Opened " + path + " (" + warning + ")";
    }
    return true;
}

} // namespace editor
