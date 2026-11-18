# Checkpoint 3: OpenGL integration and the interactive prototype.
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# ---- Editor core: no OpenGL, so it builds and is tested everywhere ----------
# Triangulation, the furniture catalogue, the layout model, CPU mesh
# building, ray picking, the camera rig and the undoable editing operations.
add_library(editor STATIC
    src/editor/triangulate.cpp
    src/editor/catalogue.cpp
    src/editor/layout.cpp
    src/editor/mesh.cpp
    src/editor/camera_rig.cpp
    src/editor/picking.cpp
    src/editor/editor.cpp
)
target_include_directories(editor PUBLIC src)
target_link_libraries(editor PUBLIC roomcraft view scene anim)
roomcraft_warnings(editor)

add_raster_test(test_triangulate editor)
add_raster_test(test_layout editor)
target_compile_definitions(test_layout PRIVATE ROOMCRAFT_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/layouts")
add_raster_test(test_mesh editor)
add_raster_test(test_camera_picking editor)
add_raster_test(test_editor editor)
