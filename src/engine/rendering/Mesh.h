#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

class Mesh {
public:
    Mesh(const std::vector<glm::vec3>& positions,
         const std::vector<glm::vec3>& normals,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    // Non-copyable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Movable
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;

    static Mesh createCube(float size);
    static Mesh createPlane(float size);
    static Mesh createCylinder(float radius, float height, int segments);

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
};
