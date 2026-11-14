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
