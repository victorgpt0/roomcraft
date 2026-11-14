# Part 3 — Viewing and projection: presenter notes

Issue #7, branch `cp2/view`.

**What this part is.** Checkpoint 1 drew the floor plan at a fixed
1 pixel = 1 cm. This part adds **cameras**: a 2D camera that shows any part
of the room at any size in any rectangle of the canvas (pan, zoom, minimap,
mouse clicks mapped back to the room), and a 3D camera that orbits the room
and draws it as a wireframe in perspective or orthographic projection. Both
are "just matrices": a `Mat3` for 2D, a `Mat4` for 3D, built once per frame
and applied to every vertex.

**Run it**

```sh
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure   # 16 tests, all pass
./build/cp2_view_demo                        # writes demo_output/cp2/view_*.gif
```

| Test | Checks | Result |
|---|---|---|
| `test_view` | 1,281 | 0 failed |
| `cp2_view_demo` (writes all five GIFs) | runs as a test | pass |
| The 14 tests already on `dev` | unchanged | pass |

Tested with GCC 15.2 (MSYS2 UCRT64) on Windows 11, `-Wall -Wextra -Wpedantic`,
no warnings.

**Files**

| File | What is in it |
|---|---|
| `src/view/camera2d.h/.cpp` | `Camera2D`, `windowToViewport`, pan, zoom, aspect, `screenToRoom`, drawing through a camera |
| `src/view/mat4.h/.cpp` | `Mat4`: translation, scaling, rotations, `lookAt`, `perspective`, `orthographic` |
| `src/view/camera3d.h/.cpp` | `OrbitCamera`, `eyePosition`, `viewMatrix`, `clipToNearPlane`, `drawWireMesh` |
| `tests/test_view.cpp` | the tests |
| `demo/cp2_view_demo.cpp` | the five animations |

**The GIFs.** All five are written by the program itself with Part 5's
`output::AnimationWriter`, at 20 frames per second, each under 2 MB. The
three best are copied to `docs/images/cp2/`: `view_zoom.gif`,
`view_projection.gif` and `view_reveal.gif`. `view_minimap.gif` and
`view_orbit.gif` are in `demo_output/cp2/` after running the demo. Frames were
checked by decoding the files with a separate small decoder and looking at
individual frames; **watch them play in a browser before presenting.**

---

## Suggested presenting order

1. Window-to-viewport mapping (show the derivation in `camera2d.cpp`).
2. Pan and zoom (`view_zoom.gif`).
3. Clipping to the viewport, and the minimap (`view_zoom.gif`, `view_minimap.gif`).
4. 3D transforms: `Mat4` (`mat4.h`, rotation tests).
5. The view matrix (`lookAt`).
6. Orbit with spherical coordinates (`view_orbit.gif`).
7. Orthographic versus perspective projection (`view_projection.gif`).
8. The perspective divide and near-plane clipping (the two tests in `testDrawWireMesh`).
9. Finish with `view_reveal.gif`: 2D plan → 3D, everything together.

---

## 1. Window-to-viewport mapping

1. **What technique?** The window-to-viewport transformation: the matrix that
   maps a rectangle of the world (the *window*, here a part of the room in
   cm) onto a rectangle of the screen (the *viewport*, in pixels).
2. **How was it implemented?** `view::windowToViewport()` in
   `src/view/camera2d.cpp`, built as a composite of three Part 1 matrices:

   ```
   M = T(viewport corner) * S(viewport size / window size) * T(-window corner)
   ```

   Read right to left: move the window's corner to the origin, stretch the
   window to the viewport's size, move the origin to the viewport's corner.
   The derivation (a point 30% across the window must land 30% across the
   viewport) is in the comment above the function. Room coordinates and
   pixels both have y pointing down (the CP1 convention), so unlike the
   textbook version no flip is needed.
3. **Which component uses it?** Every 2D view: the zoomed editor view, the
   minimap, and also the last step of the 3D pipeline (section 8).
4. **What does it achieve?** The floor plan is no longer tied to
   1 pixel = 1 cm. The CP1 view is the special case window = viewport, which
   gives the identity matrix (tested).
