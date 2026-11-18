#include "gfx/shader.h"

#include <vector>

namespace gfx {

namespace {

GLuint compile(GLenum type, const char* source, std::string& error) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") + " shader: " + log.data();
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

Shader::~Shader() {
    if (program_ != 0) {
        glDeleteProgram(program_);
    }
}

bool Shader::build(const char* vertexSource, const char* fragmentSource, std::string& error) {
    const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource, error);
    if (vertex == 0) {
        return false;
    }
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    // Fixed attribute slots, matching the vertex layout in gpu_mesh.cpp.
    glBindAttribLocation(program_, 0, "aPos");
    glBindAttribLocation(program_, 1, "aNormal");
    glBindAttribLocation(program_, 2, "aColor");
    glLinkProgram(program_);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint ok = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 1 ? length : 1), '\0');
        glGetProgramInfoLog(program_, length, nullptr, log.data());
        error = std::string("link: ") + log.data();
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    return true;
}

void Shader::use() const {
    glUseProgram(program_);
}

GLint Shader::location(const char* name) {
    const auto found = locations_.find(name);
    if (found != locations_.end()) {
        return found->second;
    }
    const GLint where = glGetUniformLocation(program_, name);
    locations_[name] = where;
    return where;
}

void Shader::set(const char* name, float value) {
    glUniform1f(location(name), value);
}

void Shader::set(const char* name, int value) {
    glUniform1i(location(name), value);
}

void Shader::set(const char* name, xform::Vec3 value) {
    glUniform3f(location(name), value.x, value.y, value.z);
}

void Shader::set(const char* name, float x, float y, float z, float w) {
    glUniform4f(location(name), x, y, z, w);
}

void Shader::set(const char* name, const view::Mat4& value) {
    glUniformMatrix4fv(location(name), 1, GL_TRUE, &value.m[0][0]);
}

} // namespace gfx
