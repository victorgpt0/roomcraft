# RoomCraft Checkpoint 2 — video script

This script follows the five sections in the submission guidelines. Times are
rough and assume a 14-minute video. Every block names its speaker and the
shared-screen content. Anything only the team can supply is marked
`[TEAM TO FILL: …]`; do not replace a blank with a guess.

**Shared-screen setup before recording**

- 1920 × 1080 capture, browser at 100% zoom, notifications off.
- Open the GIFs in a browser or image viewer that loops them; do not scrub
  frame by frame during the live narration.
- Keep `README.md` and the repository tree open in tabs for the component and
  technique sections.
- Use the committed GIFs under `docs/images/cp2/`, not `demo_output/`, so the
  recording matches the repository.

---

## 1. Proposed vs actual — Victor — 0:00–1:30

**On screen:** the top of `README.md`, then `docs/images/cp2/showcase.gif`
looping in a browser.

**Victor:**

“RoomCraft started as a plan for a browser layout tool: a web app with
Three.js for the 3D view. What we actually built is a pure C++17 software
rasterizer with no external graphics library. Every line, polygon, curve, GIF
byte and pixel in this video comes from our own code.

The largest change is the interactive editor. The plan assumed users would
drag and edit furniture live. Checkpoint 2 instead renders scripted
animations: the furniture moves along keyframes, the cameras follow fixed
paths, and the clicks in the CP1 selection demo are simulated. The interactive
editor is now the job of Checkpoint 3, when OpenGL integration removes the
cost of redrawing the whole software canvas.

Two promised features are deliberately deferred: real lighting and colour
pickers. They need the interactive OpenGL editor first, so they are CP3 and
final work rather than CP2 work.

One feature grew beyond the original plan: curved furniture. We have a cubic
Bézier sofa, a kidney-shaped table built from three Bézier curves, and a
Catmull–Rom curtain track that passes through every clicked point. The CP1
techniques table is still in the README, and the new CP2 table sits below it.”

**On screen:** scroll from the showcase GIF to the CP1 techniques table and
then the CP2 techniques table in `README.md`.

---

## 2. Group responsibilities — all members — 1:30–6:00

Each member speaks for about 50 seconds. The block is taken from that member's
notes file under `docs/cp2/notes/`.

### 2.1 Victor Gichuru — Parts 1, 5a and 5b — 1:30–2:20

**On screen:** `docs/images/cp2/transform_pivot.png`, then
`docs/images/cp2/output_first_animation.gif`, then this script.

