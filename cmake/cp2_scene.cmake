# Checkpoint 2, Part 2: hierarchy and 3D geometry (see docs/cp2/README.md).
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# Scene graph nodes (local shapes placed by parent-relative poses) and
# extrusion from the 2D floor plan into 3D wireframes.
add_library(scene STATIC
    src/scene/node.cpp
    src/scene/extrude.cpp
)
target_include_directories(scene PUBLIC src)
target_link_libraries(scene PUBLIC xform)
roomcraft_warnings(scene)

add_raster_test(test_scene scene)
