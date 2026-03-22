#include "Mesh.h"

#include <utility>

Mesh::Mesh(const std::vector<glm::vec3>& positions,
           const std::vector<glm::vec3>& normals,
           const std::vector<uint32_t>& indices) {
    indexCount_ = static_cast<GLsizei>(indices.size());

    // Interleave: position (vec3) + normal (vec3) = 6 floats per vertex
    std::vector<float> vertData;
    vertData.reserve(positions.size() * 6);
    for (size_t i = 0; i < positions.size(); ++i) {
        vertData.push_back(positions[i].x);
        vertData.push_back(positions[i].y);
        vertData.push_back(positions[i].z);
        if (i < normals.size()) {
            vertData.push_back(normals[i].x);
            vertData.push_back(normals[i].y);
            vertData.push_back(normals[i].z);
        } else {
            vertData.push_back(0.0f);
            vertData.push_back(1.0f);
            vertData.push_back(0.0f);
        }
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertData.size() * sizeof(float)),
                 vertData.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    // location 0: position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // location 1: normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Mesh::~Mesh() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
}

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(other.vao_), vbo_(other.vbo_), ebo_(other.ebo_),
      indexCount_(other.indexCount_) {
    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.indexCount_ = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (ebo_) glDeleteBuffers(1, &ebo_);
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        ebo_ = other.ebo_;
        indexCount_ = other.indexCount_;
        other.vao_ = 0;
        other.vbo_ = 0;
        other.ebo_ = 0;
        other.indexCount_ = 0;
    }
    return *this;
}

void Mesh::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

Mesh Mesh::createCube(float size) {
    float h = size * 0.5f;

    // 8 unique positions, 6 faces * 4 verts with normals
    std::vector<glm::vec3> positions = {
        // Front face (+Z)
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h},
        // Back face (-Z)
        { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h},
        // Left face (-X)
        {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h},
        // Right face (+X)
        { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h},
        // Top face (+Y)
        {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h},
        // Bottom face (-Y)
        {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h},
    };

    std::vector<glm::vec3> normals = {
        // Front
        {0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1},
        // Back
        {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1},
        // Left
        {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0},
        // Right
        {1, 0, 0}, {1, 0, 0}, {1, 0, 0}, {1, 0, 0},
        // Top
        {0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0},
        // Bottom
        {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0},
    };

    std::vector<uint32_t> indices;
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return Mesh(positions, normals, indices);
}

Mesh Mesh::createPlane(float size) {
    float h = size * 0.5f;

    std::vector<glm::vec3> positions = {
        {-h, 0.0f, -h},
        { h, 0.0f, -h},
        { h, 0.0f,  h},
        {-h, 0.0f,  h},
    };

    std::vector<glm::vec3> normals = {
        {0, 1, 0},
        {0, 1, 0},
        {0, 1, 0},
        {0, 1, 0},
    };

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    return Mesh(positions, normals, indices);
}
