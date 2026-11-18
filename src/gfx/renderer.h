#pragma once

#include "editor/camera_rig.h"
#include "editor/editor.h"
#include "gfx/gpu_mesh.h"
#include "gfx/shader.h"
#include "xform/vec.h"

#include <string>
#include <vector>

// Draws the room with OpenGL.
//
// One renderer, two looks, and everything in between:
//   - plan (camera blend 0): flat colours, footprints, outlines, the grid and
//     the door swing - the floor plan of Checkpoints 1 and 2, now drawn by
//     the GPU;
//   - 3D (blend 1): every piece as its multi-part model, lit by a sun and an
//     optional ceiling lamp (Blinn-Phong), with shadows on the floor.
// During the transition the models are drawn with their heights multiplied
// by the blend, so the furniture grows out of the plan, and the lighting
// fades in by the same amount.
//
// GPU meshes are rebuilt only when the editor's revision changes.

namespace gfx {

struct Lighting {
    xform::Vec3 sunDirection{0.35f, 0.8f, -0.5f}; // unit, pointing TOWARDS the sun
    xform::Vec3 sunColor{0.75f, 0.72f, 0.66f};
    xform::Vec3 ambient{0.38f, 0.40f, 0.44f};
    bool lampOn = false;
    xform::Vec3 lampColor{0.95f, 0.80f, 0.55f};
    float shininess = 28.0f;
    float specular = 0.18f;
};

struct RenderOptions {
    bool grid = true;
    float gridSpacing = 50.0f;
    bool cutawayWalls = true;   // lower the walls between the camera and the room
    bool shadows = true;
    bool wireframe = false;
    bool backFaceCulling = true;
    xform::Vec3 background{0.86f, 0.88f, 0.91f};
};

// Pixel rectangle of the framebuffer to draw into (OpenGL convention: x, y
// is the BOTTOM-left corner).
struct Viewport {
    int x = 0, y = 0, width = 1, height = 1;
};

class Renderer {
public:
    // Compiles the shaders. Needs a current OpenGL 3.3 context.
    bool init(std::string& error);

    // Draws one frame. hoverId: the piece under the mouse (-1 = none).
    void render(const editor::Editor& editor, const editor::CameraState& camera, const RenderOptions& options,
                const Lighting& lighting, int hoverId, const Viewport& viewport);

    // Which walls the last frame showed cut down (for tests of the cutaway
    // rule and for the UI).
    const std::vector<bool>& wallsCut() const { return wallsCut_; }

private:
    struct ItemMeshes {
        int id = 0;
        GpuMesh model;     // 3D parts
        GpuMesh flat;      // plan footprint
        GpuMesh outline;   // plan outline
        GpuMesh edges;     // 3D selection outline
        float height = 0;  // top of the model, for labels and the lamp
    };
    struct WallMeshes {
        GpuMesh full, cut, opening;
        xform::Vec3 normal, centre;
    };

    void rebuild(const editor::Editor& editor);
    void setTint(const editor::Item& item, const editor::ItemStatus& status, bool selected, bool hovered, float plan);

    Shader shader_;
    unsigned builtRevision_ = 0;
    std::vector<ItemMeshes> items_;
    std::vector<WallMeshes> walls_;
    GpuMesh floor_, grid_, doorSwing_;
    std::vector<bool> wallsCut_;
};

// Planar projected shadow: the matrix that flattens geometry onto the plane
// Y = height along the light direction (pointing towards the light).
// A point P goes to P - L * (P.y - height) / L.y.
view::Mat4 shadowMatrix(xform::Vec3 towardsLight, float height);

// Should a wall be drawn cut down? Yes when the cutaway is on and the camera
// is outside the room on that wall's side: the wall's outward normal points
// towards the eye.
bool wallFacesCamera(xform::Vec3 outwardNormal, xform::Vec3 wallCentre, xform::Vec3 eye);

} // namespace gfx
