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

# ---- The OpenGL application -------------------------------------------------
# Needs a C compiler (GLFW and the glad loader are C) and, the first time it
# is configured, an internet connection to download GLFW and Dear ImGui.
# Turn it off with -DROOMCRAFT_BUILD_APP=OFF to build only the libraries and
# tests (for example on a machine without a display).
option(ROOMCRAFT_BUILD_APP "Build the interactive OpenGL app (downloads GLFW and Dear ImGui)" ON)

if(ROOMCRAFT_BUILD_APP)
    enable_language(C)
    include(FetchContent)
    find_package(OpenGL REQUIRED)

    # GLFW: window, OpenGL context, mouse and keyboard.
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    # X11 only on Linux: Wayland sessions run it through XWayland, and this
    # avoids needing the Wayland protocol tools to build.
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    # Dear ImGui: the panels, buttons, sliders and colour pickers.
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glfw imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

    # glad: loads the OpenGL 3.3 core functions at run time. Generated with
    # glad 0.1.36 (see third_party/glad/src/glad.c for the command line).
    add_library(glad STATIC third_party/glad/src/glad.c)
    target_include_directories(glad PUBLIC third_party/glad/include)
    if(UNIX)
        target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
    endif()

    # Our OpenGL layer: shaders, GPU meshes and the scene renderer.
    add_library(gfx STATIC
        src/gfx/shader.cpp
        src/gfx/gpu_mesh.cpp
        src/gfx/renderer.cpp
    )
    target_include_directories(gfx PUBLIC src)
    target_link_libraries(gfx PUBLIC editor glad OpenGL::GL)
    roomcraft_warnings(gfx)

    add_executable(roomcraft_app
        app/main.cpp
        app/app.cpp
        app/ui.cpp
        app/capture.cpp
        app/software_view.cpp
    )
    target_include_directories(roomcraft_app PRIVATE app)
    target_link_libraries(roomcraft_app PRIVATE gfx imgui glfw output view)
    # The example layouts are listed in File > Open wherever the app runs from.
    target_compile_definitions(roomcraft_app PRIVATE ROOMCRAFT_EXAMPLES_DIR="${CMAKE_SOURCE_DIR}/layouts")
    roomcraft_warnings(roomcraft_app)

    # The scripted capture opens a (hidden) window, so it needs a display and
    # a GPU driver. It also checks real mouse/keyboard interaction (see
    # app/capture.cpp) and fails if any check fails. Off by default so that
    # CTest still passes on machines without a display.
    option(ROOMCRAFT_TEST_APP "Run the OpenGL app's scripted capture as a CTest test (needs a display)" OFF)
    if(ROOMCRAFT_TEST_APP)
        add_test(NAME roomcraft_app_capture
                 COMMAND roomcraft_app --size 1440 900 --capture ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp3)
    endif()
    if(WIN32 AND MINGW)
        # Static runtime: the .exe starts from Explorer without MSYS2's DLLs.
        target_link_options(roomcraft_app PRIVATE -static)
    endif()
endif()
