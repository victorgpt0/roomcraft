# Part 2 — Hierarchy and 3D geometry

This part turns a flat list of furniture into a **scene graph**: a tree of
parts whose transforms are relative to their parent. It also lifts the 2D
floor plan into 3D by extruding footprints into prisms, and adds a small
oblique projection so the result can be drawn before the Part 3 camera exists.

The implementation lives in `src/scene/node.*` and `src/scene/extrude.*`,
with tests in `tests/test_scene.cpp` and the animations produced by
`demo/cp2_scene_demo.cpp`.

---

## 1. Hierarchical transforms and the matrix stack

### What technique or algorithm was used?

Each node stores its shape in **local coordinates** and a `Pose` that is
relative to its **parent**. Its matrix in the room is the product of every
pose from the root down to that node:

```text
world = rootPose * ... * parentPose * nodePose
```

Because points are column vectors (the same convention as Part 1), the
right-most matrix is applied first. A child is therefore placed in the
parent's frame, then the parent is placed in the grandparent's frame, and so
on. This is the same hierarchical transform used by game engines, Blender's
outliner and skeletal animation.

Drawing the tree is a **matrix stack**. The recursive call pushes the node's
world matrix (the new top of the stack) and, when it returns, the parent
matrix is still unchanged (the pop). In this code the push/pop is done without
mutating anything:

```cpp
const xform::Mat3 world = parent * xform::poseMatrix(node.pose);
for (const Node& child : node.children) {
    drawNode(canvas, child, world);   // push
}
// `world` was a local value; `parent` is untouched here: pop
```

### How was it implemented?

- `src/scene/node.h` declares `scene::Node` and `drawNode`.
- `src/scene/node.cpp`, `scene::drawNode`, computes
  `world = parent * poseMatrix(node.pose)`, transforms the local `shape` with
  that matrix, fills it with the CP1 scanline fill (`raster::fillPolygonScanline`),
  outlines it with `raster::drawPolygonOutline`, then recurses into `children`.
- The same file contains `scene::worldMatrix(root, name, found)`, which walks
  the tree and returns the full world matrix of a named node. A missing name
  sets `*found = false` and returns the identity.
- A node with an empty `shape` is a pure group: it still contributes its pose
  to its children.

### Which project component uses it?

The `scene` library. The demo uses it directly for the fan, swivel chair and
door. Later parts (the showcase) can use the same nodes for articulated
furniture and can use `worldMatrix` to find where a part ended up without
walking the graph again.

### What does it achieve in the project?

It removes duplicated animation work. The fan hub spins once and all four
blades follow. The chair seat swivels and its backrest and armrests follow,
while the base does not. The room moves and its door, walls, rug and cabinet
all follow the room root. The tests also prove that changing one child cannot
move its parent or its siblings.

### Which demo image or GIF shows the result?

- `docs/images/cp2/scene_fan.gif` — hub spins, four blade children follow,
  whole fan sways.
- `docs/images/cp2/scene_door.gif` — door and room contents follow the moving
  room root.

---

## 2. The scene graph: room, furniture, parts

### What technique or algorithm was used?

A **tree** of nodes. Each edge means “this part is attached to that one”.
The root is the whole object (a fan, a chair, or the room); internal nodes are
groups or moving assemblies (hub, seat); leaves are shapes (blades, armrests,
door leaf). Sibling order is draw order, so later children appear on top.

### How was it implemented?

`scene::Node` has `name`, `shape`, `color`, `pose` and
`std::vector<Node> children`. The demo builds the tree fresh on every frame:
it changes only the poses at the levels that are moving and leaves the local
shapes untouched. For example:

```text
fan (sway)
└── hub (spin)
    ├── blade 1
    ├── blade 2
    ├── blade 3
    ├── blade 4
    └── cap
```

and

```text
chair (rolls across the room)
├── base (stays fixed relative to the chair)
│   └── base marker
└── seat (swivels)
    ├── backrest
    ├── left armrest
    └── right armrest
```

### Which project component uses it?

`scene::Node` / `scene::drawNode`, exercised by `demo/cp2_scene_demo.cpp`.
The tests build two- and three-level models and compare the child world
matrices with hand-computed matrices.

### What does it achieve in the project?

It makes articulated furniture possible with a small amount of code and no
special-case animation logic. It also makes the relationship between the room
and its contents explicit: the door is a child of the room, not a shape that
happens to be near it.

### Which demo image or GIF shows the result?

`scene_chair.gif` in `demo_output/cp2/` (the chair is left out of the committed
images because the fan and door show the hierarchy more clearly) and
`docs/images/cp2/scene_door.gif`.

---

## 3. Extrusion and the 2D-to-3D coordinate mapping

### What technique or algorithm was used?

Each floor-plan vertex is duplicated at two heights. With the shared mapping

```text
plan point (x, y) at height h  ->  world point (X = x, Y = h, Z = y)
```

