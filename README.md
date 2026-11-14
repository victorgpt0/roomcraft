# RoomCraft

**RoomCraft** is a 2D-to-3D interior layout visualiser. Users lay out a room and
its furniture on a top-down 2D grid and will later view the result in 3D.
Checkpoint 2 adds matrices, cameras, a scene graph, animations and a from-scratch
GIF encoder; the floor plan is still the single source of truth.

![RoomCraft Checkpoint 2 integration showcase](docs/images/cp2/showcase.gif)

*`showcase.gif` (22 s): the door opens, furniture arranges itself, a sofa is
mirrored into the room, the fan and vacuum animate, the 2D camera zooms, and
the plan is extruded into a 3D orbit reveal.*

![Complete RoomCraft floor plan rendered by the demo](docs/images/floor_plan.png)

*The complete floor plan written by `roomcraft_demo` (1 pixel = 1 cm, room 5.6 m × 4.0 m).*

---

## Group Members

| Name | GitHub |
|---|---|
| Victor Gichuru | [@victorgpt0](https://github.com/victorgpt0) |
| Tobiko Leriari | [@Leriari](https://github.com/Leriari) |
| Kristein Mwaura | [@Houstonsboy](https://github.com/Houstonsboy) |
| Emmanuel Oringe | [@eoringe](https://github.com/eoringe) |
| Joy Kipkemboi | [@Joychepchumba](https://github.com/Joychepchumba) |

---

## Technologies and tools

| | |
|---|---|
| Language | C++17 (standard library only) |
| Build system | CMake ≥ 3.20 |
| Tests | CTest, with a small assertion helper ([tests/test_helpers.h](tests/test_helpers.h)) |
| Output | Binary PPM (P6), 24-bit BMP, GIF89a animations and bitmap text, written by our own code |
| Tested with | GCC 13.3.0 + CMake 3.28.3 on Pop!_OS 24.04 (Linux); also GCC 15.2 (MSYS2 UCRT64) + Ninja on Windows 11 in earlier CP2 work |
| Version control | Git / GitHub |

---

## Graphics techniques implemented

All coordinates are **screen coordinates**: the origin is the top-left corner,
x grows to the right and **y grows downward**. Integer coordinates are pixel
centres. Because y points down, positive angles turn **clockwise** on screen.
See [src/raster/types.h](src/raster/types.h).

| Technique | Algorithm | Source file | RoomCraft use |
|---|---|---|---|
| Line drawing | DDA | [line.cpp](src/raster/line.cpp) | Comparison with Bresenham |
| Line drawing | Bresenham / midpoint, integer-only, all 8 octants | [line.cpp](src/raster/line.cpp) | Every outline, grid line and wall |
| Line operations | Thick lines (perpendicular width), dashed lines, polylines | [line.cpp](src/raster/line.cpp) | Walls, dimension lines, furniture outlines |
| Line clipping | Cohen-Sutherland (region codes) | [line.cpp](src/raster/line.cpp) | Editor grid clipped to the room |
| Polygon representation | Ordered vertex list | [polygon.h](src/raster/polygon.h) | Furniture footprints, room outline |
| Polygon operations | Translate, rotate and scale about a pivot | [polygon.cpp](src/raster/polygon.cpp) | Moving, turning and resizing furniture |
| Polygon measurements | Shoelace area, area centroid, bounding box | [polygon.cpp](src/raster/polygon.cpp) | Floor-space statistics, selection boxes |
| Point in polygon | Even-odd ray casting | [polygon.cpp](src/raster/polygon.cpp) | Which piece of furniture was clicked |
| Polygon clipping | Sutherland-Hodgman | [clipping.cpp](src/raster/clipping.cpp) | Does the furniture fit inside the room? |
| Polygon filling | Scanline fill with an edge table and active edge table (even-odd, half-open rule) | [fill.cpp](src/raster/fill.cpp) | Solid furniture and floor; tiles without gaps or overlaps |
| Region filling | Boundary fill, 4-connected, explicit stack | [fill.cpp](src/raster/fill.cpp) | "Paint bucket" for floor zones |
| Circles | Midpoint circle with 8-way symmetry; filled disc | [circle.cpp](src/raster/circle.cpp) | Round tables, chairs, plant pots |
| Arcs | Midpoint circle limited to an angle range | [circle.cpp](src/raster/circle.cpp) | Door swings |
| Ellipses | Two-region midpoint ellipse with 4-way symmetry; filled ellipse | [ellipse.cpp](src/raster/ellipse.cpp) | Rugs, pillows |
| Bézier curves | Cubic Bézier: Bernstein form **and** de Casteljau | [curves.cpp](src/raster/curves.cpp) | Curved sofa, kidney-shaped coffee table |
| Splines | Catmull-Rom (passes through every point) | [curves.cpp](src/raster/curves.cpp) | Curtain track through clicked points |
| Splines (extra) | Uniform cubic B-spline (approximating) | [curves.cpp](src/raster/curves.cpp) | Smooth rounded outlines |
| Editor logic | `hitTest`, `findFurnitureAt`, `fitsInRoom` | [furniture.cpp](src/roomcraft/furniture.cpp) | Selecting furniture, flagging furniture that sticks through a wall |

Every algorithm is commented in its source file: what it does, why it works,
and how it fits the coordinate convention.

### More Notes

- **Bresenham** uses only integer additions and comparisons. One decision
  variable, derived in the comments, covers all eight octants using step
  directions `sx` and `sy`.
- **The scanline fill** recomputes each edge's x from the line equation on
  every row, instead of adding 1/slope each time as in the textbook version.
  Repeated float additions drift, and where an edge passes exactly through a
  pixel centre the drift can move a pixel by one. That would break the
  guarantee that neighbouring polygons neither overlap nor leave gaps, which
  the tests check.
- **The midpoint ellipse** traces region 1 from the top tip and region 2 from
  the side tip, splitting them at the slope = −1 point. The usual single-pass
  textbook loop put pixels up to a whole pixel off the curve on small or flat
  ellipses: 21-128 of the 3,600 sizes from 1×1 to 60×60, depending on the
  region-switch rule. The two-tip version keeps every pixel within half a
  pixel, with no gaps, for all 22,500 sizes from 1×1 to 150×150. Both
  versions use the same midpoint test, so the idea is unchanged.

---

## Checkpoint 2: transformations and animation

Checkpoint 2 turns the fixed 1 pixel = 1 cm floor plan into a scene that can be
placed, animated, viewed through cameras and exported as a GIF. The new
libraries are `xform` (matrices and poses), `output` (GIF and text), `scene`
(hierarchy and extrusion), `view` (2D/3D cameras and projection) and `anim`
(timing, interpolation, keyframes and paths).

| Technique | Algorithm | Source file | RoomCraft use |
|---|---|---|---|
| 2D homogeneous transforms | 3×3 matrix product, inverse by adjugate, pivot composites | [mat3.cpp](src/xform/mat3.cpp) | Move, rotate, scale, mirror and undo furniture transforms |
| Pose / model matrix | `T(position) · R(angle) · S(scale)`, then `worldFootprint` | [pose.cpp](src/xform/pose.cpp), [furniture.cpp](src/roomcraft/furniture.cpp) | One local footprint placed in the room by a pose |
| Hierarchical transforms | Parent-child matrix stack; recursive draw and named world-matrix lookup | [node.cpp](src/scene/node.cpp) | Fan hub/blades, swivel chair parts, room and door |
| 3D extrusion | Duplicate a footprint at floor and height, then join bottom ring, top ring and uprights | [extrude.cpp](src/scene/extrude.cpp) | Wireframe furniture and room walls |
| Oblique projection | Cabinet/cavalier shear: `x += 0.5z·cos45°`, `y += 0.5z·sin45°` | [extrude.cpp](src/scene/extrude.cpp) | Temporary 3D preview before the CP2 cameras |
| GIF89a container | Header, logical screen descriptor, global palette, loop extension, frame blocks, trailer | [gif.cpp](src/output/gif.cpp) | Looping RoomCraft animations |
| LZW compression | Variable 9-12-bit dictionary, CLEAR/END codes and least-significant-bit packing | [gif.cpp](src/output/gif.cpp) | Small GIF files, e.g. 60 frames in 625 KB |
| Palette quantisation | Exact flat colours; 8×8×4 fixed fallback; nearest-RGB mapping when a palette freezes | [animation_writer.cpp](src/output/animation_writer.cpp) | Exact colours normally, sensible gradients when over 256 colours |
| Bitmap text | 5×7 glyph table, 6-cell advance and integer scaling | [text.cpp](src/output/text.cpp) | Captions, labels and frame counters |
| 2D camera | Window-to-viewport affine map, pan, zoom about an anchor, inverse mapping | [camera2d.cpp](src/view/camera2d.cpp) | Zoomed editor view, minimap and screen-to-room clicks |
| 2D viewport clipping | Sutherland-Hodgman for polygons, Cohen-Sutherland for lines, applied after the camera transform | [camera2d.cpp](src/view/camera2d.cpp) | Clean cuts at the viewport edge |
| 3D transforms | 4×4 homogeneous translation, rotation, scaling, `lookAt`, orthographic and perspective matrices | [mat4.cpp](src/view/mat4.cpp) | Camera and projection pipeline |
| Orbit camera | Spherical radius/azimuth/elevation plus a tangent up vector that survives elevation 90° | [camera3d.cpp](src/view/camera3d.cpp) | Orbit animations and the top-down plan view |
| Perspective divide and near clipping | Clip edges against the near plane in homogeneous clip space, divide by `w`, map NDC to the viewport | [camera3d.cpp](src/view/camera3d.cpp) | Correct wireframes even when geometry is behind the camera |
| Fixed timestep | `Δt = 1/fps`, frame count, frame time and normalised progress | [track.cpp](src/anim/track.cpp) | Deterministic, reproducible animations |
| Interpolation | Scalar `lerp`, shortest-arc `lerpAngle`, and `lerpPose` in pose parameter space | [easing.cpp](src/anim/easing.cpp) | Furniture moves and turns without shrinking |
| Easing curves | Quadratic ease-in/out and cubic smoothstep ease-in-out | [easing.cpp](src/anim/easing.cpp) | Smooth acceleration and deceleration |
| Keyframe tracks | Sorted keys, interval lookup, local time normalisation and per-key easing | [track.cpp](src/anim/track.cpp) | Furniture arrangement and collision scenes |
| Catmull-Rom path motion | Spline evaluation with an arc-length table for constant speed and tangent heading | [path.cpp](src/anim/path.cpp) | Robot vacuum and other path animations |
| Integration timeline | Fixed-step scene dispatch, frame assembly, captions and progress bar | [cp2_showcase.cpp](demo/cp2_showcase.cpp) | The 22-second six-scene showcase |

The dependency direction is one-way: `xform` supplies poses and matrices to
`scene` and `view`; `scene` supplies meshes to `view`; `anim` supplies poses to
`roomcraft` furniture; and `output` writes whatever the rasterizer drew. The
showcase is the integration point that uses all five libraries in one
timeline.

---

## How to compile and run

The build produces the libraries `raster`, `roomcraft`, `xform`, `output`,
`scene`, `view` and `anim`; 14 unit-test executables; `roomcraft_demo`; and
the six Checkpoint 2 demo executables.

### Linux - Pop!_OS 24.04

The commands below were run on Pop!_OS 24.04 with GCC 13.3.0, CMake 3.28.3
and Unix Makefiles. A clean configure and build produced no warnings with
`-Wall -Wextra -Wpedantic`.

```sh
cmake -S . -B build
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/roomcraft_demo
./build/cp2_transform_demo
./build/cp2_output_demo
./build/cp2_scene_demo
./build/cp2_view_demo
./build/cp2_anim_demo
./build/cp2_showcase demo_output/cp2
```

### Windows - MSYS2 / MinGW 

Install [MSYS2](https://www.msys2.org/), then in an MSYS2 UCRT64 shell install
the tools:

```sh
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

Then, in PowerShell from the project folder:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"   # once per terminal
cmake -S . -B build -G Ninja                    # configure (once)
cmake --build build                             # build
ctest --test-dir build --output-on-failure      # run all tests
.\build\roomcraft_demo.exe                      # write the CP1 images
.\build\cp2_showcase.exe demo_output\cp2        # write the CP2 GIFs
```

The team previously built and ran this setup with GCC 15.2 on Windows 11.

### Windows - Visual Studio 2022

The **"Desktop development with C++"** workload must be installed (Visual
Studio Installer → Modify). This path was not re-tested for Checkpoint 2.

- **From the IDE:** *File → Open → Folder…* and pick the project folder.
  Visual Studio detects `CMakeLists.txt`. Build with *Build → Build All*, run
  the tests from the *Test Explorer*, and choose `roomcraft_demo.exe` as the
  startup item to run the CP1 demo.
- **From a "Developer PowerShell for VS 2022":**

  ```powershell
  cmake -S . -B build-vs -G "Visual Studio 17 2022"
  cmake --build build-vs --config Release
  ctest --test-dir build-vs -C Release --output-on-failure
  .\build-vs\Release\cp2_showcase.exe demo_output\cp2
  ```

### macOS

The Linux commands should also work with Clang and CMake ≥ 3.20, but macOS was
not tested for this project.

### Running a single test

```sh
ctest --test-dir build -R test_line   # or run ./build/test_line directly
```

Each test prints a summary such as `test_line: 71144 checks, 0 failed` and
exits with a non-zero code if any check fails.

---

## How to view the output images and animations

All demos write under `demo_output/`; the Checkpoint 2 demos write under
`demo_output/cp2/`. That folder is git-ignored, so run the demo to produce the
files. The best results are copied to `docs/images/cp2/` for this README.

| Program | Writes to | Files |
|---|---|---|
| `roomcraft_demo` | `demo_output/` (or a folder given as the first argument) | `01_lines` … `07_selection`, each as `.bmp` and `.ppm` |
| `cp2_transform_demo` | `demo_output/cp2/` | `transform_basic`, `transform_order`, `transform_pivot`, `transform_mirror`, `transform_pose` (`.bmp`) |
| `cp2_output_demo` | `demo_output/cp2/` | `output_first_animation.gif`, `output_font.bmp`, `output_palette.gif` |
| `cp2_scene_demo` | `demo_output/cp2/` | `scene_fan.gif`, `scene_chair.gif`, `scene_door.gif`, `scene_room_3d.gif` |
| `cp2_view_demo` | `demo_output/cp2/` | `view_zoom.gif`, `view_minimap.gif`, `view_orbit.gif`, `view_projection.gif`, `view_reveal.gif` |
| `cp2_anim_demo` | `demo_output/cp2/` | `anim_easing.gif`, `anim_arrange.gif`, `anim_collision.gif`, `anim_vacuum.gif` |
| `cp2_showcase` | `demo_output/cp2/` (or a folder given as the first argument) | `showcase.gif` plus `showcase_1_door.gif` … `showcase_6_reveal.gif` |
| `test_*` | `tests/output/` | one image per test, `.bmp` and `.ppm` |

- **`.bmp`** opens directly in Windows Photos, macOS Preview and most image
  viewers.
- **`.ppm`** is the simplest image format (a short text header followed by raw
  RGB bytes). Windows cannot open it without extra software; use GIMP or
  IrfanView, or an image viewer on Linux.
- **`.gif`** animations open and loop in a browser, Windows Photos, macOS
  Preview and most image viewers. They are written by the program's own GIF89a
  encoder, not by an external tool.

Each CP2 demo prints what it wrote and where. `roomcraft_demo` also prints the
area of each piece of furniture, a fit check and the result of each simulated
click to the terminal.

| Checkpoint 2 demo | What it shows |
|---|---|
| `cp2_transform_demo` | Basic matrices, order of composition, pivot rotation, reflection/shear and one pose placed six ways |
| `cp2_output_demo` | A 60-frame animation, the full 5×7 font sheet, and the fixed-palette fallback for a 61,727-colour gradient |
| `cp2_scene_demo` | Hierarchical fan/chair/door motion, extrusion and the oblique 3D preview |
| `cp2_view_demo` | Pan/zoom, minimap, orbit, orthographic-versus-perspective comparison and the 2D-to-3D reveal |
| `cp2_anim_demo` | Easing comparison, keyframed arrangement, wall-collision feedback and the constant-speed vacuum path |
| `cp2_showcase` | The complete six-scene integration story and each scene as a shorter clip |

| Demo image | Shows |
|---|---|
| `01_lines` | Thick walls with door and window gaps, grid clipped to the room, dashed dimension line, DDA vs Bresenham, a polyline |
| `02_polygons` | Armchair translated, rotated and scaled about its centroid, with bounding boxes; L-shaped sofa whose centroid lies outside it |
| `03_clipping_filling` | Furniture pushed through walls and clipped to the room, floor tiles with no gaps, boundary-filled play mat |
| `04_circles_ellipses` | Round dining table and chairs, oval rug, pillows, plant pots, door swings |
| `05_curves` | Bézier sofa with its control polygon, kidney coffee table, Catmull-Rom vs B-spline curtain track |
| `06_floor_plan` | The complete room |
| `07_selection` | Simulated clicks (orange = selected piece) and a bookshelf sticking through the wall (red) |

![Selection and fit check: clicked pieces in orange, a bookshelf through the wall in red](docs/images/selection.png)

*`07_selection`: clicks (✕) select the topmost piece under the cursor;
`fitsInRoom` flags the bookshelf pushed through the right wall.*

![Curves: Bézier sofa and table, Catmull-Rom and B-spline through clicked points](docs/images/curves.png)

*`05_curves`: a Bézier sofa with its dashed control polygon, a kidney table made
of three Bézier curves, a Catmull-Rom spline (purple) passing through every
clicked point ✕, and a B-spline (green) that smooths past them.*

The Checkpoint 1 previews are PNG copies of the demo's BMP files, converted
with Windows' built-in image tools. The CP2 transform and font previews are
PNG copies of the BMP output, converted with ImageMagick as recorded in the
notes; every CP2 GIF in `docs/images/cp2/` is written by the program itself and
committed unmodified. The program writes BMP, PPM and GIF.

---

## Testing

Every test checks real properties, not just that the code runs. CTest runs 21
tests: 14 unit-test executables, `roomcraft_demo`, and the six CP2 demos.
All 21 passed on the tested Linux machine.

### Checkpoint 1 tests

| Test | Checks | What it verifies |
|---|---|---|
| `test_canvas` | 31 | Sizes ≤ 0 rejected; out-of-bounds pixels; PPM and BMP bytes read back |
| `test_line` | 71,144 | Bresenham and DDA in all 961 directions with \|dx\|, \|dy\| ≤ 15: both endpoints, no gaps, one pixel per step, within ½ pixel of the ideal line; 16 Cohen-Sutherland cases; thick, dashed and polyline helpers |
| `test_polygon` | 55 | Exact area 11250 and centroid (125, 100) of a known triangle; transformations; bounding box; point-in-polygon on concave shapes and vertex tie cases |
| `test_clipping_fill` | 58 | Clipped square has the expected 4 vertices; clipped triangle has 5; filled rectangle has exactly w×h pixels; hexagon split into 6 triangles has 0 overlaps and 0 gaps; boundary fill handles a 1000×1000 area and leaks through a gap as expected |
| `test_circle_ellipse` | 2,415 | Every circle radius 1-60 and ellipse 1×1-16×16: within ½ pixel, symmetric, no gaps; filled pixel count ≈ πr²; fill = outline + inside; arc ranges |
| `test_curves` | 76 | Bézier end points exact; Bernstein = de Casteljau at 1,001 values of t; Catmull-Rom passes through every point with matching tangents at joints; B-spline properties |
| `test_furniture` | 27 | Hit tests (including the inner corner of a concave sofa), topmost piece wins, `fitsInRoom` cases |
| `roomcraft_demo` | - | The CP1 demo runs and writes all 7 image pairs |

### Checkpoint 2 tests

| Test | Checks | What it verifies |
|---|---|---|
| `test_mat3` | 4,683 | Homogeneous transforms against hand-computed points and the CP1 polygon operations; order and associativity; pivots; reflection determinant; inverse and singular matrices |
| `test_pose` | 162 | Scale-rotate-move order; `worldFootprint`; hit testing and `fitsInRoom` on moved, turned, scaled and mirrored pieces; every drawn furniture pixel passes its hit test |
| `test_gif` | 877 | GIF structure and delays; LZW round trips including code-size growth and table resets; palette fallback and freezing; per-pixel decode comparison |
| `test_text` | 378 | Glyph cells for `A`, `7`, `g`; advance widths; all 95 glyphs distinct and in-cell; scaling; text clipped at every edge |
| `test_scene` | 259 | Two- and three-level world matrices; parent/sibling isolation; hierarchy drawing; look-up by name; extrusion counts and rings; room helper; oblique projection |
| `test_view` | 1,281 | Window-to-viewport corners and centre; pan/zoom anchor; inverse screen-to-room; viewport clipping; `Mat4` rotations; `lookAt`; orbit at elevation 90; orthographic/perspective; near-plane clipping and pixel-exact wireframes |
| `test_anim` | 2,012 | Fixed timestep; lerp endpoints and angle wrapping; easing properties; `PoseTrack` ordering/clamping; path control points, heading and equal arc length per step |
| `cp2_transform_demo`, `cp2_output_demo`, `cp2_scene_demo`, `cp2_view_demo`, `cp2_anim_demo`, `cp2_showcase` | - | Each demo runs and writes its expected images/GIFs |

The seven CP2 unit tests contain 9,652 checks. These counts are the summaries
printed by the current executables (`test_*: N checks, 0 failed`).

---

## Current progress

**Checkpoint 1 - Technical Foundation: complete.**

- [x] Line drawing and line operations
- [x] Polygon representation and operations
- [x] Polygon clipping and filling
- [x] Circle and ellipse representation
- [x] Curves: Bézier curves and splines
- [x] Techniques applied to RoomCraft objects (demo floor plan)
- [x] Tests with real assertions, run by CTest
- [x] GitHub repository with source, README and commit history

**Checkpoint 2 - Transformations and Animation: complete.**

- [x] 3×3 homogeneous transforms, matrix inverse and furniture poses
- [x] GIF89a encoder with LZW, animation writer and 5×7 bitmap text
- [x] Hierarchical scene graph, extrusion and oblique 3D preview
- [x] 2D window-to-viewport camera, pan, zoom and screen-to-room mapping
- [x] 3D `Mat4`, `lookAt`, orbit camera, orthographic/perspective projection
- [x] Fixed timestep, interpolation, easing, keyframe tracks and constant-speed paths
- [x] Six-scene integration showcase, per-scene GIFs and presenter notes
- [x] CP2 tests and README

**Next: Checkpoint 3 - OpenGL integration.** The plan is to replace the
software preview path with real-time OpenGL rendering, then add the interactive
editor that CP2 deliberately left out. Lighting and colour pickers are
deferred until that editor exists.

---

## Known limitations

**Geometry and editing**

- **Sutherland-Hodgman needs a convex clip window.** `fitsInRoom` therefore
  only works for convex rooms, such as rectangles. L-shaped rooms are not
  supported yet.
- **Concave furniture clipped into separate pieces** comes back as one polygon
  with zero-width "bridge" edges along the wall. Its area and fill are
  correct, but its outline shows the bridges.
- **No anti-aliasing:** lines and edges are aliased ("jaggies").
- **Boundary fill needs a closed boundary.** A one-pixel gap lets the fill leak
  out, possibly over the whole canvas. It is 4-connected on purpose, so it
  does not leak through diagonal Bresenham walls.
- **Thick lines** have square-cut ends and no corner joins, so thick polylines
  show small notches at the corners.
- **Curves are drawn as polylines** with a fixed number of samples (no
  adaptive subdivision). Too few samples look angular.
- **The uniform B-spline does not reach its first and last control points.**
  That is a property of this spline type; it was not clamped.
- **Ellipses are axis-aligned only.** Rotated ellipses are not supported.
- **Round furniture** uses a many-sided polygon footprint for hit testing and
  fit checks, while it is drawn with the exact midpoint circle.
- **No interactive editor yet.** CP2 animations are scripted; selecting,
  dragging and editing in real time waits for the CP3 OpenGL layer. There is
  no lighting or colour picker yet either.

**3D viewing**

- **Wireframes only:** there is no hidden-line removal, so edges behind
  furniture remain visible.
- **Only the near plane is clipped in 3D.** The far plane and sides are
  handled by Cohen-Sutherland after projection, which is correct for lines.
- **`fillPlanPolygon` skips a whole polygon** if any vertex is behind the near
  plane; it does not clip polygons in 3D. The demos keep the camera outside
  the room, so this does not occur there.

**Animation and output**

- **At most 256 colours per GIF**, with no dithering, so gradients show bands.
  If the 257th colour appears after the first frame, the palette freezes and
  later colours are approximated by the nearest existing entry.
- **Every GIF frame stores the whole picture.** Frames are not reduced to the
  changed region, so files are larger than an optimising encoder would make.
- **GIF frame timing has 0.01 s resolution and a 0.02 s minimum.** The 12 fps
  showcase is therefore stored as 80 ms per frame and plays at 12.5 fps.
- **Text is one line of a fixed-width ASCII font.** Non-ASCII characters are
  drawn as one box per byte, and lower-case descenders are squeezed into the
  seven-row cell.
- **`PoseTrack` interpolates position linearly** between keyframes. Curved
  trajectories need `poseOnPathConstantSpeed`, and the arc-length table is
  rebuilt on each call.
- **Matrices store `float`.** Transform tests bound room-sized error to a few
  thousandths of a pixel, but very long accumulated products will drift;
  rebuilding from a pose each frame avoids this.

**Platform testing**

- Tested for this README with GCC 13.3.0, CMake 3.28.3 and Unix Makefiles on
  Pop!_OS 24.04 (Linux).
- The team previously tested GCC 15.2 (MSYS2 UCRT64) + Ninja on Windows 11.
  MSVC (`/W4`), Clang and macOS were not tested for CP2.
