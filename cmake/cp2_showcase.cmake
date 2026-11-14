# Checkpoint 2, Part 4b: integration showcase (see docs/cp2/README.md).
# Included from the root CMakeLists.txt; paths are relative to the repo root.

# One continuous animation plus a shorter GIF for each scene. The executable
# links every CP2 library and roomcraft (for Furniture and fitsInRoom).
add_executable(cp2_showcase demo/cp2_showcase.cpp)
target_link_libraries(cp2_showcase PRIVATE xform output scene view anim roomcraft)
target_compile_definitions(cp2_showcase PRIVATE
    ROOMCRAFT_DEMO_OUTPUT_DIR="${ROOMCRAFT_DEMO_OUTPUT_DIR}"
)
roomcraft_warnings(cp2_showcase)

# Writing all seven GIFs is the test: it fails if any file cannot be written.
add_test(NAME cp2_showcase COMMAND cp2_showcase ${ROOMCRAFT_DEMO_OUTPUT_DIR}/cp2)
