#pragma once

#include "GL33Loader.h"
#include "core/util/Log.h"

#include <string>
#include <stdexcept>

namespace godsim {

/// Compiles and links an OpenGL shader program from source strings.
class ShaderProgram {
public:
    ShaderProgram() = default;

    /// Compile vertex + fragment shaders and link into a program.
    void compile(const std::string& vert_src, const std::string& frag_src) {
        GLuint vert = compile_stage(GL_VERTEX_SHADER, vert_src, "vertex");
        GLuint frag = compile_stage(GL_FRAGMENT_SHADER, frag_src, "fragment");

        program_ = gl::CreateProgram();
        gl::AttachShader(program_, vert);
        gl::AttachShader(program_, frag);
        gl::LinkProgram(program_);

        GLint success;
        gl::GetProgramiv(program_, GL_LINK_STATUS, &success);
        if (!success) {
            char log[512];
            gl::GetProgramInfoLog(program_, 512, nullptr, log);
            gl::DeleteShader(vert);
            gl::DeleteShader(frag);
            throw std::runtime_error(std::string("Shader link failed: ") + log);
        }

        gl::DeleteShader(vert);
        gl::DeleteShader(frag);
    }

    void use() const { gl::UseProgram(program_); }

    GLint uniform(const char* name) const {
        return gl::GetUniformLocation(program_, name);
    }

    void set_mat4(const char* name, const float* data) const {
        gl::UniformMatrix4fv(uniform(name), 1, GL_FALSE, data);
    }

    void set_vec3(const char* name, float x, float y, float z) const {
        gl::Uniform3f(uniform(name), x, y, z);
    }

    void set_float(const char* name, float v) const {
        gl::Uniform1f(uniform(name), v);
    }

    void set_int(const char* name, int v) const {
        gl::Uniform1i(uniform(name), v);
    }

    GLuint id() const { return program_; }

    ~ShaderProgram() {
        if (program_) gl::DeleteProgram(program_);
    }

    // Move only
    ShaderProgram(ShaderProgram&& o) noexcept : program_(o.program_) { o.program_ = 0; }
    ShaderProgram& operator=(ShaderProgram&& o) noexcept {
        if (program_) gl::DeleteProgram(program_);
        program_ = o.program_; o.program_ = 0;
        return *this;
    }
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

private:
    GLuint compile_stage(GLenum type, const std::string& source, const char* label) {
        GLuint shader = gl::CreateShader(type);
        const char* src = source.c_str();
        gl::ShaderSource(shader, 1, &src, nullptr);
        gl::CompileShader(shader);

        GLint success;
        gl::GetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            gl::GetShaderInfoLog(shader, 512, nullptr, log);
            gl::DeleteShader(shader);
            throw std::runtime_error(
                std::string("Shader compile (") + label + ") failed: " + log);
        }

        return shader;
    }

    GLuint program_ = 0;
};

} // namespace godsim