5. **Result:** `testWindowToViewport` checks that the four window corners
   land on the four viewport corners, the centre on the centre, and that the
   matrix entries are the derived `sx`, `vxmin - sx * wxmin`. Every frame of
   `view_zoom.gif` and `view_minimap.gif`.

## 2. Pan and zoom

1. **What technique?** Panning (moving the window) and zooming about a fixed
   point (scaling the window about that point), plus fixing the aspect ratio.
2. **How was it implemented?** In `src/view/camera2d.cpp`:
   - `pan(camera, dx, dy)` moves the window by (dx, dy) cm. The room appears
     to slide the *other* way on screen.
   - `zoomAbout(camera, factor, anchor)` moves every window corner `c` to
     `anchor + (c - anchor) / factor`, i.e. a scaling of the window about the
     anchor by 1/factor. The anchor's pixel is
     `vxmin + (anchor - wxmin) * vw / ww`; both `(anchor - wxmin)` and `ww`
     are divided by the same factor, so the anchor does not move on screen.
   - `matchViewportAspect(camera)` grows the window about its centre until
     its width/height ratio equals the viewport's, so `sx == sy` and circles
     stay round. It grows rather than shrinks so nothing asked for is cut off.
   - `screenToRoom(camera, pixel, room)` uses the *inverse* matrix
     (`xform::inverse`, Part 1) to turn a click on the screen into a room
     point.
3. **Which component uses it?** The editor view: zooming into the piece under
   the mouse, scrolling around the room, and selecting a piece by clicking on
   the zoomed view.
4. **What does it achieve?** Precise placement of small pieces, and a mouse
   click still hits the right piece at any zoom: `testScreenToRoom` zooms
   6.4× onto the nightstand, maps a screen point back through the inverse and
   `roomcraft::findFurnitureAt` returns the nightstand.
5. **Result:** `view_zoom.gif`: the camera pans to the armchair, then zooms
   3.5× about it, and the orange cross on the armchair's centre does not move
   while everything else grows away from it. `testZoomAbout` checks the anchor
   stays fixed, a point 200 px away moves to 400 px, and zooming in and out by
   the same factor returns the original window. `testPan` checks the
   60 px / 40 px movement for a 30 cm / 20 cm pan at zoom 2.

## 3. Clipping to the viewport

1. **What technique?** Clipping what the camera draws to the viewport, using
   the Checkpoint 1 algorithms: **Sutherland-Hodgman** for filled polygons and
   **Cohen-Sutherland** for lines, followed by the CP1 **scanline fill** and
   **Bresenham** line.
2. **How was it implemented?** In `src/view/camera2d.cpp`:
   `fillPolygonThroughCamera` (transform every vertex, clip the polygon with
   `raster::clipPolygonSutherlandHodgman` against the viewport rectangle,
   `raster::fillPolygonScanline`), `drawLineThroughCamera` (transform the two
   ends, `raster::drawLineClipped` = Cohen-Sutherland + Bresenham),
   `drawPolygonOutlineThroughCamera` (every edge as a clipped line) and
   `drawFurnitureThroughCamera` (fill + outline of each piece's
   `worldFootprint`). Two design decisions worth mentioning:
   - Clipping happens **after** the transform, in pixels, where the viewport is
     a plain axis-aligned rectangle (what Cohen-Sutherland needs; Sutherland-
     Hodgman needs a convex window, and a rectangle is convex).
   - The **outline** is drawn by clipping each original edge as a line, *not*
     by outlining the clipped polygon. Otherwise every piece cut by the
     viewport would get a false dark edge along the viewport border, and a
     concave piece (the L-shaped sofa) would show Sutherland-Hodgman's
     zero-width "bridge" edges.
   - Walls are drawn as filled rectangles 8 cm thick, not thick lines, so they
     get thicker as you zoom in like everything else.
3. **Which component uses it?** Every 2D view; several viewports can share
   one canvas without drawing over each other (main view + minimap).
4. **What does it achieve?** Pieces are cut exactly at the viewport edge, and
   nothing outside it is touched. Viewport pixels follow the same half-open
   rule as the scanline fill (viewport x 50..150 = pixel columns 50..149).
