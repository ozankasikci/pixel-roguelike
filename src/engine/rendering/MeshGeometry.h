#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include <utility>

struct RawMeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
};

inline RawMeshData generateCube(float size) {
    float h = size * 0.5f;

    std::vector<glm::vec3> positions = {
        // Front (+Z)
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h},
        // Back (-Z)
        { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h},
        // Left (-X)
        {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h},
        // Right (+X)
        { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h},
        // Top (+Y)
        {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h},
        // Bottom (-Y)
        {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h},
    };

    std::vector<glm::vec3> normals = {
        {0,0,1},{0,0,1},{0,0,1},{0,0,1},
        {0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1},
        {-1,0,0},{-1,0,0},{-1,0,0},{-1,0,0},
        {1,0,0},{1,0,0},{1,0,0},{1,0,0},
        {0,1,0},{0,1,0},{0,1,0},{0,1,0},
        {0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},
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

    return {positions, normals, indices};
}

inline RawMeshData generatePlane(float size) {
    float h = size * 0.5f;
    return {
        {{-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h}},
        {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}},
        {0, 1, 2, 0, 2, 3}
    };
}

inline RawMeshData generateCylinder(float radius, float height, int segments) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;

    constexpr float PI = 3.14159265f;

    // Side vertices: 2 rings
    for (int ring = 0; ring <= 1; ++ring) {
        float y = ring * height;
        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(i) / segments * 2.0f * PI;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            positions.push_back(glm::vec3(x, y, z));
            normals.push_back(glm::normalize(glm::vec3(std::cos(angle), 0.0f, std::sin(angle))));
        }
    }

    // Side indices
    int stride = segments + 1;
    for (int i = 0; i < segments; ++i) {
        uint32_t bl = i, br = i + 1, tl = i + stride, tr = i + 1 + stride;
        indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
        indices.push_back(br); indices.push_back(tr); indices.push_back(tl);
    }

    // Bottom cap
    uint32_t botCenter = static_cast<uint32_t>(positions.size());
    positions.push_back(glm::vec3(0, 0, 0));
    normals.push_back(glm::vec3(0, -1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(botCenter);
        indices.push_back(i + 1);
        indices.push_back(i);
    }

    // Top cap
    uint32_t topCenter = static_cast<uint32_t>(positions.size());
    positions.push_back(glm::vec3(0, height, 0));
    normals.push_back(glm::vec3(0, 1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(i + stride);
        indices.push_back(i + 1 + stride);
    }

    return {positions, normals, indices};
}

inline RawMeshData mergeMeshes(const std::vector<std::pair<RawMeshData, glm::mat4>>& parts) {
    RawMeshData result;

    for (const auto& [mesh, transform] : parts) {
        uint32_t baseIndex = static_cast<uint32_t>(result.positions.size());
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            glm::vec3 pos = glm::vec3(transform * glm::vec4(mesh.positions[i], 1.0f));
            glm::vec3 norm = glm::normalize(normalMatrix * mesh.normals[i]);
            result.positions.push_back(pos);
            result.normals.push_back(norm);
        }

        for (uint32_t idx : mesh.indices) {
            result.indices.push_back(baseIndex + idx);
        }
    }

    return result;
}
