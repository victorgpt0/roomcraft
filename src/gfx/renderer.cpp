#include "gfx/renderer.h"

#include "editor/catalogue.h"
#include "editor/layout.h"
#include "editor/mesh.h"

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

// Vertex shader: places each vertex. uModel stretches heights during the
// 2D-to-3D transition (or flattens a shadow onto the floor); uViewProj is
// projection * view from the camera rig - our own view::Mat4.
const char* kVertexShader = R"(#version 330 core
in vec3 aPos;
in vec3 aNormal;
in vec3 aColor;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vPos = world.xyz;
    vNormal = aNormal; // uModel only scales Y; vertical and horizontal faces keep their directions
    vColor = aColor;
    gl_Position = uViewProj * world;
}
)";

// Fragment shader: Blinn-Phong lighting.
//   colour = base * (ambient + sum of diffuse) + sum of specular
//   diffuse  = lightColour * max(N . L, 0)
//   specular = lightColour * max(N . H, 0)^shininess * strength, H = normalize(L + V)
// The ceiling lamp is a point light whose strength falls off with distance
// (attenuation 1 / (1 + k1 d + k2 d^2)). uLit blends from the flat plan
// colours (0) to the lit result (1). uMode 1 draws a flat colour (shadows).
const char* kFragmentShader = R"(#version 330 core
in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;
out vec4 fragColor;
uniform int uMode;
uniform vec4 uFlatColor;
uniform float uLit;
uniform vec3 uEye;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbient;
uniform int uLampOn;
uniform vec3 uLampPos;
uniform vec3 uLampColor;
uniform float uShininess;
uniform float uSpecular;
uniform vec4 uTint;
void main() {
    if (uMode == 1) {
        fragColor = uFlatColor;
        return;
    }
    vec3 base = mix(vColor, uTint.rgb, uTint.a);
    if (uLit <= 0.0) {
        fragColor = vec4(base, 1.0);
        return;
    }
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uEye - vPos);
    if (dot(N, V) < 0.0) {
        N = -N; // seen from behind (culling off): light the side we see
    }
    vec3 diffuse = uAmbient;
    vec3 specular = vec3(0.0);

    float d = max(dot(N, uSunDir), 0.0);
    diffuse += uSunColor * d;
    if (d > 0.0) {
        vec3 H = normalize(uSunDir + V);
        specular += uSunColor * pow(max(dot(N, H), 0.0), uShininess) * uSpecular;
    }

    if (uLampOn == 1) {
        vec3 toLamp = uLampPos - vPos;
        float dist = length(toLamp);
        vec3 L = toLamp / dist;
        float attenuation = 1.0 / (1.0 + 0.0009 * dist + 0.0000035 * dist * dist);
        float dl = max(dot(N, L), 0.0);
        diffuse += uLampColor * dl * attenuation;
        if (dl > 0.0) {
            vec3 H = normalize(L + V);
            specular += uLampColor * attenuation * pow(max(dot(N, H), 0.0), uShininess) * uSpecular;
        }
    }
    vec3 lit = base * diffuse + specular;
    fragColor = vec4(mix(base, lit, uLit), 1.0);
}
)";

const raster::Color kOutline{45, 45, 50};
const raster::Color kSelect{255, 140, 0};
const raster::Color kGridColor{180, 160, 130};

} // namespace

view::Mat4 shadowMatrix(xform::Vec3 l, float height) {
    // x' = x - l.x * (y - h) / l.y,  y' = h,  z' = z - l.z * (y - h) / l.y
    const float ly = std::max(l.y, 0.05f);
    view::Mat4 m = view::Mat4::identity();
    m.m[0][1] = -l.x / ly;
    m.m[0][3] = l.x * height / ly;
    m.m[1][1] = 0.0f;
    m.m[1][3] = height;
    m.m[2][1] = -l.z / ly;
    m.m[2][3] = l.z * height / ly;
    return m;
}

bool wallFacesCamera(xform::Vec3 n, xform::Vec3 centre, xform::Vec3 eye) {
    return (eye.x - centre.x) * n.x + (eye.z - centre.z) * n.z > 0.0f;
}

bool Renderer::init(std::string& error) {
    return shader_.build(kVertexShader, kFragmentShader, error);
}

