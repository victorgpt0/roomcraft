# Checkpoint 2, Part 1: transform core (see docs/cp2/README.md).
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# 3x3 homogeneous matrices, small 3D types and Pose.
add_library(xform STATIC
    src/xform/mat3.cpp
    src/xform/pose.cpp
)
target_include_directories(xform PUBLIC src)
target_link_libraries(xform PUBLIC raster)
roomcraft_warnings(xform)

# Furniture now carries a Pose, so the CP1 roomcraft library needs xform.
target_link_libraries(roomcraft PUBLIC xform)

add_raster_test(test_mat3 xform)
add_raster_test(test_pose roomcraft)

# Demo: one still image per transform technique, written to demo_output/cp2/.
add_executable(cp2_transform_demo demo/cp2_transform_demo.cpp)
target_link_libraries(cp2_transform_demo PRIVATE roomcraft)
target_compile_definitions(cp2_transform_demo PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(cp2_transform_demo)
add_test(NAME cp2_transform_demo COMMAND cp2_transform_demo ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp2)
