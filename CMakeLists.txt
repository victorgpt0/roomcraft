cmake_minimum_required(VERSION 3.20)
project(RoomCraft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

function(roomcraft_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- Checkpoint 1: pure C++ software rasterizer -----------------------------
# Standard library only. No OpenGL/GLFW/GLM/ImGui here: everything writes
# directly to Canvas's own pixel buffer, exported as PPM and BMP images.
add_library(raster STATIC
    src/raster/canvas.cpp
    src/raster/line.cpp
    src/raster/polygon.cpp
    src/raster/clipping.cpp
    src/raster/fill.cpp
    src/raster/circle.cpp
    src/raster/ellipse.cpp
    src/raster/curves.cpp
)
target_include_directories(raster PUBLIC src)
roomcraft_warnings(raster)

# RoomCraft editor logic built on the rasterizer (furniture hit testing and
# room-fit checks).
add_library(roomcraft STATIC
    src/roomcraft/furniture.cpp
)
target_link_libraries(roomcraft PUBLIC raster)
roomcraft_warnings(roomcraft)

# Demo: writes one image per technique plus a full floor plan.
set(ROOMCRAFT_DEMO_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/demo_output)
file(MAKE_DIRECTORY ${ROOMCRAFT_DEMO_OUTPUT_DIR})
add_executable(roomcraft_demo demo/roomcraft_demo.cpp)
target_link_libraries(roomcraft_demo PRIVATE roomcraft)
target_compile_definitions(roomcraft_demo PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(roomcraft_demo)

# --- Tests ------------------------------------------------------------------
# Each test_* executable runs real checks (tests/test_helpers.h) and returns
# non-zero on failure, so `ctest` reports pass/fail. They also write images
# to tests/output/ as visual evidence.
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/output)
enable_testing()

# add_raster_test(name [extra libraries...])
function(add_raster_test test_name)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE raster ${ARGN})
    target_compile_definitions(${test_name} PRIVATE
        TESTS_OUTPUT_DIR="${CMAKE_SOURCE_DIR}/tests/output"
    )
    roomcraft_warnings(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_raster_test(test_canvas)
add_raster_test(test_line)
add_raster_test(test_polygon)
add_raster_test(test_clipping_fill)
add_raster_test(test_circle_ellipse)
add_raster_test(test_curves)
add_raster_test(test_furniture roomcraft)

# The demo must run and write all its images without errors.
add_test(NAME roomcraft_demo COMMAND roomcraft_demo ${ROOMCRAFT_DEMO_OUTPUT_DIR})
