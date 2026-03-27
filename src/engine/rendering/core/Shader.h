#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    // Non-copyable
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setVec2(const std::string& name, const glm::vec2& v) const;
    void setFloat(const std::string& name, float v) const;
    void setInt(const std::string& name, int v) const;

    GLuint id() const { return program_; }

private:
    GLuint program_ = 0;

    static GLuint compileShader(GLenum type, const std::string& source, const std::string& path);
    static std::string readFile(const std::string& path);
};
