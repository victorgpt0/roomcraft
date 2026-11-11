# RoomCraft CP2 — Transformation and Animation: task prompts

CP2 asks us to build on the CP1 components by implementing **transformations
and animation in pure C++**. This file splits that work into five parts, one
per member, and gives each member a prompt to paste into their AI coding
assistant.

**How to use this file**

1. Find your row in the table below.
2. Pull the latest `dev` branch and open your AI assistant in the repository root.
3. Paste the whole prompt for your part (the text inside the grey box).
4. Stay in the loop: you will present this work on camera, so read what the
   assistant writes and ask it to explain anything that is unclear.

## Who does what

Each part is a GitHub issue with its own branch. The issue contains the same
prompt as this file; the assignee of the issue owns the part.

| Issue | Student ID | Part | Branch | Starts |
|---|---|---|---|---|
| [#4](https://github.com/victorgpt0/roomcraft/issues/4) | 167026 | 1 — Transform core | `cp2/transform` | Wave 1 (now) |
| [#5](https://github.com/victorgpt0/roomcraft/issues/5) | 161309 | 5a — Output (GIF, text) | `cp2/output` | Wave 1 (now) |
| [#6](https://github.com/victorgpt0/roomcraft/issues/6) | 167998 | 2 — Hierarchy and 3D geometry | `cp2/scene` | Wave 2 |
| [#7](https://github.com/victorgpt0/roomcraft/issues/7) | 152502 | 3 — Viewing and projection | `cp2/view` | Wave 2 |
| [#8](https://github.com/victorgpt0/roomcraft/issues/8) | 166913 | 4a — Animation engine | `cp2/anim` | Wave 2 |
| [#9](https://github.com/victorgpt0/roomcraft/issues/9) | 166913 | 4b — Final showcase | `cp2/showcase` | Wave 3 |
| [#10](https://github.com/victorgpt0/roomcraft/issues/10) | 161309 | 5b — README and video script | `cp2/docs` | Wave 3 |

**Waves.** Parts 1 and 5 depend on nothing and start immediately, in parallel.
Parts 2, 3 and 4 use the code from parts 1 and 5, so they start once both are
merged into `dev`. Wave 3 (prompts 4b and 5b) starts once parts 1–4 are merged.

---

## Common rules (every prompt refers to these)

### Pure C++

- C++17, **standard library only**. No OpenGL, SDL, GLFW, GLM, stb, ImageMagick,
  ffmpeg or any other library or external tool. Everything is drawn through
  `raster::Canvas` using the CP1 algorithms.
- Keep the CP1 coordinate convention (`src/raster/types.h`): origin top-left,
  **y grows downward**, 1 pixel = 1 cm, angles in **degrees**, and a positive
  angle turns **clockwise on screen**.
- The build must stay warning-free with `-Wall -Wextra -Wpedantic`.
- Comment each algorithm the way CP1 does: what it does, why it works, and how
  it is used in RoomCraft.

### Files you may touch

Each part owns the files listed in its prompt. Do not edit files owned by
another part or CP1 files, unless the prompt says so. This is what keeps the
five branches free of merge conflicts.

### Build hook

Each part registers its targets in its own file `cmake/cp2_<part>.cmake`. The
root `CMakeLists.txt` includes those files through the block below. If the
block is not in `CMakeLists.txt` yet, append it **at the very end of the file,
copied exactly, character for character** (identical additions on two branches
merge cleanly; different ones conflict). Make no other change to the root
`CMakeLists.txt`.

```cmake

# --- Checkpoint 2 parts: one file per member, see docs/cp2/README.md --------
foreach(cp2_part transform output scene view anim showcase)
    if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/cp2_${cp2_part}.cmake)
        include(${CMAKE_SOURCE_DIR}/cmake/cp2_${cp2_part}.cmake)
    endif()
endforeach()
```

Inside your `.cmake` file, paths are relative to the repository root (for
example `src/xform/mat3.cpp`). You can use the existing helpers
`roomcraft_warnings(target)` and `add_raster_test(test_name [libraries...])`.

### Tests

Every part adds a CTest test with **real assertions** using
`tests/test_helpers.h`, in the style of the CP1 tests. All CP1 tests must
still pass.

### Demo output

- Each part has its own demo executable that writes to `demo_output/cp2/`
  (this folder is git-ignored).
- Copy the two or three best results to `docs/images/cp2/` and commit them, so
  they can be shown in the README and the video. Keep each file under 2 MB.

### Presenter notes

Each part writes `docs/cp2/notes/<part>.md`. For **each technique** in the
part, answer the five questions from the submission guidelines:

1. What technique or algorithm was used?
2. How was it implemented? (name the file and function)
3. Which project component uses it?
4. What does it achieve in the project?
5. Which demo image or GIF shows the result?

End with a **Challenges** section listing the problems that really came up
during this work and how they were solved. Do not invent challenges.

### Git: commit as yourself

Instructions to the AI assistant:

1. Before the first commit, run `git config user.name` and
   `git config user.email`, show me both values, and **ask me to confirm**
   they are my own GitHub identity. If they are not, stop and give me the
   `git config` commands to fix them for this repository; do not set them
   yourself. The email must be one that is verified on my GitHub account (or
   my GitHub `noreply` address), otherwise the commits will not link to my
   profile.
2. Never use `--author` or environment variables to commit as someone else.
3. The branch named in the prompt already exists on `origin`. Run
   `git fetch origin`, check the branch out, and merge the latest
   `origin/dev` into it before starting.
4. Commit in small logical steps (for example: library, tests, demo, notes),
   each with a message that says what changed. Do not make one giant commit.
5. Never push to `dev` or `main` directly, never force-push, and never rewrite
   other people's commits.
6. When everything in "Done when" is true, push the branch
   (`git push -u origin <branch>`; if I work from a fork, push to my fork) and
   open a pull request into `dev` of `victorgpt0/roomcraft`. Use `gh pr create`
   if it is available, otherwise print the URL for me to open. The pull
   request description lists what was built and how to run the demo, and
   contains `Closes #<issue number>` for the issue named in the prompt.

---

## Shared contract

These are the interfaces the parts use from each other. The owner of each
header implements it **exactly as written** (names, namespaces, parameter
order). Owners may add more functions, but may not change or remove these.

### Part 1 owns `src/xform/` (library `xform`, links `raster`)

```cpp
// src/xform/vec.h
namespace xform {
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec4 { float x = 0, y = 0, z = 0, w = 1; };

// Wireframe model: vertices plus index pairs for the edges.
struct WireMesh {
    std::vector<Vec3> vertices;
    std::vector<std::pair<int, int>> edges;
};
}

// src/xform/mat3.h
namespace xform {
// 3x3 homogeneous 2D transform. m[row][column]. Points are COLUMN vectors:
// p' = M * p, so in A * B the transform B is applied first.
struct Mat3 {
    float m[3][3];
    static Mat3 identity();
    static Mat3 translation(float tx, float ty);
    static Mat3 rotation(float angleDegrees);          // clockwise on screen
    static Mat3 scaling(float sx, float sy);
    static Mat3 shear(float shx, float shy);           // x' = x + shx*y, y' = y + shy*x
    static Mat3 reflectionAcrossXAxis();               // y -> -y
    static Mat3 reflectionAcrossYAxis();               // x -> -x
    static Mat3 rotationAbout(float angleDegrees, raster::Point pivot);
    static Mat3 scalingAbout(float sx, float sy, raster::Point pivot);
};
Mat3 operator*(const Mat3& a, const Mat3& b);
raster::Point transformPoint(const Mat3& m, raster::Point p);
raster::Polygon transformPolygon(const Mat3& m, const raster::Polygon& polygon);
float determinant(const Mat3& m);
bool inverse(const Mat3& m, Mat3& out);                // false if not invertible
}

// src/xform/pose.h
namespace xform {
// Where a piece is: its local shape is scaled, then rotated, then moved.
struct Pose {
    raster::Point position{0.0f, 0.0f};
    float angleDegrees = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};
// translation(position) * rotation(angleDegrees) * scaling(scaleX, scaleY)
Mat3 poseMatrix(const Pose& pose);
}
```

Part 1 also extends `src/roomcraft/furniture.h`:

```cpp
struct Furniture {
    std::string name;
    raster::Polygon footprint;   // LOCAL coordinates
    raster::Color color;
    xform::Pose pose;            // new, last member, identity by default
};
// The footprint placed in the room: transformPolygon(poseMatrix(pose), footprint).
raster::Polygon worldFootprint(const Furniture& piece);
```

With the default pose, local coordinates equal room coordinates, so all CP1
code keeps working. New pieces should define their footprint centred on
(0, 0) and be placed with `pose`.

### Part 5 owns `src/output/` (library `output`, links `raster`)

```cpp
// src/output/animation_writer.h
namespace output {
// Collects frames and writes <directory>/<name>.gif (looping).
class AnimationWriter {
public:
    AnimationWriter(const std::string& directory, const std::string& name,
                    int width, int height, int framesPerSecond);
    bool addFrame(const raster::Canvas& canvas);  // false if the size differs or writing fails
    bool finish();                                // writes the trailer and closes the file
};
}

// src/output/text.h
namespace output {
// Built-in bitmap font (5x7 cells). (x, y) is the top-left corner; scale >= 1.
void drawText(raster::Canvas& canvas, int x, int y, const std::string& text,
              raster::Color color, int scale = 1);
int textWidth(const std::string& text, int scale = 1);
}
```

### Part 2 owns `src/scene/` (library `scene`, links `xform`)

```cpp
// src/scene/node.h
namespace scene {
// One part of a hierarchical model. `pose` is relative to the PARENT.
struct Node {
    std::string name;
    raster::Polygon shape;       // local coordinates; may be empty (pure group)
    raster::Color color;
    xform::Pose pose;
    std::vector<Node> children;
};
// Draws the node and its children. world = parent * poseMatrix(node.pose).
void drawNode(raster::Canvas& canvas, const Node& node, const xform::Mat3& parent);
}

// src/scene/extrude.h
namespace scene {
// Lifts a floor-plan polygon into a 3D prism from the floor up to `height`.
xform::WireMesh extrude(const raster::Polygon& worldFootprint, float height);
}
```

**2D-to-3D mapping (parts 2, 3 and 4 all use this):** plan point (x, y) at
height h becomes world point **(X = x, Y = h, Z = y)**. Y points up, units are
centimetres.

### Part 3 owns `src/view/` (library `view`, links `xform`)

```cpp
// src/view/camera2d.h
namespace view {
struct Camera2D {
    raster::Rect window;     // the part of the room to show (room coordinates)
    raster::Rect viewport;   // where it appears on the canvas (pixels)
};
xform::Mat3 windowToViewport(const Camera2D& camera);
}

// src/view/mat4.h
namespace view {
// 4x4 homogeneous 3D transform, column vectors, m[row][column], like Mat3.
struct Mat4 {
    float m[4][4];
    static Mat4 identity();
    static Mat4 translation(float tx, float ty, float tz);
    static Mat4 scaling(float sx, float sy, float sz);
    static Mat4 rotationX(float angleDegrees);
    static Mat4 rotationY(float angleDegrees);
    static Mat4 rotationZ(float angleDegrees);
    static Mat4 lookAt(xform::Vec3 eye, xform::Vec3 target, xform::Vec3 up);
    static Mat4 perspective(float fovYDegrees, float aspect, float zNear, float zFar);
    static Mat4 orthographic(float left, float right, float bottom, float top,
                             float zNear, float zFar);
};
Mat4 operator*(const Mat4& a, const Mat4& b);
xform::Vec4 operator*(const Mat4& m, xform::Vec4 v);
}

// src/view/camera3d.h
namespace view {
// Camera on a sphere around `target` (spherical coordinates).
struct OrbitCamera {
    xform::Vec3 target;
    float radius = 800.0f;
    float azimuthDegrees = 45.0f;
    float elevationDegrees = 30.0f;   // 90 = straight down (the floor-plan view)
};
xform::Vec3 eyePosition(const OrbitCamera& camera);
Mat4 viewMatrix(const OrbitCamera& camera);
// Projects every edge (clipped at the near plane and the canvas) and draws it.
void drawWireMesh(raster::Canvas& canvas, const xform::WireMesh& mesh,
                  const Mat4& viewProjection, raster::Color color);
}
```

### Part 4 owns `src/anim/` (library `anim`, links `xform`)

```cpp
// src/anim/easing.h
namespace anim {
enum class Easing { Linear, EaseIn, EaseOut, EaseInOut };
float ease(Easing easing, float t);          // t and result in [0, 1]
float lerp(float a, float b, float t);
float lerpAngle(float aDegrees, float bDegrees, float t);   // takes the shorter way round
xform::Pose lerpPose(const xform::Pose& a, const xform::Pose& b, float t);
}

// src/anim/track.h
namespace anim {
// Keyframed pose over time. sample() clamps before the first and after the last key.
class PoseTrack {
public:
    void addKey(float timeSeconds, const xform::Pose& pose, Easing easingIntoKey = Easing::EaseInOut);
    xform::Pose sample(float timeSeconds) const;
    float duration() const;
};
}

// src/anim/path.h
namespace anim {
// Position on a Catmull-Rom path through `points` at u in [0, 1], facing along the path.
xform::Pose poseOnPath(const std::vector<raster::Point>& points, float u);
}
```

---

## Prompt 1 — Transform core (167026)

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. Checkpoint 1 is finished. I own Part 1 of
Checkpoint 2: the transform core. I will present this work on camera, so
explain your decisions to me as you go.

First read docs/cp2/README.md. Follow its "Common rules" section exactly
(pure C++, files I may touch, build hook, tests, demo output, presenter notes,
and "Git: commit as yourself"), and implement the Part 1 interfaces in its
"Shared contract" section exactly as written. Also read src/raster/types.h,
src/raster/polygon.h and src/roomcraft/furniture.h to match the CP1 style.
Then build the project and run ctest to confirm everything passes before you
change anything.

Branch: cp2/transform. It already exists on origin; check it out and merge the
latest origin/dev into it before you start.

Files I own:
- src/xform/vec.h, src/xform/mat3.h, src/xform/mat3.cpp, src/xform/pose.h,
  src/xform/pose.cpp
- src/roomcraft/furniture.h and furniture.cpp (extend only, see below)
- cmake/cp2_transform.cmake
- tests/test_mat3.cpp, tests/test_pose.cpp
- demo/cp2_transform_demo.cpp
- docs/cp2/notes/transform.md, docs/images/cp2/transform_*
If adding the pose member causes warnings in existing CP1 files that
initialise Furniture, you may make the smallest possible fix there.

Build:
1. Library `xform` with Vec3, Vec4, WireMesh, Mat3 and Pose from the contract.
   Mat3 uses homogeneous coordinates and column vectors. Mat3::rotation must
   give the same result as the CP1 raster::rotate (positive = clockwise on
   screen, y down). rotationAbout and scalingAbout must be built as composites
   (translate to origin, transform, translate back) and the comments must show
   that derivation. inverse uses the adjugate and determinant.
2. Furniture pose: add `xform::Pose pose` as the last member of
   roomcraft::Furniture, add worldFootprint(), and make hitTest,
   findFurnitureAt, fitsInRoom, areaInsideRoom and drawFurniture work on the
   world footprint. In cp2_transform.cmake, link the existing `roomcraft`
   library to `xform`. All CP1 tests and the CP1 demo must still pass
   unchanged in behaviour.
3. tests/test_mat3.cpp with real assertions, at least:
   - each basic matrix maps known points to hand-computed results;
   - Mat3 rotation, translation and scaling agree with raster::rotate,
     raster::translate and raster::scale on a polygon;
   - order matters: translation * rotation differs from rotation * translation,
     with the expected points for each;
   - rotationAbout leaves the pivot fixed;
   - reflecting twice gives the identity; shear keeps the polygon area
     (determinant 1); scaling multiplies the area by sx * sy;
   - M * inverse(M) is the identity for several composites, and inverse
     returns false for a singular matrix.
   tests/test_pose.cpp: poseMatrix order (scale, then rotate, then move), and
   hit testing and fitsInRoom on a piece that is moved and rotated by its pose.
4. demo/cp2_transform_demo.cpp, writing BMP images to demo_output/cp2/ that
   show, on RoomCraft furniture inside the room outline:
   - transform_basic: an armchair translated, rotated, scaled, reflected and
     sheared, each next to the original drawn as a dashed outline;
   - transform_order: the same two matrices multiplied in both orders;
   - transform_pivot: a door rotated about its hinge in several steps, and a
     piece rotated about its own centre versus about the room corner;
   - transform_mirror: an L-shaped sofa mirrored to fit the opposite corner,
     and a sheared copy used as a drop shadow;
   - transform_pose: the same local footprint placed three times with
     different poses, with fitsInRoom failures drawn in red.
   Register the demo as a CTest test so it must run without errors.
5. Presenter notes in docs/cp2/notes/transform.md, covering: homogeneous
   coordinates, each basic transform, composite transforms and why order
   matters, pivot transforms, inverse, and the pose refactor.

Done when:
- a clean configure and build has no warnings;
- ctest passes every test, old and new;
- the demo images exist and the best ones are copied to docs/images/cp2/;
- the notes are written;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by giving me a short walkthrough of what you built, in the order I
should present it.
```

---

## Prompt 5a — Output: GIF encoder and text (161309)

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. Checkpoint 1 is finished. I own Part 5 of
Checkpoint 2: animation output. CP2 is about animation, but the project can
only write still BMP/PPM images. My job is to let it write animated GIFs and
draw text, using the standard library only. I will present this work on
camera, so explain your decisions to me as you go.

First read docs/cp2/README.md. Follow its "Common rules" section exactly
(pure C++, files I may touch, build hook, tests, demo output, presenter notes,
and "Git: commit as yourself"), and implement the Part 5 interfaces in its
"Shared contract" section exactly as written. Also read src/raster/canvas.h
and canvas.cpp to match the CP1 style. Then build the project and run ctest
to confirm everything passes before you change anything.

Branch: cp2/output. It already exists on origin; check it out and merge the
latest origin/dev into it before you start. This part does not depend on any other
CP2 part.

Files I own:
- src/output/gif.h, gif.cpp, animation_writer.h, animation_writer.cpp,
  text.h, text.cpp
- cmake/cp2_output.cmake
- tests/test_gif.cpp, tests/test_text.cpp
- demo/cp2_output_demo.cpp
- docs/cp2/notes/output.md, docs/images/cp2/output_*

Build:
1. Library `output` (links `raster`).
2. A GIF89a encoder written from scratch: header, logical screen descriptor,
   global colour table, the NETSCAPE2.0 looping extension, and per frame a
   graphic control extension (frame delay), image descriptor and
   LZW-compressed pixel data in sub-blocks. Explain LZW in the comments:
   the code table, clear and end codes, growing code size, and what happens
   when the table is full.
3. Colour handling: the rasterizer draws flat colours, so build the palette
   from the colours actually used. If the animation uses more than 256
   colours, fall back to a fixed palette and map each pixel to the nearest
   entry. The palette must be the same for every frame.
4. output::AnimationWriter as in the contract. Frames must be streamed to the
   file as they are added, not all kept in memory. It creates the directory if
   needed.
5. output::drawText with a built-in 5x7 bitmap font covering at least A-Z,
   a-z, 0-9, space and . , : - + x / % ( ). Unknown characters draw a box.
6. tests/test_gif.cpp with real assertions. Write a small LZW/GIF decoder
   inside the test file, then: encode several known frames, read the file
   back, and check the signature, size, frame count, delay, loop extension
   and that every decoded pixel equals the original. Include a frame of random
   pixels and a frame of one flat colour, a one-pixel-wide image, and the
   more-than-256-colours fallback (check each pixel maps to its nearest
   palette colour). tests/test_text.cpp: exact pixels of one known glyph,
   textWidth, scaling, and that text running off the canvas does not crash.
7. demo/cp2_output_demo.cpp writing to demo_output/cp2/:
   - output_first_animation.gif: the CP1 room outline with a piece of
     furniture sliding across it and a frame counter drawn with drawText;
   - output_font.bmp: the whole font at scale 1, 2 and 3;
   - output_palette.gif: an animation with more than 256 colours to show the
     fallback.
   Register the demo as a CTest test. Open the GIFs in a browser or image
   viewer to confirm they play and loop, and tell me how you checked.
8. Presenter notes in docs/cp2/notes/output.md, covering: frame-by-frame
   animation, the GIF file structure, LZW compression, palette building, and
   the bitmap font.

Done when:
- a clean configure and build has no warnings;
- ctest passes every test, old and new;
- the GIFs play correctly and the best ones are copied to docs/images/cp2/;
- the notes are written;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by giving me a short walkthrough of what you built, in the order I
should present it.
```

---

## Prompt 2 — Hierarchy and 3D geometry (167998)

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. Checkpoint 1 is finished. I own Part 2 of
Checkpoint 2: hierarchical models (a scene graph) and turning the 2D floor
plan into 3D geometry. I will present this work on camera, so explain your
decisions to me as you go.

First read docs/cp2/README.md. Follow its "Common rules" section exactly
(pure C++, files I may touch, build hook, tests, demo output, presenter notes,
and "Git: commit as yourself"), and implement the Part 2 interfaces in its
"Shared contract" section exactly as written.

This part depends on Part 1 (src/xform/) and Part 5 (src/output/). Check that
both exist on origin/dev. If either is missing, stop and tell me; do not write
your own copy. Read their headers, plus src/roomcraft/furniture.h and
demo/roomcraft_demo.cpp for the furniture shapes. Then build the project and
run ctest to confirm everything passes before you change anything.

Branch: cp2/scene. It already exists on origin; check it out and merge the
latest origin/dev into it before you start.

Files I own:
- src/scene/node.h, node.cpp, extrude.h, extrude.cpp
- cmake/cp2_scene.cmake
- tests/test_scene.cpp
- demo/cp2_scene_demo.cpp
- docs/cp2/notes/scene.md, docs/images/cp2/scene_*

Build:
1. Library `scene` (links `xform`).
2. scene::Node and drawNode from the contract. drawNode multiplies the parent
   matrix by the node's pose matrix, draws the node's shape with the CP1
   scanline fill and outline, then recurses into the children. Explain in the
   comments how the recursion acts as a matrix stack (push on the way down,
   pop on the way back). Add a helper that returns the world matrix of a named
   node, so other code can find where a part ended up.
3. scene::extrude from the contract, using the mapping in the README: plan
   point (x, y) at height h becomes (X = x, Y = h, Z = y). A footprint with n
   vertices gives 2n vertices and 3n edges (bottom ring, top ring, uprights).
   Also add a helper that builds the room: floor outline plus walls of a given
   height.
4. A 2D oblique (cabinet) projection helper so my demo can draw wireframes
   without Part 3: screen x = X + 0.5 * Z * cos(45 deg), screen y = -Y + 0.5 *
   Z * sin(45 deg), plus an offset. Explain that this is a shear.
5. tests/test_scene.cpp with real assertions, at least:
   - a child's world position equals parent matrix times child pose, checked
     against hand-computed points for a two-level and a three-level model;
   - rotating the parent moves the child on a circle and keeps its distance;
   - changing a child does not move its parent or its siblings;
   - extrude gives the expected vertex and edge counts, the bottom ring at
     Y = 0, the top ring at Y = height, and the X/Z values of the footprint;
   - the node lookup helper returns the right matrix and reports a missing name.
6. demo/cp2_scene_demo.cpp writing GIFs to demo_output/cp2/ with
   output::AnimationWriter:
   - scene_fan.gif: a ceiling fan (hub with four blade children) spinning
     while the whole fan sways, showing that the blades follow the hub;
   - scene_chair.gif: a swivel chair (base, seat, backrest, armrests) that
     rolls across the room while the seat turns on the base;
   - scene_door.gif: a door as a child of the room, swinging on its hinge
     while the room itself is moved, showing that everything follows the root;
   - scene_room_3d.gif: the furnished room extruded to wireframe boxes with
     sensible heights per piece, drawn with the oblique projection, with the
     pieces rising from flat to full height.
   Register the demo as a CTest test.
7. Presenter notes in docs/cp2/notes/scene.md, covering: hierarchical
   transforms and the matrix stack, the scene graph (room, furniture, parts),
   extrusion and the 2D-to-3D coordinate mapping, and oblique projection.

Done when:
- a clean configure and build has no warnings;
- ctest passes every test, old and new;
- the GIFs play correctly and the best ones are copied to docs/images/cp2/;
- the notes are written;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by giving me a short walkthrough of what you built, in the order I
should present it.
```

---

## Prompt 3 — Viewing and projection (152502)

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. Checkpoint 1 is finished. I own Part 3 of
Checkpoint 2: the viewing transforms. That is the 2D camera (window to
viewport, pan and zoom) and the 3D camera (view matrix, projection, orbit).
I will present this work on camera, so explain your decisions to me as you go.

First read docs/cp2/README.md. Follow its "Common rules" section exactly
(pure C++, files I may touch, build hook, tests, demo output, presenter notes,
and "Git: commit as yourself"), and implement the Part 3 interfaces in its
"Shared contract" section exactly as written.

This part depends on Part 1 (src/xform/) and Part 5 (src/output/). Check that
both exist on origin/dev. If either is missing, stop and tell me; do not write
your own copy. Read their headers, plus src/raster/line.h and
src/raster/clipping.h (the CP1 clipping algorithms you will reuse) and
demo/roomcraft_demo.cpp for the furniture shapes. Then build the project and
run ctest to confirm everything passes before you change anything.

Branch: cp2/view. It already exists on origin; check it out and merge the
latest origin/dev into it before you start.

Files I own:
- src/view/camera2d.h, camera2d.cpp, mat4.h, mat4.cpp, camera3d.h, camera3d.cpp
- cmake/cp2_view.cmake
- tests/test_view.cpp
- demo/cp2_view_demo.cpp
- docs/cp2/notes/view.md, docs/images/cp2/view_*

Build:
1. Library `view` (links `xform`).
2. Camera2D and windowToViewport from the contract, built as the composite
   translate(-window corner), scale(viewport size / window size),
   translate(viewport corner), with that derivation in the comments. Add
   helpers to pan, to zoom about a point (that point must stay still on
   screen), and to fix the window's aspect ratio to the viewport's. Add a
   function that draws a furniture list through a camera: transform each world
   footprint with the camera matrix, clip polygons to the viewport with CP1
   Sutherland-Hodgman and lines with CP1 Cohen-Sutherland, then fill.
3. Mat4 from the contract: column vectors, same style as Mat3. lookAt builds
   the camera basis from eye, target and up. perspective and orthographic map
   the view volume to the cube -1..1. Write the derivations in the comments.
4. OrbitCamera from the contract: eyePosition from spherical coordinates
   (radius, azimuth, elevation) around the target, with Y up. drawWireMesh
   transforms each edge to clip space, clips it against the near plane BEFORE
   the perspective divide (explain why), divides by w, maps to canvas pixels
   with y flipped, clips to the canvas, and draws with CP1 Bresenham.
5. tests/test_view.cpp with real assertions, at least:
   - windowToViewport maps the window corners and centre to the viewport
     corners and centre; zoom keeps the anchor point fixed; pan moves by the
     expected amount; round trip through the inverse matrix returns the
     original point (this is how a mouse click is mapped back to the room);
   - Mat4 basics on hand-computed points; rotationY by 90 degrees; products;
   - lookAt maps the eye to the origin and the target onto the negative z axis;
   - perspective maps points on the near and far planes to depth -1 and +1,
     and a point twice as far away lands half as far from the screen centre;
   - eyePosition for known angles (elevation 90 is straight above the target),
     and the eye is always `radius` from the target;
   - an edge crossing the near plane is clipped, and an edge fully behind the
     camera draws no pixels.
6. demo/cp2_view_demo.cpp writing GIFs to demo_output/cp2/ with
   output::AnimationWriter. Build box meshes by hand for the room and a few
   pieces (8 vertices and 12 edges each, using X = plan x, Y = height,
   Z = plan y):
   - view_zoom.gif: the floor plan with the camera panning and zooming into
     one piece, furniture clipped cleanly at the viewport edge;
   - view_minimap.gif: two viewports on one canvas, a full-room minimap with
     a rectangle showing the main view's window as it moves;
   - view_orbit.gif: the wireframe room with the orbit camera going once round;
   - view_projection.gif: orthographic and perspective side by side during
     the same orbit;
   - view_reveal.gif: the camera starting straight above the room (it looks
     like the 2D plan) and tilting down into a 3D view.
   Register the demo as a CTest test.
7. Presenter notes in docs/cp2/notes/view.md, covering: window-to-viewport
   mapping, pan and zoom, clipping to the viewport, 3D transforms, the view
   matrix, orthographic versus perspective projection, the perspective divide,
   and orbit with spherical coordinates.

Done when:
- a clean configure and build has no warnings;
- ctest passes every test, old and new;
- the GIFs play correctly and the best ones are copied to docs/images/cp2/;
- the notes are written;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by giving me a short walkthrough of what you built, in the order I
should present it.
```

---

## Prompt 4a — Animation engine (166913)

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. Checkpoint 1 is finished. I own Part 4 of
Checkpoint 2: the animation engine. That is easing, interpolation, keyframes
and motion along a path. I will present this work on camera, so explain your
decisions to me as you go.

First read docs/cp2/README.md. Follow its "Common rules" section exactly
(pure C++, files I may touch, build hook, tests, demo output, presenter notes,
and "Git: commit as yourself"), and implement the Part 4 interfaces in its
"Shared contract" section exactly as written.

This part depends on Part 1 (src/xform/) and Part 5 (src/output/). Check that
both exist on origin/dev. If either is missing, stop and tell me; do not write
your own copy. Read their headers, plus src/raster/curves.h (the CP1
Catmull-Rom spline you will reuse), src/roomcraft/furniture.h and
demo/roomcraft_demo.cpp for the furniture shapes. Then build the project and
run ctest to confirm everything passes before you change anything.

Branch: cp2/anim. It already exists on origin; check it out and merge the
latest origin/dev into it before you start.

Files I own:
- src/anim/easing.h, easing.cpp, track.h, track.cpp, path.h, path.cpp
- cmake/cp2_anim.cmake
- tests/test_anim.cpp
- demo/cp2_anim_demo.cpp
- docs/cp2/notes/anim.md, docs/images/cp2/anim_*

Build:
1. Library `anim` (links `xform`).
2. Easing and interpolation from the contract. ease clamps t to [0, 1].
   lerpAngle must take the shorter way round (350 to 10 degrees goes through
   0, not back through 180). lerpPose interpolates position, angle (with
   lerpAngle) and scale. Give the formula of each easing curve in the comments.
3. PoseTrack from the contract: keys are kept sorted by time; sample finds the
   two keys around the time, turns the time into a 0..1 fraction, applies the
   easing of the later key, and interpolates. Before the first key and after
   the last it returns that key's pose.
4. poseOnPath from the contract: position from the CP1 Catmull-Rom spline,
   heading from the direction of travel (a small step ahead on the curve), so
   an object turns to face where it is going. Add a version that moves at
   constant speed by measuring the path length in small steps (arc-length
   table), and explain why plain u gives uneven speed.
5. A small fixed-timestep helper: given frames per second and a duration, it
   gives the frame count and the time of each frame, so every demo advances
   time the same way.
6. tests/test_anim.cpp with real assertions, at least:
   - every easing returns 0 at 0 and 1 at 1, never decreases, and EaseInOut
     is 0.5 at 0.5; EaseIn starts slower than Linear, EaseOut ends slower;
   - lerp end points and midpoint; lerpAngle across the 360/0 boundary in
     both directions;
   - PoseTrack returns each key exactly at its time, clamps outside the range,
     works with keys added out of order, and gives the midpoint pose halfway
     between two Linear keys;
   - poseOnPath passes through the first and last points, faces right on a
     path going right and down on a path going down (y is down), and the
     constant-speed version covers equal distance in equal time within a
     small tolerance.
7. demo/cp2_anim_demo.cpp writing GIFs to demo_output/cp2/ with
   output::AnimationWriter, drawn on the CP1 room:
   - anim_easing.gif: four identical armchairs sliding across the room, one
     per easing type, labelled with output::drawText;
   - anim_arrange.gif: an empty room where each piece slides in, turns and
     scales into place from keyframes, one after another;
   - anim_collision.gif: a piece pushed through a wall, turning red on every
     frame where roomcraft::fitsInRoom fails and back when it returns;
   - anim_vacuum.gif: a round robot vacuum following a Catmull-Rom path
     between the furniture at constant speed, facing its direction of travel,
     leaving a dashed trail.
   Register the demo as a CTest test.
8. Presenter notes in docs/cp2/notes/anim.md, covering: frame-by-frame
   animation and the fixed timestep, linear interpolation, easing, angle
   interpolation, keyframing, and path animation with constant speed.

Done when:
- a clean configure and build has no warnings;
- ctest passes every test, old and new;
- the GIFs play correctly and the best ones are copied to docs/images/cp2/;
- the notes are written;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by giving me a short walkthrough of what you built, in the order I
should present it.
```

---

## Prompt 4b — Final showcase (166913, wave 3)

Run this after parts 1, 2, 3, 4a and 5a are all merged into `dev`.

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. All five Checkpoint 2 libraries are merged into
dev: xform (matrices, pose), output (GIF, text), scene (hierarchy, extrude),
view (cameras, projection) and anim (easing, keyframes, paths). I own the
integration: one showcase animation that uses all of them together. I will
present this work on camera, so explain your decisions to me as you go.

First read docs/cp2/README.md and follow its "Common rules" section exactly,
including "Git: commit as yourself". Read the headers of all five libraries
and each part's demo in demo/cp2_*_demo.cpp. Use the libraries as they are.
If something you need is missing or broken in another part, stop and tell me
so I can ask its owner; do not edit their files. Then build the project and
run ctest to confirm everything passes before you change anything.

Branch: cp2/showcase. It already exists on origin; check it out and merge the
latest origin/dev into it before you start.

Files I own:
- demo/cp2_showcase.cpp
- cmake/cp2_showcase.cmake
- docs/cp2/notes/showcase.md, docs/images/cp2/showcase_*

Build demo/cp2_showcase.cpp, which writes demo_output/cp2/showcase.gif: one
continuous animation of about 20 to 30 seconds, with a caption drawn by
output::drawText naming the technique on screen in each scene:
1. The empty room appears; the door swings open on its hinge (pivot rotation,
   hierarchy).
2. Furniture arranges itself from keyframes with easing (translate, rotate,
   scale; PoseTrack).
3. One piece is pushed through a wall, turns red, and is mirrored to fit the
   opposite corner (clipping check, reflection).
4. The ceiling fan spins and the robot vacuum follows its path (hierarchy,
   path animation).
5. The 2D camera zooms into one piece and back out (window to viewport).
6. The 2D-to-3D reveal: the furniture is extruded with scene::extrude and
   drawn with view::drawWireMesh while the orbit camera tilts from straight
   above down to a 3D view and goes once round the room (extrusion,
   perspective projection, orbit).
Also write each scene as its own shorter GIF (showcase_1_door.gif and so on)
for use in the README and the video. Keep every committed file under 2 MB;
reduce the canvas size or frame rate if needed. Register the showcase as a
CTest test.

Write docs/cp2/notes/showcase.md: for each scene, which libraries and
functions it uses, so it works as a map of how the components relate to one
another, plus a Challenges section with the real integration problems.

Done when:
- a clean configure and build has no warnings;
- ctest passes every test;
- showcase.gif plays correctly from start to finish and the GIFs are copied
  to docs/images/cp2/;
- the notes are written;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by giving me a short walkthrough of the showcase, scene by scene.
```

---

## Prompt 5b — Documentation and delivery (161309, wave 3)

Run this after the showcase (prompt 4b) is merged into `dev`.

```text
You are working in the RoomCraft repository, a pure C++17 software rasterizer
for a room-layout visualiser. Checkpoint 2 (transformation and animation) is
implemented and merged into dev. I own the documentation and the submission
material. I will present this work on camera, so explain your decisions to me
as you go.

First read docs/cp2/README.md and follow its "Common rules" section exactly,
including "Git: commit as yourself". Read the current README.md, every file in
docs/cp2/notes/, and the list of files in docs/images/cp2/. Build the project,
run ctest and run every cp2 demo, and use the real output: do not write down
a check count, file name or result you have not seen yourself.

Branch: cp2/docs. It already exists on origin; check it out and merge the
latest origin/dev into it before you start.

Files I own:
- README.md
- docs/cp2/video_script.md

Do:
1. Update README.md for Checkpoint 2, keeping the Checkpoint 1 content:
   - a showcase GIF near the top;
   - a "Checkpoint 2: transformations and animation" techniques table in the
     same format as the CP1 table (technique, algorithm, source file,
     RoomCraft use), filled in from the code and the notes;
   - the new demo executables, what each writes, and how to view GIFs;
   - the new tests with their real check counts;
   - "Current progress" updated, with CP3 (OpenGL integration) as the next step;
   - "Known limitations" updated: remove those CP2 fixed, add the new ones;
   - the build instructions checked on my machine, saying which platform and
     compiler were actually tested.
2. Write docs/cp2/video_script.md following the five sections of the
   submission guidelines:
   - Proposed vs actual: use the comparison in the team's plan (web app with
     Three.js became pure C++; interactive editor became scripted animation
     until OpenGL at CP3; lighting and colour pickers deferred; curved
     furniture added);
   - Group responsibilities: one block per member taken from their notes file
     (responsibilities, techniques, what they built, challenges);
   - Project components: each component, its purpose, and how they relate;
   - Techniques and algorithms: for each one, the five questions, and which
     GIF or image to show on screen;
   - Recommendations: plans for CP3 and the final, and lessons learnt.
   Give each block a speaker and a rough time, and list what must be on the
   shared screen at that moment. Leave clearly marked blanks for anything
   only the team can supply, such as lessons learnt; do not make them up.
3. Check that every image and link in README.md resolves to a file in the
   repository.

Done when:
- README.md describes what is actually in the repository;
- the video script covers all five guideline sections and every member;
- the work is committed in small steps as me, pushed, and a pull request
  into dev is open.
Finish by listing anything in the README or script that I still need to fill
in or confirm with the team.
```

---

## Merging

- One member reviews and merges each pull request into `dev`. The reviewer
  builds the branch and runs `ctest` first.
- Merge order: parts 1 and 5a, then 2, 3 and 4a in any order, then 4b, then 5b.
- When `dev` is complete and tested, open one pull request from `dev` into
  `main` for the submission.
