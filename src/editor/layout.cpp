#include "editor/layout.h"

#include "editor/triangulate.h"
#include "xform/mat3.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace editor {

std::vector<Opening> openings(const Room& room) {
    std::vector<Opening> result;

    // Door: 90 cm wide, 210 cm tall, in the bottom wall, 50 cm from the
    // right-hand corner (or centred if the room is narrow).
    const float doorWidth = std::min(90.0f, room.width - 20.0f);
    float doorStart = room.width - 50.0f - doorWidth;
    if (doorStart < 10.0f) {
        doorStart = (room.width - doorWidth) / 2.0f;
    }
    result.push_back(Opening{Wall::Bottom, doorStart, doorStart + doorWidth, 0.0f,
                             std::min(210.0f, room.wallHeight - 10.0f), true});

    // Window: centred in the top wall, from 90 cm up to 30 cm below the
    // ceiling, at most 180 cm wide.
    const float windowWidth = std::min(180.0f, room.width - 80.0f);
    if (windowWidth > 20.0f) {
        const float start = (room.width - windowWidth) / 2.0f;
        result.push_back(Opening{Wall::Top, start, start + windowWidth, 90.0f,
                                 std::max(120.0f, room.wallHeight - 30.0f), false});
    }
    return result;
}

raster::Polygon roomOutline(const Room& room) {
    return raster::makeRectangle(0.0f, 0.0f, room.width, room.depth);
}

const FurnitureType& typeOf(const Item& item) {
    const FurnitureType* type = findType(item.typeId);
    return type != nullptr ? *type : catalogue().front();
}

roomcraft::Furniture toFurniture(const Item& item) {
    return roomcraft::Furniture{item.name, typeOf(item).footprint, item.color, item.pose};
}

raster::Polygon itemFootprint(const Item& item) {
    return roomcraft::worldFootprint(toFurniture(item));
}

Item makeItem(const std::string& typeId, raster::Point position, int id) {
    Item item;
    item.id = id;
    item.typeId = typeId;
    const FurnitureType* type = findType(typeId);
    item.name = type != nullptr ? type->name : typeId;
    item.color = type != nullptr ? type->color : raster::Color{200, 200, 200};
    item.pose.position = position;
    return item;
}

Layout sampleLayout() {
    Layout layout;
    const auto add = [&layout](const char* type, float x, float y, float angle) {
        Item item = makeItem(type, {x, y}, layout.nextId++);
        item.pose.angleDegrees = angle;
        layout.items.push_back(item);
    };
    // Sleeping corner, top left.
    add("bed_double", 100, 115, 0);
    add("nightstand", 205, 30, 0);
    add("wardrobe", 32, 300, 90);
    // Living area, right.
    add("rug", 400, 230, 0);
    add("sofa_l", 430, 95, 0);
    add("coffee_table", 395, 230, 0);
    add("tv_unit", 410, 397, 180);
    add("plant", 530, 380, 0);
    add("bookshelf", 260, 400, 180);
    // Dining, bottom left.
    add("table_round", 175, 320, 0);
    add("chair", 175, 245, 180);
    add("chair", 250, 320, 270);
    add("chair", 100, 320, 90);
    add("floor_lamp", 535, 245, 0);
    return layout;
}

namespace {

bool boxesOverlap(const raster::Rect& a, const raster::Rect& b) {
    return a.xmin < b.xmax && b.xmin < a.xmax && a.ymin < b.ymax && b.ymin < a.ymax;
}

} // namespace

float collisionArea(const Item& a, const Item& b) {
    const FurnitureType& typeA = typeOf(a);
    const FurnitureType& typeB = typeOf(b);
    if (typeA.floorLevel || typeB.floorLevel) {
        return 0.0f;
    }
    // Cheap rejection on the whole footprints first.
    if (!boxesOverlap(raster::computeBoundingBox(itemFootprint(a)), raster::computeBoundingBox(itemFootprint(b)))) {
        return 0.0f;
    }
    const xform::Mat3 placeA = xform::poseMatrix(a.pose);
    const xform::Mat3 placeB = xform::poseMatrix(b.pose);
    std::vector<raster::Polygon> partsB;
    std::vector<raster::Rect> boxesB;
    for (const Part& part : typeB.parts) {
        partsB.push_back(xform::transformPolygon(placeB, part.footprint));
        boxesB.push_back(raster::computeBoundingBox(partsB.back()));
    }
    float total = 0.0f;
    for (const Part& partA : typeA.parts) {
        const raster::Polygon planA = xform::transformPolygon(placeA, partA.footprint);
        const raster::Rect boxA = raster::computeBoundingBox(planA);
        const float bottomA = partA.bottom * a.heightScale;
        const float topA = partA.top * a.heightScale;
        for (std::size_t j = 0; j < typeB.parts.size(); ++j) {
            const Part& partB = typeB.parts[j];
            const float bottomB = partB.bottom * b.heightScale;
            const float topB = partB.top * b.heightScale;
            if (!(bottomA < topB && bottomB < topA) || !boxesOverlap(boxA, boxesB[j])) {
                continue; // different heights, or apart on the plan
            }
            total += overlapArea(planA, partsB[j]);
        }
    }
    return total;
}

