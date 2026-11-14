# Checkpoint 2, Part 4: animation engine (see docs/cp2/README.md).
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# Easing curves, pose interpolation, keyframe tracks and path animation.
add_library(anim STATIC
    src/anim/easing.cpp
    src/anim/track.cpp
    src/anim/path.cpp
)
target_include_directories(anim PUBLIC src)
target_link_libraries(anim PUBLIC xform)
roomcraft_warnings(anim)

add_raster_test(test_anim anim)

# Demo: four animated GIFs showing easing curves, keyframed furniture
# arrangement, wall-collision detection and constant-speed Catmull-Rom path
# motion, written to demo_output/cp2/.
add_executable(cp2_anim_demo demo/cp2_anim_demo.cpp)
target_link_libraries(cp2_anim_demo PRIVATE anim roomcraft output)
target_compile_definitions(cp2_anim_demo PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(cp2_anim_demo)
add_test(NAME cp2_anim_demo COMMAND cp2_anim_demo ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp2)

