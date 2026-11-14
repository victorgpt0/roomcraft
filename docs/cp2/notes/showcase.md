# Part 4b — Final integration showcase: presenter notes

Issue #9, branch `cp2/showcase`.

**What this part is.** Parts 1–4 and 5a built the transform core, output,
hierarchy/3D geometry, viewing/projection, animation engine, GIF writer and
text renderer. This part is the integration: one continuous 22-second story
that uses all five libraries together, plus a shorter GIF for each scene.

- `showcase.gif` — the complete six-scene animation (400 × 300, 12 fps).
- `showcase_1_door.gif` — room reveal, hierarchy and the door swinging on its
  hinge.
- `showcase_2_arrange.gif` — furniture arriving from keyframes with easing.
- `showcase_3_mirror.gif` — the sofa is pushed through a wall and turns red,
  then a reflection across the diagonal carries it into the opposite corner.
- `showcase_4_fan_vacuum.gif` — a hierarchical ceiling fan and a vacuum
  following a constant-speed Catmull–Rom path.
- `showcase_5_zoom.gif` — the 2D camera zooms about the armchair and back out.
- `showcase_6_reveal.gif` — extruded furniture and walls drawn as a wireframe,
  with the orbit camera tilting from the plan view into 3D.

All drawing still goes through the CP1 rasterizer. The demo only calls the
shared CP2 interfaces; it does not change another part's files.

