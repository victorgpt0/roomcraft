cmake_minimum_required(VERSION 3.20)
project(RoomCraft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Checkpoint 1: pure C++ software rasterizer -----------------------------
# Standard library only. No OpenGL/GLFW/GLM/ImGui here: everything writes
# directly to Canvas's own pixel buffer, exported as PPM to prove it works.
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

if(MSVC)
    target_compile_options(raster PRIVATE /W4)
else()
    target_compile_options(raster PRIVATE -Wall -Wextra)
endif()

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/output)

function(add_raster_test test_name)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE raster)
    target_compile_definitions(${test_name} PRIVATE
        TESTS_OUTPUT_DIR="${CMAKE_SOURCE_DIR}/tests/output"
    )
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W4)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()
endfunction()

add_raster_test(test_line)
add_raster_test(test_polygon)
add_raster_test(test_clipping_fill)
add_raster_test(test_circle_ellipse)
add_raster_test(test_curves)
