#pragma once

#include "gfx/gl.h"
#include "view/mat4.h"
#include "xform/vec.h"

#include <string>
#include <unordered_map>

// A GLSL shader program: a vertex shader (runs once per vertex, places it on
// the screen) and a fragment shader (runs once per pixel, computes its
// colour), compiled and linked by the graphics driver.

namespace gfx {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Compiles and links. On failure returns false and puts the driver's
    // error log in `error`.
    bool build(const char* vertexSource, const char* fragmentSource, std::string& error);

    void use() const;
    GLuint id() const { return program_; }

    void set(const char* name, float value);
    void set(const char* name, int value);
    void set(const char* name, xform::Vec3 value);
    void set(const char* name, float x, float y, float z, float w);
    // Our matrices are stored m[row][column] (row-major); OpenGL expects
    // column-major, so they are uploaded with transpose = GL_TRUE.
    void set(const char* name, const view::Mat4& value);

private:
    GLint location(const char* name);

    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> locations_;
};

} // namespace gfx