**Run it**

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure   # 21 tests, all pass
./build/cp2_showcase demo_output/cp2
```

The demo writes seven GIFs and fails if any output is missing or reaches
2,000,000 bytes (2 MB read strictly, not 2 MiB). It also fails if the scene 3
mirror pose is not a true reflection (see scene 3). Current sizes:

| File | Frames | Size |
|---|---:|---:|
| `showcase_1_door.gif` | 36 | 199.5 KB |
| `showcase_2_arrange.gif` | 48 | 341.4 KB |
| `showcase_3_mirror.gif` | 46 | 349.0 KB |
| `showcase_4_fan_vacuum.gif` | 48 | 400.5 KB |
| `showcase_5_zoom.gif` | 38 | 203.4 KB |
| `showcase_6_reveal.gif` | 48 | 324.7 KB |
| `showcase.gif` | 264 | 1814.3 KB |

**One room, one story.** Scene 2 places the furniture with the bottom-left
corner deliberately empty. Scene 3 starts from exactly that layout and ends
with the sofa mirrored into that corner, and scenes 4, 5 and 6 all draw the
same final layout (`finalLayout()` / `finalPose()`), so nothing jumps between
scenes in the continuous GIF.

`cp2_showcase` is also registered as CTest test 21, so the test suite writes
and checks the whole integration.

---

## Suggested presenting order

1. **Timeline and frame assembly** — `sceneAt`, `renderFrame` and
   `output::AnimationWriter` in `demo/cp2_showcase.cpp`.
2. **Hierarchy and pivot rotation** — scene 1, `showcase_1_door.gif`.
3. **Keyframes and easing** — scene 2, `showcase_2_arrange.gif`.
4. **Collision feedback and reflection** — scene 3,
   `showcase_3_mirror.gif`.
5. **Hierarchy plus path animation** — scene 4,
   `showcase_4_fan_vacuum.gif`.
6. **Window-to-viewport zoom** — scene 5, `showcase_5_zoom.gif`.
7. **Extrusion, perspective projection and orbit** — scene 6,
   `showcase_6_reveal.gif`.

---

## 1. The empty room appears and the door swings on its hinge

1. **What technique?** Pivot rotation through a parent/child hierarchy. The
   door's local origin is its hinge, so rotating the door node in its parent's
   coordinate system is a rotation about the hinge. The room also grows about
   its centre with a `T(centre) * S * T(-centre)` composite.
2. **How was it implemented?** `renderDoorScene()` in
   `demo/cp2_showcase.cpp` builds the appear transform with
   `xform::Mat3::translation`, `xform::Mat3::scaling` and `operator*`, then
   transforms the floor and grid through it with
   `xform::transformPolygon`/`transformPoint`. The room is a `scene::Node`
   tree: `room -> walls + door -> knob + hinge`. `scene::drawNode` composes
   each child pose with its parent, and the whole tree is pushed through
   `view::windowToViewport(camera) * grow`.
3. **Which components use it?** `xform`, `scene`, `view`; the swing arc uses
   `raster` line drawing through `view::drawLineThroughCamera`.
4. **What does it achieve?** The door and knob move as one rigid assembly
   without any hand-written knob arc; the knob inherits both the door's hinge
   rotation and the room's appear scaling.
5. **Result:** `showcase_1_door.gif` (scene 1 of `showcase.gif`), 36 frames.

## 2. Furniture arranges itself from keyframes with easing

1. **What technique?** Keyframe interpolation of `Pose` values with linear and
   eased segments, staggered per piece so the room fills in a readable order.
2. **How was it implemented?** `arrangementSchedule()` creates one
   `anim::PoseTrack` per furniture spec and adds three keys: a small, rotated
   pose at the doorway, a lifted midpoint, and the target pose. It uses
   `Easing::Linear`, `Easing::EaseIn` and `Easing::EaseOut`; each frame calls
   `PoseTrack::sample(t)`. The midpoint blends position with `anim::lerp` and
   angle with `anim::lerpAngle`. `renderArrangeScene()` builds a
   `roomcraft::Furniture` from the sampled pose, draws the destination as a
   faint `roomcraft::worldFootprint` outline, and draws the moving piece with
   `view::drawFurnitureThroughCamera`.
3. **Which components use it?** `anim`, `roomcraft`, `view`.
4. **What does it achieve?** Position, rotation and scale interpolate
   independently in pose space, so shapes stay rigid while they turn; the
   ghost outline makes the destination of each move clear to the viewer.
5. **Result:** `showcase_2_arrange.gif` (scene 2 of `showcase.gif`), 48 frames.

## 3. A piece is pushed through a wall, turns red, and is mirrored

1. **What technique?** A footprint-versus-room collision check plus a
   reflection across the diagonal line `y = x`. The L-sofa's corner points to
   the top-right, so it fits the top-right corner of the room. Reflecting it
   across `y = x` turns its top bar into a left bar and its right leg into a
   bottom bar: the same L now fits the bottom-left corner, the opposite one.
   A plain left-right flip (`scaleX = -1` alone) would only fit the top-left.
2. **How was it implemented?** `renderMirrorScene()` starts from scene 2's
   final layout. A `PoseTrack` holds the sofa at `kSofaArrived`, pushes it to
   x = 568 (through the right wall), pulls it back, then eases to
   `kSofaMirrored = {(140, 320), -90°, scaleX -1}`. In pose form that is
   `R(-90) * S(-1, 1)`: `(x, y) -> (-x, y) -> (y, x)`, which is exactly the
   reflection across `y = x`. `checkDiagonalMirror()` proves it at start-up:
   it builds the textbook composite `Mat3::rotation(45) *
   Mat3::reflectionAcrossXAxis() * Mat3::rotation(-45)` (turn the mirror line
   onto the x axis, reflect, turn back), compares it entry by entry with
   `xform::poseMatrix(kSofaMirrored)`, and checks `xform::determinant == -1`.
   The demo, and so the CTest test, fails if they differ. Every frame calls
   `roomcraft::fitsInRoom(sofa, roomOutline())`; a failed fit changes the
   fill to `kWarningFill` and adds a `kWarning` outline through
   `view::drawPolygonOutlineThroughCamera`. While the sofa travels, its
   destination is drawn as a faint outline, as in scene 2.
3. **Which components use it?** `roomcraft` (`Furniture`, `fitsInRoom`,
   `worldFootprint`), `xform` (`Mat3::rotation`, `reflectionAcrossXAxis`,
   `poseMatrix`, `determinant`, `transformPolygon`), `anim` (`PoseTrack`),
   `view` (camera-space drawing).
4. **What does it achieve?** The red frame is a visible, geometry-driven
   failure signal rather than a scripted colour change. The reflection is real
   geometry: the determinant of -1 is what tells a reflection apart from any
   rotation, and it is why the same L-shaped footprint fits snugly (20 cm
   from both walls) into the corner diagonally opposite where it started.
5. **Result:** `showcase_3_mirror.gif` (scene 3 of `showcase.gif`), 46 frames.

## 4. The ceiling fan spins and the vacuum follows its path

1. **What technique?** Two-level hierarchy for the fan and constant-speed
   Catmull–Rom path following for the vacuum.
2. **How was it implemented?** `renderFanVacuumScene()` builds
   `fan -> hub -> four blades + cap` with `scene::Node`; rotating the hub
   `720°` over the scene spins every blade, while the fan node itself sways.
   The vacuum pose comes from
   `anim::poseOnPathConstantSpeed(path, progress, 512)`, so the heading follows
   the spline and the speed stays uniform in arc length. The eight waypoints
   start at the doorway, go round the dining table, into the crook of the
   mirrored sofa, along the foot of the bed and back over the rug, keeping
   the 16 cm vacuum clear of every piece in `finalLayout()`. The dashed trail is
   drawn from fixed arc-length chord samples, not raw spline parameters. The
   vacuum body, top cap and direction arrow are transformed through
   `xform::poseMatrix` and drawn with `view` helpers.
3. **Which components use it?** `scene`, `anim`, `xform`, `roomcraft`
   (`makeCircleFootprint`), `view`, `raster`.
4. **What does it achieve?** The fan's blades inherit the hub rotation for free,
   and the vacuum turns to face its direction of travel without speeding up on
   long spline segments or slowing down in tight bends.
5. **Result:** `showcase_4_fan_vacuum.gif` (scene 4 of `showcase.gif`),
   48 frames.

## 5. The 2D camera zooms into one piece and back out

1. **What technique?** Window-to-viewport mapping with a camera zoom about a
   fixed world point.
2. **How was it implemented?** `renderZoomScene()` starts from `planCamera()`,
   which uses `view::Camera2D`, `view::matchViewportAspect` and the shared
   `view::windowToViewport` mapping. It then calls
   `view::zoomAbout(camera, factor, kArmchairCentre)` with a factor from 1 to
   3.3 and an eased hold. `view::roomToScreen` places an orange cross on the
   armchair, using `raster::drawLineBresenham` directly.
3. **Which components use it?** `view`, `raster`.
4. **What does it achieve?** The armchair stays under the same screen pixel
   while the window shrinks around it; the room and furniture are drawn through
   the same camera helpers, so only the mapping changes. Measured over all 38
   frames, the cross stays at pixel x = 201–211, y = 100–111: it never moves
   sideways, and only by one pixel vertically from rounding.
5. **Result:** `showcase_5_zoom.gif` (scene 5 of `showcase.gif`), 38 frames.

## 6. 2D-to-3D reveal: extrusion, perspective projection and orbit

1. **What technique?** Extruding the same 2D footprints into wireframe solids,
   projecting them with a perspective matrix, and orbiting the camera from a
   straight-down plan view to a 3D view while making one full turn.
2. **How was it implemented?** `solids3D()` calls
   `scene::extrudedRoom(roomOutline(), 240)` for the walls and
   `scene::extrude(roomcraft::worldFootprint(piece), height)` for every piece,
   producing `xform::WireMesh` data. `renderRevealScene()` builds
   `view::Mat4::perspective(45°, aspect, 10, 5000)` and a `view::OrbitCamera`
   with elevation `90° -> 28°` and azimuth `0° -> 360°`. It draws the filled
   plan with `view::fillPlanPolygon` under the wireframes, the floor grid with
   `view::drawWireMesh`, and each solid again with `view::drawWireMesh`, all
   clipped to the viewport.
3. **Which components use it?** `scene` (`extrude`, `extrudedRoom`), `xform`
   (`Vec3`, `WireMesh`), `view` (`Mat4`, `OrbitCamera`, `viewMatrix`,
   `fillPlanPolygon`, `drawWireMesh`), `roomcraft`, `raster`.
4. **What does it achieve?** The reveal extrudes the exact footprints of
   `finalLayout()`, including the mirrored sofa, so the 3D objects correspond
   one-for-one to the furniture the viewer just watched in scenes 3–5. Keeping the filled plan underneath preserves the
   colour coding during the tilt.
5. **Result:** `showcase_6_reveal.gif` (scene 6 of `showcase.gif`), 48 frames.

## Cross-scene integration map

| Layer | Functions used | Role in the showcase |
|---|---|---|
| `raster` | `Canvas`, `Polygon`, `fillPolygon`, lines, circles | Final pixel drawing for every scene. |
| `xform` | `Mat3` (`rotation`, `reflectionAcrossXAxis`, `operator*`), `determinant`, `poseMatrix`, `transformPoint`, `transformPolygon`, `Vec3`, `WireMesh` | Pivots, poses, the diagonal reflection check and 3D mesh data. |
| `roomcraft` | `Furniture`, `worldFootprint`, `fitsInRoom`, `makeCircleFootprint` | Shared furniture model and collision check. |
| `scene` | `Node`, `drawNode`, `extrude`, `extrudedRoom` | Door/fan hierarchies and 3D extrusion. |
| `anim` | `FixedTimestep`, `PoseTrack`, `ease`, `lerp`, `lerpAngle`, `poseOnPathConstantSpeed` | Deterministic timing, keyframes and path motion. |
| `view` | `Camera2D`, `windowToViewport`, `zoomAbout`, `roomToScreen`, `Mat4::perspective`, `OrbitCamera`, `fillPlanPolygon`, `drawWireMesh` | 2D mapping, zoom, perspective projection and orbit. |
| `output` | `AnimationWriter`, `drawText` | Captions, progress bar and GIF output. |

`FixedTimestep` supplies frame indices for both the short clips and the
continuous story. `sceneAt()` splits the 22-second timeline into the six scene
durations, and `renderFrame()` clears the canvas, fills the paper viewport,
dispatches to the scene renderer, then draws the frame and caption chrome.
The continuous GIF uses the same per-scene renderers as the short clips, so
there is only one implementation of each technique.

## Challenges

- **A name collision while building the fan.** The vacuum cap polygon and the
  fan cap node were both named `cap` in `renderFanVacuumScene()`, and the
  compiler rejected the redeclaration. Renamed the polygon to `vacuumCap`.
- **An unused variable in the first scene.** The first camera draft declared a
  `start` point that the final door-swing version did not use, which fails the
  warning-free requirement. Removed it.
- **Captions ran off the right edge.** During visual QA the scene 2, 3, 4 and 6
  subtitles were 67–72 characters long; at the bitmap font's 6 px per character
  plus the 8 px margin they exceeded the 400 px canvas. Rewrote them to 61–63
  characters so every line fits inside the frame.
- **The mirrored sofa would not fit if every other piece was kept.** The first
  scene 3 included all target furniture, which left no clear bottom-left
  corner. The first fix dropped the wardrobe, dining table, armchair and sofa
  from scene 3's backdrop and landed the sofa at `(170, 345)`. Watching the
  continuous GIF frame by frame showed that this broke the story: four pieces
  vanished at the cut from scene 2, the rug changed from a rectangle to an
  ellipse, the sofa ended about 30 cm from both walls rather than in the
  corner, and scenes 4–6 put it back in the top-right as if scene 3 never
  happened. The final fix changes the layout instead of the backdrop. The bed
  turns 90° against the left wall, the wardrobe moves to the right wall and
  the dining table to the middle, so the bottom-left corner is free. Scene 3
  then starts from scene 2's layout, and scenes 4–6 keep the mirrored sofa
  through `finalLayout()`.
- **`scaleX = -1` alone mirrors to the wrong corner.** A left-right flip
  turns a top-right L into a top-left L, which is not the opposite corner. The
  reflection that maps top-right to bottom-left is across the diagonal
  `y = x`. `Pose` has no "mirror line" field, but `scaleX = -1` combined with
  a -90° turn is exactly that reflection. `checkDiagonalMirror()` turns this
  into a run-time check against `R(45) * Rx * R(-45)`, so a sign mistake in
  the pose would fail the CTest test rather than just look slightly wrong.
- **The new layout put the vacuum through the dining table.** After the
  table moved to `(330, 360)`, the frame-by-frame check showed the vacuum
  clipping the table's top edge between the second and third waypoints.
  Lifting those waypoints to `(450, 305)` and `(330, 268)` keeps the spline,
  including its bulge between keys, at least 16 cm above the table.
- **The continuous GIF is the size risk.** The 22-second story is 264 frames
  at 400 × 300 and now measures 1,857,851 bytes, close enough to the limit that
  future edits could break the rule silently. The demo checks every written
  GIF with `std::filesystem::file_size` and fails if any file reaches
  2,000,000 bytes. That limit was originally 2 MiB (2,097,152 bytes), which
  would have let a file of, say, 2.05 MB pass a "under 2 MB" rule.
- **The scene lengths and the total could disagree.** `kTotalSeconds` was a
  hand-typed 22 next to the six scene lengths. It is now computed from
  `kSceneSeconds` at compile time, so changing one scene's length cannot cut
  off the end of the continuous GIF.
- **This part could not start until its five blockers were merged.** When the
  work was first attempted, `dev` contained only `xform` and `output`, and the
  `scene`, `view` and `anim` branches were still empty. Five of the six scenes
  need at least one of those three, so the work waited for them rather than
  stubbing another part's interfaces.

## Known limitations

- Every scene uses the shared 5.6 m × 4.0 m room and fixed furniture layout;
  this is deliberate for the showcase but means the demo cannot rearrange the
  room interactively.
- The 3D reveal is wireframe only, so there is no hidden-line removal and the
  far edges of solids remain visible.
- GIF frame delays are stored in centiseconds, so 12 fps is written as 80 ms
  per frame (12.5 fps on playback); the 264-frame story plays for about 21.1
  seconds rather than exactly 22.