void Renderer::rebuild(const editor::Editor& editor) {
    const editor::Layout& layout = editor.layout();
    const editor::Room& room = layout.room;

    editor::MeshData data;
    editor::addFloor(data, room);
    floor_.upload(data, GL_TRIANGLES);

    data.clear();
    editor::addGrid(data, room, 50.0f, 0.3f, kGridColor);
    grid_.upload(data, GL_LINES);

    data.clear();
    editor::addDoorSwing(data, room, 0.6f, {120, 120, 120});
    doorSwing_.upload(data, GL_LINES);

    walls_.clear();
    const std::vector<editor::Opening> openings = editor::openings(room);
    for (const editor::WallMesh& wall : editor::buildWalls(room)) {
        WallMeshes meshes;
        meshes.full.upload(wall.full, GL_TRIANGLES);
        meshes.cut.upload(wall.cut, GL_TRIANGLES);
        editor::MeshData opening;
        for (const editor::Opening& o : openings) {
            if (o.wall == wall.wall) {
                editor::addOpeningModel(opening, room, o);
            }
        }
        meshes.opening.upload(opening, GL_TRIANGLES);
        meshes.normal = wall.outwardNormal;
        meshes.centre = wall.centre;
        walls_.push_back(std::move(meshes));
    }

    items_.clear();
    for (const editor::Item& item : layout.items) {
        ItemMeshes meshes;
        meshes.id = item.id;
        data.clear();
        editor::addItemModel(data, item);
        meshes.model.upload(data, GL_TRIANGLES);
        data.clear();
        editor::addItemFlat(data, item, 0.0f);
        meshes.flat.upload(data, GL_TRIANGLES);
        data.clear();
        editor::addPolygonOutline(data, editor::itemFootprint(item), 0.0f, kOutline);
        meshes.outline.upload(data, GL_LINES);
        data.clear();
        editor::addItemEdges(data, item, kSelect);
        meshes.edges.upload(data, GL_LINES);
        meshes.height = editor::typeOf(item).height * item.heightScale;
        items_.push_back(std::move(meshes));
    }
    builtRevision_ = editor.revision();
}

// Colour feedback, mixed into the piece's own colours:
//   red    - sticks through a wall      amber - collides with another piece
//   orange - selected                   white - under the mouse
void Renderer::setTint(const editor::Item& item, const editor::ItemStatus& status, bool selected, bool hovered,
                       float plan) {
    (void)item;
    float r = 0, g = 0, b = 0, a = 0;
    if (!status.fitsInRoom) {
        r = 0.9f, g = 0.1f, b = 0.1f, a = 0.55f;
    } else if (status.overlaps) {
        r = 1.0f, g = 0.7f, b = 0.0f, a = 0.5f;
    } else if (selected) {
        r = 1.0f, g = 0.55f, b = 0.0f, a = 0.25f + 0.1f * plan;
    } else if (hovered) {
        r = 1.0f, g = 1.0f, b = 1.0f, a = 0.25f;
    }
    shader_.set("uTint", r, g, b, a);
}

