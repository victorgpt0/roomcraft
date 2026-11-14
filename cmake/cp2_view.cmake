# Checkpoint 2, Part 3: viewing and projection (see docs/cp2/README.md).
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# 2D camera (window to viewport, pan, zoom), 4x4 matrices and the 3D orbit
# camera with the wireframe pipeline. Links xform for Mat3/Vec3 and roomcraft
# for Furniture (drawFurnitureThroughCamera); roomcraft itself links xform.
add_library(view STATIC
    src/view/camera2d.cpp
    src/view/mat4.cpp
    src/view/camera3d.cpp
)
target_include_directories(view PUBLIC src)
target_link_libraries(view PUBLIC xform roomcraft)
roomcraft_warnings(view)

add_raster_test(test_view view)

# Demo: pan/zoom, minimap, orbit, ortho vs perspective and the top-down
# reveal, written as GIFs to demo_output/cp2/.
add_executable(cp2_view_demo demo/cp2_view_demo.cpp)
target_link_libraries(cp2_view_demo PRIVATE view output)
target_compile_definitions(cp2_view_demo PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(cp2_view_demo)
add_test(NAME cp2_view_demo COMMAND cp2_view_demo ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp2)