std::vector<ItemStatus> checkLayout(const Layout& layout) {
    const std::size_t n = layout.items.size();
    std::vector<ItemStatus> status(n);
    const raster::Polygon room = roomOutline(layout.room);

    for (std::size_t i = 0; i < n; ++i) {
        status[i].fitsInRoom = roomcraft::fitsInRoom(toFurniture(layout.items[i]), room);
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            const float area = collisionArea(layout.items[i], layout.items[j]);
            if (area > 1.0f) {
                status[i].overlaps = status[j].overlaps = true;
                status[i].overlapArea += area;
                status[j].overlapArea += area;
            }
        }
    }
    return status;
}

float floorCoverage(const Layout& layout) {
    const float roomArea = layout.room.width * layout.room.depth;
    if (roomArea <= 0.0f) {
        return 0.0f;
    }
    float covered = 0.0f;
    for (const Item& item : layout.items) {
        if (!typeOf(item).floorLevel) {
            covered += raster::computeArea(itemFootprint(item));
        }
    }
    return covered / roomArea;
}

bool clampInsideRoom(Item& item, const Room& room) {
    const raster::Rect box = raster::computeBoundingBox(itemFootprint(item));
    const auto shiftFor = [](float low, float high, float limit) {
        if (high - low > limit) {
            return limit / 2.0f - (low + high) / 2.0f; // too big: centre it
        }
        if (low < 0.0f) {
            return -low;
        }
        if (high > limit) {
            return limit - high;
        }
        return 0.0f;
    };
    const float dx = shiftFor(box.xmin, box.xmax, room.width);
    const float dy = shiftFor(box.ymin, box.ymax, room.depth);
    if (dx == 0.0f && dy == 0.0f) {
        return false;
    }
    item.pose.position.x += dx;
    item.pose.position.y += dy;
    return true;
}

// ---- Saving and loading ---------------------------------------------------------

std::string serialize(const Layout& layout) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    const Room& r = layout.room;
    out << "roomcraft-layout 1\n";
    out << "room " << r.width << ' ' << r.depth << ' ' << r.wallHeight << ' ' << int(r.floorColor.r) << ' '
        << int(r.floorColor.g) << ' ' << int(r.floorColor.b) << ' ' << int(r.wallColor.r) << ' '
        << int(r.wallColor.g) << ' ' << int(r.wallColor.b) << '\n';
    for (const Item& item : layout.items) {
        out << "item " << item.typeId << ' ' << item.pose.position.x << ' ' << item.pose.position.y << ' '
            << item.pose.angleDegrees << ' ' << item.pose.scaleX << ' ' << item.pose.scaleY << ' '
            << item.heightScale << ' ' << int(item.color.r) << ' ' << int(item.color.g) << ' '
            << int(item.color.b) << ' ' << item.name << '\n';
    }
    return out.str();
}

namespace {

bool readColor(std::istringstream& in, raster::Color& color) {
    int r = 0, g = 0, b = 0;
    if (!(in >> r >> g >> b) || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return false;
    }
    color = raster::Color{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
    return true;
}

} // namespace

bool deserialize(const std::string& text, Layout& layout, std::string* warning) {
    std::istringstream lines(text);
    std::string line;
    if (!std::getline(lines, line) || line.rfind("roomcraft-layout 1", 0) != 0) {
        return false;
    }

    Layout result;
    bool haveRoom = false;
    int skipped = 0;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // files edited on Windows
        }
        std::istringstream in(line);
        std::string keyword;
        if (!(in >> keyword) || keyword[0] == '#') {
            continue; // blank line or comment
        }
        if (keyword == "room") {
            Room& r = result.room;
            if (!(in >> r.width >> r.depth >> r.wallHeight) || !readColor(in, r.floorColor) ||
                !readColor(in, r.wallColor) || r.width < 50.0f || r.depth < 50.0f || r.wallHeight < 50.0f ||
                r.width > 5000.0f || r.depth > 5000.0f || r.wallHeight > 1000.0f) {
                return false;
            }
            haveRoom = true;
        } else if (keyword == "item") {
            Item item;
            if (!(in >> item.typeId >> item.pose.position.x >> item.pose.position.y >> item.pose.angleDegrees >>
                  item.pose.scaleX >> item.pose.scaleY >> item.heightScale) ||
                !readColor(in, item.color)) {
                return false;
            }
            std::getline(in >> std::ws, item.name);
            if (findType(item.typeId) == nullptr) {
                ++skipped;
                continue;
            }
            if (item.name.empty()) {
                item.name = findType(item.typeId)->name;
            }
            item.id = result.nextId++;
            result.items.push_back(item);
        } else {
            return false;
        }
    }
    if (!haveRoom) {
        return false;
    }
    if (warning != nullptr) {
        *warning = skipped > 0 ? std::to_string(skipped) + " item(s) of unknown type skipped" : std::string();
    }
    layout = result;
    return true;
}

bool saveLayout(const Layout& layout, const std::string& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file << serialize(layout);
    return file.good();
}

bool loadLayout(const std::string& path, Layout& layout, std::string* warning) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return deserialize(text.str(), layout, warning);
}

} // namespace editor
