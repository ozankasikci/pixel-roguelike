#include "Mesh.h"
#include "MeshGeometry.h"

#include <utility>

Mesh::Mesh(const std::vector<glm::vec3>& positions,
           const std::vector<glm::vec3>& normals,
           const std::vector<glm::vec2>& uvs,
           const std::vector<glm::vec3>& tangents,
           const std::vector<uint32_t>& indices) {
    indexCount_ = static_cast<GLsizei>(indices.size());

    if (!positions.empty()) {
        aabbMin_ = positions.front();
        aabbMax_ = positions.front();
        for (const auto& position : positions) {
            aabbMin_ = glm::min(aabbMin_, position);
            aabbMax_ = glm::max(aabbMax_, position);
        }
    }

    // Interleave: position (vec3) + normal (vec3) + uv (vec2) + tangent (vec3)
    std::vector<float> vertData;
    vertData.reserve(positions.size() * 11);
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
        if (i < uvs.size()) {
            vertData.push_back(uvs[i].x);
            vertData.push_back(uvs[i].y);
        } else {
            vertData.push_back(0.0f);
            vertData.push_back(0.0f);
        }
        if (i < tangents.size()) {
            vertData.push_back(tangents[i].x);
            vertData.push_back(tangents[i].y);
            vertData.push_back(tangents[i].z);
        } else {
            vertData.push_back(1.0f);
            vertData.push_back(0.0f);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);

    // location 1: normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    // location 2: uv (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                          (void*)(6 * sizeof(float)));

    // location 3: tangent (vec3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                          (void*)(8 * sizeof(float)));

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
    auto data = generateCube(size);
    return Mesh(data.positions, data.normals, data.uvs, data.tangents, data.indices);
}

Mesh Mesh::createPlane(float size) {
    auto data = generatePlane(size);
    return Mesh(data.positions, data.normals, data.uvs, data.tangents, data.indices);
}

Mesh Mesh::createCylinder(float radius, float height, int segments) {
    auto data = generateCylinder(radius, height, segments);
    return Mesh(data.positions, data.normals, data.uvs, data.tangents, data.indices);
}

Mesh Mesh::createHand() {
    auto data = generateHand();
    return Mesh(data.positions, data.normals, data.uvs, data.tangents, data.indices);
}

Mesh Mesh::createDagger() {
    auto data = generateDagger();
    return Mesh(data.positions, data.normals, data.uvs, data.tangents, data.indices);
}