5. **Result:** `view_zoom.gif`, panning while zoomed: the sofa, rug and
   wardrobe slide in and out and are cut cleanly at the frame.
   `view_minimap.gif`: the orange rectangle is the main camera's *window*,
   drawn on the minimap through the minimap's own camera and clipped to it.
   The cross at the centre of the main view is mapped back to the room with
   `screenToRoom` and shown on the minimap. `testDrawThroughCamera` checks that
   a piece larger than the window fills **exactly** 100 × 70 pixels with no
   outline at the border, that a rotated concave sofa touches no pixel outside
   the viewport, and that a long line is clipped to exactly 100 pixels.

## 4. 3D transforms (Mat4)

1. **What technique?** 4×4 homogeneous matrices for 3D: translation, scaling,
   rotation about X, Y and Z, and products.
2. **How was it implemented?** `struct Mat4` in `src/view/mat4.h/.cpp`, the
   same conventions as Part 1's `Mat3`: `m[row][column]`, column vectors,
   `p' = M * p`, so in `A * B` B happens first. The 3D world uses the shared
   mapping plan (x, y) at height h → (X = x, Y = h, Z = y), with Y up. This is
   right-handed, so the rotations are the standard right-handed ones. Each
   rotation is the 2D rotation applied to the two coordinates perpendicular to
   the axis, in cyclic order x → y → z → x (comment in `mat4.cpp`).
3. **Which component uses it?** The 3D preview: the view and projection
   matrices, and Part 4's showcase.
4. **What does it achieve?** The same "one matrix per frame" idea as Part 1,
   one dimension up. One subtlety: seen from above, `rotationY(+a)` turns the
   plan **counter-clockwise**, the opposite of `Mat3::rotation(+a)` (clockwise
   on screen, because screen y points down). So a 3D piece turned like its 2D
   pose uses `rotationY(-angle)`; `testRotationY` checks exactly this.
5. **Result:** `testMat4Basics` (translation moves points but not directions
   (w = 0), rotations by 90° on hand-worked points), `testRotationY`,
   `testMat4Products` (T·S ≠ S·T, (A·B)·p = A·(B·p), four quarter turns are the
   identity).

## 5. The view matrix (lookAt)

1. **What technique?** The viewing transformation: rewrite every world point
   in the camera's own coordinate system.
2. **How was it implemented?** `Mat4::lookAt(eye, target, up)` in
   `src/view/mat4.cpp`. It builds the camera basis: forward
   `f = normalize(target - eye)`, right `r = normalize(f × up)`, true up
   `u = r × f`. The view matrix is a rotation whose rows are `r`, `u`, `-f`,
   applied after a translation by `-eye`. The derivation is in the comment.
3. **Which component uses it?** `view::viewMatrix(OrbitCamera)` and every 3D
   frame.
4. **What does it achieve?** After it, the camera is at the origin looking
   down −z with up along +y, which is the fixed setting the projection
   matrices assume.
5. **Result:** `testLookAt`: the eye maps to (0, 0, 0), the target to
   (0, 0, −distance), the three rows are perpendicular unit vectors, distances
   are preserved.

## 6. Orbit with spherical coordinates

1. **What technique?** Placing the camera on a sphere round the room with
   spherical coordinates (radius, azimuth, elevation).
2. **How was it implemented?** `view::eyePosition()` in
   `src/view/camera3d.cpp`:

   ```
   eye = target + r * ( cos(el) sin(az),  sin(el),  cos(el) cos(az) )
   ```

   That is the point (0, 0, r) tilted up by the elevation (rotation about X)
   and turned round by the azimuth (rotation about Y). Azimuth 0 is on the
   +Z side (the door side of the plan), elevation 90 is straight above.
   `viewMatrix()` = `lookAt(eye, target, upVector)`.

   **The up-vector problem.** The usual up (0, 1, 0) breaks at elevation 90:
   the camera then looks straight down, parallel to up, and `f × up = 0`. The
   camera basis falls apart. `view::upVector()` uses the tangent of the sphere
   in the elevation direction instead, which is always perpendicular to the
   line of sight. Below 90° it gives exactly the same matrix as (0, 1, 0)
   (tested); at 90° it points to the top of the plan, so the top view shows
   the plan the right way up.
