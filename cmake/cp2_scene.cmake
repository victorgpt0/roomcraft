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

# Demo: fan, swivel chair, hinged door and the extruded room, written to
# demo_output/cp2/ as animated GIFs.
add_executable(cp2_scene_demo demo/cp2_scene_demo.cpp)
target_link_libraries(cp2_scene_demo PRIVATE scene roomcraft output)
target_compile_definitions(cp2_scene_demo PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(cp2_scene_demo)
add_test(NAME cp2_scene_demo COMMAND cp2_scene_demo ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp2)
