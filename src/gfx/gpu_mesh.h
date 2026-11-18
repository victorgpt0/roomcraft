#pragma once

#include "editor/mesh.h"
#include "gfx/gl.h"

// A mesh stored in GPU memory, ready to draw.
//
//   VBO (vertex buffer object)  the editor::Vertex array: position, normal, colour
//   EBO (element buffer object) the indices: 3 per triangle or 2 per line
//   VAO (vertex array object)   remembers how the VBO's bytes map to the
//                               shader inputs aPos, aNormal and aColor
//
// The data is built on the CPU (editor::MeshData) and uploaded once per
// change; drawing it is then a single call.

namespace gfx {

class GpuMesh {
public:
    GpuMesh() = default;
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    GpuMesh(GpuMesh&& other) noexcept;
    GpuMesh& operator=(GpuMesh&& other) noexcept;

    // Replaces the contents. primitive is GL_TRIANGLES or GL_LINES.
    void upload(const editor::MeshData& data, GLenum primitive);
    void draw() const;
    bool empty() const { return count_ == 0; }

private:
    void release();

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei count_ = 0;
    GLenum primitive_ = GL_TRIANGLES;
};

} // namespace gfx