3. **Which component uses it?** The orbit animation, the reveal, and Part 4's
   showcase.
4. **What does it achieve?** An animation only has to change one or two
   angles per frame; the camera always looks at the room from the same
   distance.
5. **Result:** `view_orbit.gif` (azimuth 0 → 360 at elevation 30).
   `testEyePosition`: known angles (elevation 90 = straight above for every
   azimuth), and for 264 angle pairs the eye is exactly `radius` from the
   target. `testViewMatrix`: at elevation 90, plan x goes right on screen and
   plan y goes down, like the 2D plan.

## 7. Orthographic versus perspective projection

1. **What technique?** Projection: mapping the 3D view volume onto the cube
   −1..1 (normalised device coordinates, NDC).
2. **How was it implemented?** In `src/view/mat4.cpp`:
   - `Mat4::orthographic(l, r, b, t, n, f)`: three 1D window-to-viewport
     mappings (x from l..r, y from b..t, depth from n..f onto −1..1). The
     bottom row stays 0 0 0 1, so w = 1.
   - `Mat4::perspective(fovY, aspect, n, f)`: by similar triangles a point
     appears at height y/d on a screen at distance 1, and the top of the screen
     is tan(fov/2), so `y_ndc = y / (d * tan(fov/2))`. The matrix puts the
     distance −z into w (bottom row 0 0 −1 0); the division happens afterwards
     (section 8). The depth row (A, B) is solved from "near plane → −1, far
     plane → +1" (worked in the comment).
3. **Which component uses it?** The 3D preview. In the demo, the orthographic
   box is sized as `distance * tan(fov/2)` so that the plane through the
   target is the same size in both views.
4. **What does it achieve?** Orthographic keeps parallel lines parallel and
   sizes true, which is good for measuring. Perspective makes far things
   smaller, the way a photo of the room would look.
5. **Result:** `view_projection.gif`: the same view matrix in both halves,
   only the projection differs. On the left the room's edges stay parallel;
   on the right they converge and the far wall is smaller. `testPerspective`:
   near plane → depth −1, far plane → +1, w = distance, a point twice as far
   away lands half as far from the centre (0.4 → 0.2), and the edge of the
   field of view reaches exactly ±1. `testOrthographic`: box corners → cube
   corners, w = 1, and the same x at two depths gives the same screen x.

## 8. The perspective divide and near-plane clipping

1. **What technique?** The perspective divide (x, y, z ÷ w), with clipping
   against the near plane in clip space **before** the divide, then the
   viewport mapping and CP1 line drawing.
2. **How was it implemented?** `view::drawWireMesh()` in
   `src/view/camera3d.cpp`. For each edge: vertices × viewProjection → clip
   coordinates; `clipToNearPlane()` keeps the part with `z + w ≥ 0` (the near
   plane is `z = −w`), finding the crossing by linear interpolation
   `t = d_a / (d_a − d_b)`; divide by w; NDC → pixels with `ndcToViewport()`;
   then `raster::drawLineClipped` (Cohen-Sutherland against the canvas or
   viewport, then Bresenham).

   **Why before the divide?** For a point behind the camera w is negative, so
   dividing flips x and y and the point appears mirrored on the other side of
   the screen. At the camera's own plane w = 0 (division by zero). An edge that
   runs from in front of the camera to behind it passes through w = 0, so the
   line between its two divided ends is not the picture of the edge at all.
   Clipping first, in clip space where everything is still linear, guarantees
   every divided point has w ≥ zNear > 0.

   **NDC → pixels is the 2D camera again.** `ndcToViewport()` is
   `windowToViewport` with window −1..1, after a reflection y → −y (NDC y
   points up, canvas y down). The same function from section 1, reused.
3. **Which component uses it?** Every 3D frame. `fillPlanPolygon()` uses the
   same projection to paint the coloured floor plan on the floor of the 3D
   view (each footprint projected, Sutherland-Hodgman-clipped and
   scanline-filled).