- **Responsibilities:** the transform core (issue #4), the animation output
  library (issue #5) and the documentation/submission material (issue #10).
- **Techniques:** 3×3 homogeneous matrices, matrix inverse, pivot transforms,
  poses, GIF89a/LZW encoding, palette handling, the 5×7 bitmap font and the
  integration documentation.
- **What I built:** `src/xform/mat3.*`, `src/xform/pose.*`, the `Furniture::pose`
  refactor, `src/output/gif.*`, `src/output/animation_writer.*`,
  `src/output/text.*`, and the CP2 README/video script.
- **Challenges:** adding the pose member caused warning-free builds to fail at
  12 Checkpoint 1 initialiser sites; a click on the chair's pose position was a
  correct miss because the local origin sits in the open seat; the GIF palette
  must be written before all colours are known, so the encoder rewrites a
  placeholder table and freezes later additions; two test expectations (the
  letter `g` and a flat-colour frame) were wrong, not the code.

### 2.2 Kristein Mwaura — Part 2 — 2:20–3:10

**On screen:** `docs/images/cp2/scene_fan.gif`, then
`docs/images/cp2/scene_room_3d.gif`.

- **Responsibilities:** hierarchy and 3D geometry (issue #6).
- **Techniques:** parent–child matrix stacks, named world-matrix lookup,
  footprint extrusion to wireframe prisms, and the temporary oblique/cabinet
  projection used before the Part 3 cameras existed.
- **What I built:** `src/scene/node.*`, `src/scene/extrude.*`,
  `tests/test_scene.cpp` and the fan/chair/door/3D scenes in
  `demo/cp2_scene_demo.cpp`.
- **Challenges:** rotation signs are easy to flip because screen y points
  down; the room had to rotate about its centre while the door hinge stayed in
  room-local coordinates; cabinet projection makes the room much wider on the
  canvas, so the offset and canvas size came from measured projected bounds;
  the committed GIFs had to stay under 2 MB.

### 2.3 Tobiko Leriari — Part 3 — 3:10–4:00

**On screen:** `docs/images/cp2/view_zoom.gif`, then
`docs/images/cp2/view_projection.gif`, then `docs/images/cp2/view_reveal.gif`.

- **Responsibilities:** viewing and projection (issue #7).
- **Techniques:** window-to-viewport mapping, pan, zoom about an anchor,
  inverse screen-to-room mapping, Cohen–Sutherland and Sutherland–Hodgman
  clipping through a camera, `Mat4`, `lookAt`, orbit cameras, orthographic and
  perspective projection, perspective divide and near-plane clipping.
- **What I built:** `src/view/camera2d.*`, `src/view/mat4.*`,
  `src/view/camera3d.*`, `tests/test_view.cpp` and the five view animations.
- **Challenges:** the MSYS2/MinGW compiler failed silently until the UCRT64
  toolchain was first on PATH; `lookAt` breaks at elevation 90 with the usual
  up vector, solved with a tangent up vector; a reused camera object made four
  test checks fail until the test set its own angles; the first zoom GIF was
  2.04 MB and had to be shortened; the top view initially flared because the
  wall tops were much nearer than the floor, solved with a longer lens.

### 2.4 Joy Kipkemboi — Part 4a — 4:00–4:50

**On screen:** `docs/images/cp2/anim_easing.gif`, then
`docs/images/cp2/anim_arrange.gif`, then `docs/images/cp2/anim_vacuum.gif`.

- **Responsibilities:** the animation engine (issue #8).
- **Techniques:** fixed timesteps, scalar/pose/angle interpolation, easing
  curves, sorted keyframe tracks, collision feedback and constant-speed
  Catmull–Rom path motion.
- **What I built:** `src/anim/easing.*`, `src/anim/track.*`, `src/anim/path.*`,
  `tests/test_anim.cpp` and the easing/arrangement/collision/vacuum demos.
- **Challenges:** the tangent at the end of a Catmull–Rom spline collapses
  because the parameter is clamped, solved by measuring the tangent inside the
  valid range; raw-spline dashes stretch and bunch, solved with fixed
  arc-length samples; CP1's hardcoded bed pillows and rug border did not follow
  a rotating pose, solved by defining those details locally and applying
  `poseMatrix`.

### 2.5 Emmanuel Oringe — Part 4b — 4:50–5:40

**On screen:** `docs/images/cp2/showcase.gif`, then
`docs/images/cp2/showcase_3_mirror.gif`.

- **Responsibilities:** the final integration showcase (issue #9).
- **Techniques:** a fixed-step six-scene timeline, collision-driven colour
  feedback, reflection, hierarchy, keyframes, path animation, 2D zoom,
  extrusion and the orbit reveal, all through the five CP2 libraries.
- **What I built:** `demo/cp2_showcase.cpp`, `cmake/cp2_showcase.cmake`, the
  seven committed showcase GIFs and `docs/cp2/notes/showcase.md`.
- **Challenges:** a vacuum polygon and fan node both named `cap` collided; an
  unused `start` variable broke the warning-free build; four captions were
  67–72 characters and ran off the 400 px canvas, so they were shortened to
  61–63; the mirrored sofa did not fit when every other piece was in the
  scene, so the context was reduced; the 22-second GIF is 1,805,728 bytes, so
  the demo now fails if any output reaches 2 MiB.

### 2.6 Team member to confirm — [HM] Claude — 5:40–6:00

**On screen:** the group-members table in `README.md`.

The README lists `[HM] Claude (@claude)`, but there is no CP2 notes file or
issue assigned to that entry. `[TEAM TO CONFIRM: whether Claude should appear
as a speaking member. If yes, add the speaker, a 20-second block and the
shared-screen item here.]`

---

## 3. Project components — 6:00–8:00

**On screen:** the dependency table below, then the actual folder for each
component as it is named.

| Library | Built on | Used by |
|---|---|---|
| `raster` | — | `roomcraft`, `xform`, `output`, `scene`, `view` |
| `roomcraft` | `raster`, `xform` | `view`, demos |
| `xform` | `raster` | `roomcraft`, `scene`, `view`, `anim` |
| `scene` | `xform` | demos, showcase |
| `view` | `xform`, `roomcraft` | demos, showcase |
| `anim` | `xform` | demos, showcase |
| `output` | `raster` | demos, showcase |
| showcase | all of the above | CTest test 21 |

### 3.1 `raster` and `roomcraft` — Victor — 6:00–6:20

**On screen:** `src/raster/` and `src/roomcraft/furniture.h`.

`raster` owns the canvas, lines, polygons, clipping, fills, circles, ellipses
and curves. `roomcraft` adds the furniture model, world footprints, hit testing
and room-fit checks. Every later component eventually draws through `raster`;
`roomcraft` is where the 2D layout data lives.

### 3.2 `xform` — Victor — 6:20–6:40

**On screen:** `src/xform/mat3.h`, `src/xform/pose.h`.

`xform` turns a point, polygon or local footprint into room coordinates. It
provides the 3×3 matrix and `Pose` that every other CP2 part consumes. Poses are
what the animation engine interpolates, and matrices are what the scene graph
and cameras compose.

### 3.3 `scene` — Kristein — 6:40–7:00

**On screen:** `src/scene/node.h`, `src/scene/extrude.h`.

`scene` stores a shape in local coordinates plus a pose relative to its parent.
`drawNode` composes those poses down the tree, so spinning a fan hub moves its
blades and moving a room moves its door. `extrude` turns a 2D footprint into a
wireframe prism for the 3D view.

### 3.4 `view` — Tobiko — 7:00–7:20

**On screen:** `src/view/camera2d.h`, `src/view/mat4.h`,
`src/view/camera3d.h`.

`view` is the camera layer. `Camera2D` maps a room window to a viewport and
clips everything to it. `Mat4`, `lookAt`, `OrbitCamera` and the projection
matrices produce the 3D image. The last step of both paths is still `raster`
line and fill drawing.

### 3.5 `anim` — Joy — 7:20–7:40

**On screen:** `src/anim/easing.h`, `src/anim/track.h`, `src/anim/path.h`.

`anim` contains no drawing code. It turns time into poses: fixed timestep,
interpolation, easing, sorted keyframes and arc-length path motion. Those poses
go into `roomcraft::Furniture`, and `xform::poseMatrix` turns them into
matrices for the renderers.

### 3.6 `output` — Victor — 7:40–7:55

**On screen:** `src/output/animation_writer.h`, `src/output/text.h`.

`output` is the only writer. `AnimationWriter` streams uploaded canvases into
a GIF, and `drawText` puts captions and counters into the frame. It sits on
top of `gif` and `text` and is used by every CP2 demo.

### 3.7 Showcase integration — Emmanuel — 7:55–8:00

**On screen:** `demo/cp2_showcase.cpp`.

The showcase is not another library. It is the composition layer: one
`renderFrame` switch and one timeline that call the other six components in the
order the story needs.

---

## 4. Techniques and algorithms — all members — 8:00–11:45

Each block answers the same five questions: what the technique is, how it was
implemented, which component uses it, what it achieves in RoomCraft, and which
GIF or image shows it.

### 4.1 Homogeneous 2D transforms and pivots — Victor — 8:00–8:25

**On screen:** `docs/images/cp2/transform_pivot.png`, then
`src/xform/mat3.cpp`.

1. **What:** 3×3 homogeneous matrices for translation, rotation, scaling,
   reflection and shear, plus composites about a pivot.
2. **How:** `Mat3` stores `m[row][column]`; points are column vectors. Pivot
   rotation is `T(+P) · R(a) · T(−P)`, so the pivot goes to the origin, turns,
   and returns.
3. **Which component:** `xform`, consumed by `roomcraft`, `scene`, `view` and
   `anim`.
4. **Achieves:** a door swings on its hinge instead of the canvas origin, and
   one matrix can represent an arbitrary chain of editor operations.
5. **Show:** `transform_pivot.png`; the door is drawn at 0°, −22.5°, −45°,
   −67.5° and −90° about its hinge.

### 4.2 GIF89a, LZW and bitmap text — Victor — 8:25–8:50

**On screen:** `docs/images/cp2/output_first_animation.gif`, then
`docs/images/cp2/output_font.png`.

1. **What:** a from-scratch GIF89a container with LZW compression, a global
   palette and a 5×7 bitmap font.
2. **How:** `AnimationWriter` streams frames into `GifWriter`; `lzwCompress`
   builds a variable 9–12-bit dictionary and packs codes least-significant bit
   first; `drawText` copies a 35-cell glyph table into the canvas.
3. **Which component:** `output`; every CP2 animation and caption uses it.
4. **Achieves:** looping animations and labelled frames with no external
   library. The first animation is 60 frames at 0.05 s in 625 KB.
5. **Show:** `output_first_animation.gif` and `output_font.png`; the font sheet
   shows all 95 glyphs at scales 1, 2 and 3.

### 4.3 Hierarchical transforms and the scene graph — Kristein — 8:50–9:15

**On screen:** `docs/images/cp2/scene_fan.gif`, then `src/scene/node.cpp`.

1. **What:** a tree of nodes with poses relative to their parent, drawn with a
   recursive matrix stack.
2. **How:** `world = parent * poseMatrix(node.pose)`; `drawNode` transforms the
   local shape, fills and outlines it, then recurses into children. An empty
   shape is a group.
3. **Which component:** `scene`, used by the fan, swivel chair and door demos
   and by the showcase.
4. **Achieves:** spinning one hub spins four blades; rotating a room carries
   its walls, door, rug and cabinet without separate animation code.
5. **Show:** `scene_fan.gif`; the hub spins while the whole fan sways.

### 4.4 Extrusion and the 2D-to-3D mapping — Kristein — 9:15–9:40

**On screen:** `docs/images/cp2/scene_room_3d.gif`, then
`src/scene/extrude.cpp`.

1. **What:** duplicate a 2D footprint at floor height and object height, then
   join the bottom ring, top ring and uprights.
2. **How:** plan `(x, y)` at height `h` maps to world `(X = x, Y = h, Z = y)`.
   An `n`-vertex footprint produces `2n` vertices and `3n` edges.
3. **Which component:** `scene::extrude` and `scene::extrudedRoom`, consumed by
   `view::drawWireMesh` and the showcase.
4. **Achieves:** the existing floor plan becomes 3D with no second model
   format; the plan stays the only source of truth.
5. **Show:** `scene_room_3d.gif`; furniture and walls rise from flat footprints
   into wireframe prisms.

### 4.5 2D camera and viewport clipping — Tobiko — 9:40–10:05

**On screen:** `docs/images/cp2/view_zoom.gif`, then `src/view/camera2d.cpp`.

1. **What:** window-to-viewport mapping, pan, zoom about an anchor and clipping
   to the destination rectangle.
2. **How:** `M = T(viewport corner) · S(viewport size / window size) ·
   T(−window corner)`; `zoomAbout` scales the window about the anchor by
   `1/factor`; polygons use Sutherland–Hodgman and lines use Cohen–Sutherland
   after the transform.
3. **Which component:** `view::Camera2D`, used by the editor view, minimap and
   the showcase's zoom scene.
4. **Achieves:** the plan is no longer locked to 1 pixel = 1 cm; the armchair
   stays under the same pixel while the camera zooms, and clicks map back
   through the inverse matrix.
5. **Show:** `view_zoom.gif`; the orange cross on the armchair does not move
   while the rest of the room grows away from it.

### 4.6 Orbit, perspective and near-plane clipping — Tobiko — 10:05–10:30

**On screen:** `docs/images/cp2/view_projection.gif`, then
`docs/images/cp2/view_reveal.gif`.

1. **What:** a spherical orbit camera, `lookAt`, orthographic and perspective
   projection, perspective divide and near-plane clipping in clip space.
2. **How:** `eye = target + r(cos(el)sin(az), sin(el), cos(el)cos(az))`;
   `lookAt` builds the camera basis; perspective puts `−z` into `w`; each edge
   is clipped against `z + w ≥ 0` before dividing, because a negative `w`
   would mirror the point.
3. **Which component:** `view::OrbitCamera`, `Mat4` and `drawWireMesh`, used by
   the projection demo and the showcase reveal.
4. **Achieves:** correct wireframes even when geometry is behind the camera,
   and a stable top view at elevation 90 using the tangent up vector.
5. **Show:** `view_projection.gif` for orthographic vs perspective, then
   `view_reveal.gif` for the full plan-to-3D orbit.

### 4.7 Easing, interpolation and keyframe tracks — Joy — 10:30–10:55

**On screen:** `docs/images/cp2/anim_easing.gif`, then
`docs/images/cp2/anim_arrange.gif`.

1. **What:** linear blending, shortest-arc angle interpolation, polynomial
   easing and sorted keyframe tracks.
2. **How:** `lerp` uses the two-weight form; `lerpAngle` wraps the difference
   into `(−180°, 180°]`; `ease` provides linear, quadratic in/out and cubic
   smoothstep; `PoseTrack` uses `upper_bound` to find the surrounding keys and
   applies the destination key's easing.
3. **Which component:** `anim`, driving `xform::Pose` on `roomcraft::Furniture`.
4. **Achieves:** furniture accelerates and decelerates naturally, turns the
   short way across 0°/360°, and stays rigid because poses, not matrix
   entries, are interpolated.
5. **Show:** `anim_easing.gif` compares four chairs on the same keyframes;
   `anim_arrange.gif` fills the room from staggered tracks.

### 4.8 Constant-speed Catmull–Rom path motion — Joy — 10:55–11:20

**On screen:** `docs/images/cp2/anim_vacuum.gif`, then
`src/anim/path.cpp`.

1. **What:** Catmull–Rom path following with an arc-length table for constant
   physical speed and tangent heading.
2. **How:** the path is sampled into cumulative chord lengths
   `L_k`; a distance fraction `s` finds `d = s · L_total` by binary search and
   interpolates the spline parameter; the heading comes from a small step
   forward along the curve.
3. **Which component:** `anim::poseOnPathConstantSpeed`, used by the vacuum
   demo and the showcase.
4. **Achieves:** the vacuum moves at 195 cm/s along a 780 cm path and turns to
   face each bend without racing on long segments or crawling on short ones.
5. **Show:** `anim_vacuum.gif`; the dashed trail has equal spacing because it
   is sampled by arc length, not raw spline parameter.

### 4.9 Integration timeline, collision and reflection — Emmanuel — 11:20–11:45

**On screen:** `docs/images/cp2/showcase.gif`, then
`docs/images/cp2/showcase_3_mirror.gif`.

1. **What:** a fixed-step six-scene timeline that composes the five CP2
   libraries, with live `fitsInRoom` feedback and reflection.
2. **How:** `sceneAt` splits 22 s into six durations; `renderFrame` dispatches
   to a scene renderer; the mirror scene samples a `PoseTrack`, calls
   `roomcraft::fitsInRoom` every frame, and uses `scaleX = -1` for the
   reflected L-sofa.
3. **Which component:** `demo/cp2_showcase.cpp`, using `xform`, `output`,
   `scene`, `view` and `anim`.
4. **Achieves:** one continuous story with the same renderers as the short
   clips, and a collision signal that is geometry-driven rather than scripted.
5. **Show:** `showcase.gif`, then `showcase_3_mirror.gif`; the sofa turns red
   through the wall and becomes green again when the reflected shape fits.

---

## 5. Recommendations — all members — 11:45–13:45

### 5.1 Plans for Checkpoint 3 — Victor — 11:45–12:20

**On screen:** the CP3 row of the issue board and the CP2 limitations section
of `README.md`.

“Checkpoint 3 is the OpenGL integration. The renderer should move from writing
pixels on a software canvas to real-time GPU drawing, while keeping the CP2
scene data, poses, cameras and animation logic. The interactive editor comes
back at that point: select, drag, rotate, scale and undo in a live viewport.
Lighting and colour pickers follow once there is an editor to attach them to.
The CP2 limitations that matter most are hidden-line removal, polygon clipping
in 3D and real-time interaction.”

`[TEAM TO CONFIRM: the exact CP3 deliverables and the member-to-task split.]`

### 5.2 Plans for the final submission — Emmanuel — 12:20–12:45

**On screen:** a final roadmap slide or a blank issue list.

“For the final, the editor should be usable end to end: place furniture, open
the 3D view, adjust materials and lighting, and save or reload a layout. The
CP2 animation pipeline can remain as a guided tour or presentation mode, but it
should no longer be the only way to move the camera.”

`[TEAM TO CONFIRM: final deliverables, demo scenario, presentation date and
which member owns the final integration.]`

### 5.3 Lessons learnt — each member — 12:45–13:30

**On screen:** one bullet per member; fill each line before recording.

- **Victor:** `[TEAM TO FILL: one lesson from the transform/output/docs work.]`
- **Kristein:** `[TEAM TO FILL: one lesson from the scene/3D work.]`
- **Tobiko:** `[TEAM TO FILL: one lesson from the camera/projection work.]`
- **Joy:** `[TEAM TO FILL: one lesson from the animation work.]`
- **Emmanuel:** `[TEAM TO FILL: one lesson from the integration/showcase work.]`
- **Whole team:** `[TEAM TO FILL: one lesson about planning, review or
  integration that everyone agrees on.]`

### 5.4 Close — Victor — 13:30–13:45

**On screen:** `showcase.gif` looping, with the repository URL and the
techniques table visible if the recording style allows a split screen.

“This is RoomCraft Checkpoint 2: a pure C++ software renderer, a scene graph,
two camera systems, an animation engine, a GIF writer and a six-scene
integration showcase. The next step is OpenGL and the interactive editor.”

---

## Recording checklist

- [ ] Every speaker has a named block and a rough time.
- [ ] The five guideline sections are each announced on screen.
- [ ] Every technique block names its GIF or image.
- [ ] `showcase.gif` is shown near the start and again at the close.
- [ ] All `[TEAM TO FILL: …]` blanks are replaced by the team, not guessed.
- [ ] The final video length is within the course limit.
- [ ] Playback and audio are checked on the actual recording machine.
