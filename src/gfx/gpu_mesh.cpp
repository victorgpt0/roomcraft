#include "gfx/gpu_mesh.h"

#include <cstddef>
#include <utility>

namespace gfx {

GpuMesh::~GpuMesh() {
    release();
}

GpuMesh::GpuMesh(GpuMesh&& other) noexcept {
    *this = std::move(other);
}

GpuMesh& GpuMesh::operator=(GpuMesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
        ebo_ = std::exchange(other.ebo_, 0);
        count_ = std::exchange(other.count_, 0);
        primitive_ = other.primitive_;
    }
    return *this;
}

void GpuMesh::release() {
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
    }
    vao_ = vbo_ = ebo_ = 0;
    count_ = 0;
}

void GpuMesh::upload(const editor::MeshData& data, GLenum primitive) {
    primitive_ = primitive;
    count_ = static_cast<GLsizei>(data.indices.size());
    if (count_ == 0) {
        return;
    }
    if (vao_ == 0) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
    }
    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(editor::Vertex)),
                 data.vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(uint32_t)),
                 data.indices.data(), GL_DYNAMIC_DRAW);

    // Each vertex is 9 floats: position (3), normal (3), colour (3).
    const GLsizei stride = static_cast<GLsizei>(sizeof(editor::Vertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(editor::Vertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(editor::Vertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(editor::Vertex, r)));

    glBindVertexArray(0);
}

void GpuMesh::draw() const {
    if (count_ == 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawElements(primitive_, count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace gfx