4. **What does it achieve?** Wireframes are correct even when part of the
   scene is behind the camera.
5. **Result:** `testDrawWireMesh`: an edge from 2 units in front of the camera
   to 2 units behind it draws exactly the 26 pixels from the centre to
   NDC x = 0.5, three-quarters of the way across (without clipping, the end behind the camera would divide to the *left*
   edge instead). An edge entirely behind the camera draws **no** pixels
   (unclipped it would be a full-width line). `testNearPlaneClip` checks the
   clipped point lies on the near plane with w = zNear.

## Finale: the reveal

`view_reveal.gif` puts it all together: the orbit camera starts at elevation
90, straight above the room, and tilts down to 28° while turning 30°. At the
start the picture *is* the 2D floor plan, because the floor is parallel to the
image plane and perspective only scales a plane parallel to the image plane.
Only the box tops, which are nearer, look a little larger. A long lens
(fov 25°, camera 15 m away) keeps that effect small.

---

## How this part connects to the others

- **Uses Part 1:** `Mat3`, `transformPoint/Polygon`, `inverse`, `Pose`,
  `worldFootprint`, `Vec3/Vec4/WireMesh`.
- **Uses Part 5:** `AnimationWriter` for the GIFs, `drawText` for captions.
- **For Part 2:** meshes from `scene::extrude` can go straight into
  `drawWireMesh` (the demo builds its boxes by hand, as asked).
- **For Part 4:** a camera move is just `OrbitCamera` angles or a `Camera2D`
  window changing per frame; `fillPlanPolygon` + `drawWireMesh` draw a scene.

---

## Challenges

1. **The compiler failed silently.** The first `cmake` configure failed with
   "compiler is not able to compile a simple test program" and no error
   message. Git Bash puts `/mingw64/bin` before the MSYS2 UCRT64 toolchain on
   PATH, so `cc1plus` loaded the wrong DLLs and exited. Later the test
   executables failed with Windows error `0xc0000139` (entry point not found)
   for the same reason. Fix: put `C:\msys64\ucrt64\bin` first on PATH before
   building and running. Not a code problem, but worth knowing on the team.
2. **The camera breaks when it looks straight down.** The contract defines
   elevation 90 as the floor-plan view, which is exactly where `lookAt` with
   up = (0, 1, 0) breaks (forward and up parallel, cross product zero). Solved
   with the tangent up vector (section 6); `testViewMatrix` checks the top
   view and that nothing changes at other angles.
3. **A test failed because of reused state.** In `testViewMatrix` the camera
   object was reused, and the top-view projection check ran while the camera
   was still at elevation 35 from the check before it (4 of 1,281 checks
   failed). The pixel positions it reported matched a tilted camera, which
   showed the library was right and the test was wrong; the test now sets the
   angles it needs.
4. **Walls were half cut off.** In the first version of `view_zoom.gif` the
   starting window was exactly the room, but the walls are 8 cm thick and
   stand half *outside* the room's inner outline, so the viewport clipped them
   in half. The starting window now has a 16 cm margin.
5. **GIF size limit.** `view_zoom.gif` came out at 2.04 MB, over the 2 MB
   limit. Shortened the holds and moves (224 → 188 frames, 1.76 MB). The
   reveal was cut from 140 to 128 frames to stay clear of the limit.
6. **The top view did not look like the plan at first.** With the camera
   9.5 m above the floor, the 2.5 m wall tops were 1.36× nearer than the floor
   and flared out strongly, and the plan was small. A narrower field of view
   from further away (25°, 15 m) reduces this to 1.2×.

## Known limitations

- Wireframe only: there is no hidden-line removal, so edges behind furniture
  are visible.
- Only the near plane is clipped in 3D. The far plane and the sides are
  handled by Cohen-Sutherland in 2D after projection, which is correct for
  lines.
- `fillPlanPolygon` skips a whole polygon if any vertex is behind the near
  plane (it does not clip polygons in 3D). In the demo the camera is always
  outside the room, so this never happens.
- `windowToViewport` returns the identity for a window of zero width or
  height instead of dividing by zero.
