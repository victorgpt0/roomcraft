# Checkpoint 2, Part 5: animation output (see docs/cp2/README.md).
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# GIF89a encoder (LZW), frame-by-frame animation writer and bitmap text.
add_library(output STATIC
    src/output/gif.cpp
    src/output/animation_writer.cpp
    src/output/text.cpp
)
target_include_directories(output PUBLIC src)
target_link_libraries(output PUBLIC raster)
roomcraft_warnings(output)

add_raster_test(test_gif output)
add_raster_test(test_text output)

# Demo: a first animation, the font sheet and the fixed-palette fallback,
# written to demo_output/cp2/.
add_executable(cp2_output_demo demo/cp2_output_demo.cpp)
target_link_libraries(cp2_output_demo PRIVATE output)
target_compile_definitions(cp2_output_demo PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(cp2_output_demo)
add_test(NAME cp2_output_demo COMMAND cp2_output_demo ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp2)