the bottom ring is at `Y = 0`, the top ring is at `Y = height`, and vertical
edges join matching vertices. The plan's `y` becomes world `Z` (depth), while
world `Y` is height and points up. Units stay in centimetres.

For a footprint with `n` vertices this gives:

- `2n` vertices (bottom ring plus top ring),
- `3n` edges (bottom ring, top ring and `n` uprights).

### How was it implemented?

- `src/scene/extrude.cpp`, `scene::extrude`: copies the vertices twice, then
  emits the three edge families. It does not modify the input polygon.
- `scene::extrudedRoom`: reuses `extrude` for the room outline; the bottom ring
  is the floor perimeter, the uprights are wall corners, and the top ring joins
  the wall tops.
- `demo/cp2_scene_demo.cpp` keeps a height per piece (bed 55 cm, wardrobe
  200 cm, sofa 80 cm, table 75 cm, plant 120 cm, and so on) and rebuilds each
  prism with an animated height.

### Which project component uses it?

The `scene` library, and any later part that needs 3D furniture. The Part 3
camera and the final showcase can use the same `WireMesh` directly.

### What does it achieve in the project?

It turns the existing 2D footprints into a 3D preview with no separate model
format: the floor plan is the only source of truth. It also shows why the
coordinate mapping matters: the plan stays in its editor coordinates, and only
the height axis is added.

### Which demo image or GIF shows the result?

`docs/images/cp2/scene_room_3d.gif` — the room and furniture rise from flat
footprints to full-height wireframe prisms.

---

## 4. Oblique (cabinet) projection

### What technique or algorithm was used?

A 3D point is projected with

```text
screen x = X + 0.5 * Z * cos(45 degrees) + offset.x
screen y = -Y + 0.5 * Z * sin(45 degrees) + offset.y
```

The `-Y` flips world-up into screen-down (the rasterizer's y grows downward).
The `Z` terms fold depth into the picture. This is a **shear**: points slide by
an amount proportional to `Z`, so parallel lines stay parallel and distances
along X and Y are unchanged. There is no vanishing point, which is exactly
what a cabinet/cavalier drawing does. The factor `0.5` makes the depth look
half-size, keeping the drawing readable.

### How was it implemented?

- `src/scene/extrude.cpp`, `scene::obliqueProject`.
- `demo/cp2_scene_demo.cpp`, `drawMesh`, projects every edge of a `WireMesh`
  and draws it with the CP1 Bresenham line routine.
- The room is drawn as a filled projected floor polygon, a floor grid, wall
  uprights and furniture prisms. The canvas size and offset were chosen so the
  projected room (which becomes wider because depth is sheared sideways)
  fits comfortably.

### Which project component uses it?

The Part 2 demo only. It is deliberately a temporary stand-in for the real
camera and projection that Part 3 provides.

### What does it achieve in the project?

It makes the 3D data visible immediately, without waiting for Part 3, and it
shows the 2D-to-3D mapping in a way that is easy to explain on camera.

### Which demo image or GIF shows the result?

`docs/images/cp2/scene_room_3d.gif`.

---

## Tests

`tests/test_scene.cpp` contains 259 checks covering:

- two-level and three-level world matrices against hand-computed points;
- a child moving on a circle of constant radius while its parent rotates;
- a changed child leaving its parent and sibling matrices unchanged;
- `drawNode` filling a child's shape at the transformed position;
- lookup by name, including the missing-name case;
- extrusion counts (`2n` vertices, `3n` edges), the two rings, the X/Z mapping,
  and all three edge families;
- the room helper at `Y = 0` and `Y = wallHeight`;
- the oblique projection's known points and its equal X/Y shear for a pure Z
  step.

Run it with:

```sh
ctest --test-dir build -R test_scene --output-on-failure
```

---

## Challenges

- **Rotation signs with y down.** The plan uses screen coordinates, so a
  positive angle is clockwise. Hand-computing the three-level test initially
  made it easy to flip a sign. The fix was to write the four cardinal angles
  out explicitly in the test (0, 90, 180, 270) as well as checking the general
  circle equation.
- **Moving the room without confusing the hinge.** The door hinge must be in
  room-local coordinates, but the room should rotate about its centre rather
  than its top-left corner. The room was re-expressed with local coordinates
  centred on `(0, 0)`, and the door hinge moved to the corresponding local
  point `(120, 200)`.
- **Fitting the oblique room on the canvas.** Cabinet projection shears depth
  sideways, so the projected room is much wider than the plan. The offset and
  canvas size were chosen from the projected bounds (including the wall
  height) rather than guessing.
- **Keeping the committed GIFs under 2 MB.** Frame counts and canvas sizes
  were reduced where needed after measuring the actual file sizes; the three
  committed images are all below 1.2 MB.
- **Room helper semantics.** The room helper needed to express both the floor
  outline and the walls. It was implemented as a thin, documented wrapper over
  `extrude`: the bottom ring is the floor perimeter, the uprights are the wall
  corners and the top ring outlines the wall tops, so callers get one mesh
  without a second geometry format.