void Renderer::render(const editor::Editor& editor, const editor::CameraState& camera, const RenderOptions& options,
                      const Lighting& lighting, int hoverId, const Viewport& viewport) {
    if (builtRevision_ != editor.revision()) {
        rebuild(editor);
    }
    const editor::Layout& layout = editor.layout();
    const std::vector<editor::ItemStatus>& status = editor.status();
    const float e = camera.blend;     // 0 = plan, 1 = 3D
    const bool plan = camera.orthographic;

    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glClearColor(options.background.x, options.background.y, options.background.z, 1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    if (options.backFaceCulling) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }

    shader_.use();
    shader_.set("uViewProj", camera.viewProjection);
    shader_.set("uEye", camera.eye);
    shader_.set("uSunDir", lighting.sunDirection);
    shader_.set("uSunColor", lighting.sunColor);
    shader_.set("uAmbient", lighting.ambient);
    shader_.set("uLampOn", lighting.lampOn ? 1 : 0);
    shader_.set("uLampPos", xform::Vec3{layout.room.width / 2.0f, layout.room.wallHeight * e - 15.0f, layout.room.depth / 2.0f});
    shader_.set("uLampColor", lighting.lampColor);
    shader_.set("uShininess", lighting.shininess);
    shader_.set("uSpecular", lighting.specular);
    shader_.set("uMode", 0);
    shader_.set("uLit", e);
    shader_.set("uTint", 0.0f, 0.0f, 0.0f, 0.0f);
    const view::Mat4 grow = view::Mat4::scaling(1.0f, std::max(e, 0.002f), 1.0f);
    shader_.set("uModel", view::Mat4::identity());

    // Fills are pushed slightly back in depth, so outlines drawn exactly on
    // their edges are not hidden by them (no "z-fighting").
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    // ---- Floor (also marks the floor in the stencil buffer for shadows) ----
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    floor_.draw();
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDisable(GL_STENCIL_TEST);

    if (options.grid) {
        shader_.set("uLit", 0.0f);
        grid_.draw();
        shader_.set("uLit", e);
    }

    if (options.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (plan) {
        // ---- Plan: flat footprints in list order, each a little higher so
        // later pieces cover earlier ones, as in the CP1 painter's order. ----
        shader_.set("uLit", 0.0f);
        for (std::size_t i = 0; i < items_.size() && i < layout.items.size(); ++i) {
            const editor::Item& item = layout.items[i];
            shader_.set("uModel", view::Mat4::translation(0.0f, 1.0f + 0.05f * static_cast<float>(i), 0.0f));
            setTint(item, status[i], item.id == editor.selectedId(), item.id == hoverId, 1.0f);
            items_[i].flat.draw();
            shader_.set("uTint", 0.0f, 0.0f, 0.0f, 0.0f);
            items_[i].outline.draw();
        }
        // Walls from above: dark, as on a drawn plan.
        shader_.set("uModel", view::Mat4::identity());
        shader_.set("uTint", 0.24f, 0.24f, 0.27f, 0.9f);
        wallsCut_.assign(walls_.size(), false);
        for (WallMeshes& wall : walls_) {
            wall.full.draw();
        }
        shader_.set("uTint", 0.0f, 0.0f, 0.0f, 0.0f);
        for (WallMeshes& wall : walls_) {
            wall.opening.draw();
        }
        doorSwing_.draw();
    } else {
        // ---- 3D (and the transition): walls, openings, models. ----
        shader_.set("uModel", grow);
        wallsCut_.assign(walls_.size(), false);
        for (std::size_t w = 0; w < walls_.size(); ++w) {
            WallMeshes& wall = walls_[w];
            const bool cut = options.cutawayWalls && e > 0.3f && wallFacesCamera(wall.normal, wall.centre, camera.eye);
            wallsCut_[w] = cut;
            if (cut) {
                wall.cut.draw();
            } else {
                wall.full.draw();
                wall.opening.draw();
            }
        }
        for (std::size_t i = 0; i < items_.size() && i < layout.items.size(); ++i) {
            const editor::Item& item = layout.items[i];
            setTint(item, status[i], item.id == editor.selectedId(), item.id == hoverId, 0.0f);
            items_[i].model.draw();
        }
        shader_.set("uTint", 0.0f, 0.0f, 0.0f, 0.0f);

        // ---- Shadows: every model flattened onto the floor along the sun
        // direction, drawn translucent where the stencil says "floor" (1),
        // and each pixel only once (the stencil is bumped to 2), so
        // overlapping shadows do not get darker. ----
        if (options.shadows && lighting.sunDirection.y > 0.05f && !options.wireframe) {
            glPolygonOffset(-1.0f, -1.0f); // shadows win over the floor they lie on
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_EQUAL, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE); // a flattened model has no meaningful front
            shader_.set("uMode", 1);
            const float strength = std::clamp(lighting.sunColor.x + lighting.sunColor.y + lighting.sunColor.z, 0.0f, 3.0f) / 3.0f;
            shader_.set("uFlatColor", 0.05f, 0.04f, 0.03f, 0.32f * e * strength);
            shader_.set("uModel", shadowMatrix(lighting.sunDirection, 0.05f) * grow);
            for (std::size_t i = 0; i < items_.size() && i < layout.items.size(); ++i) {
                if (!editor::typeOf(layout.items[i]).floorLevel) {
                    items_[i].model.draw();
                }
            }
            shader_.set("uMode", 0);
            glDepthMask(GL_TRUE);
            glDisable(GL_STENCIL_TEST);
            glDisable(GL_BLEND);
            glPolygonOffset(1.0f, 1.0f);
            if (options.backFaceCulling) {
                glEnable(GL_CULL_FACE);
            }
        }
    }

    if (options.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);

    // ---- Selection outline: on top in the plan, depth-tested in 3D (so it
    // is hidden behind other furniture, like the piece itself). ----
    const int selected = editor.indexOf(editor.selectedId());
    if (selected >= 0 && static_cast<std::size_t>(selected) < items_.size()) {
        shader_.set("uLit", 0.0f);
        shader_.set("uTint", 0.0f, 0.0f, 0.0f, 0.0f);
        if (plan) {
            glDisable(GL_DEPTH_TEST);
            shader_.set("uMode", 1);
            shader_.set("uFlatColor", 1.0f, 0.55f, 0.0f, 1.0f);
            // Three slightly offset copies make the outline thicker than the
            // 1-pixel lines core OpenGL guarantees.
            const float px = 2.0f * camera.halfHeight / static_cast<float>(viewport.height);
            for (const float d : {-px, 0.0f, px}) {
                shader_.set("uModel", view::Mat4::translation(d, 50.0f, 0.0f));
                items_[static_cast<std::size_t>(selected)].outline.draw();
                shader_.set("uModel", view::Mat4::translation(0.0f, 50.0f, d));
                items_[static_cast<std::size_t>(selected)].outline.draw();
            }
            shader_.set("uMode", 0);
            glEnable(GL_DEPTH_TEST);
        } else {
            shader_.set("uModel", grow);
            items_[static_cast<std::size_t>(selected)].edges.draw();
        }
    }

    shader_.set("uModel", view::Mat4::identity());
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(0);
}

} // namespace gfx
