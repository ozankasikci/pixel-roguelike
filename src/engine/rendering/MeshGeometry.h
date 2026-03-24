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

// Bend finger vertices of a hand mesh into a gripping pose.
// Vertices above the knuckle threshold (knuckleFrac of the Y range) are
// progressively rotated around the X axis, curling them toward +Z and
// downward.  bendAngleDeg controls how far the fingertips curl (90-120
// produces a tight fist).
inline RawMeshData bendFingers(const RawMeshData& hand, float bendAngleDeg,
                               float knuckleFrac = 0.75f) {
    RawMeshData result = hand;

    // Bounding box of the original hand
    glm::vec3 bMin = hand.positions[0], bMax = hand.positions[0];
    for (const auto& p : hand.positions) {
        bMin = glm::min(bMin, p);
        bMax = glm::max(bMax, p);
    }

    float knuckleY   = bMin.y + (bMax.y - bMin.y) * knuckleFrac;
    float fingerRange = bMax.y - knuckleY;
    if (fingerRange < 1e-6f) return result;   // degenerate

    float centerZ  = (bMin.z + bMax.z) * 0.5f;
    float maxAngle = glm::radians(bendAngleDeg);

    for (size_t i = 0; i < result.positions.size(); ++i) {
        glm::vec3& pos  = result.positions[i];
        glm::vec3& norm = result.normals[i];

        if (pos.y <= knuckleY) continue;

        float frac  = glm::clamp((pos.y - knuckleY) / fingerRange, 0.0f, 1.0f);
        float angle = frac * maxAngle;
        float cosA  = std::cos(angle);
        float sinA  = std::sin(angle);

        // Rotate (Y, Z) around the knuckle pivot line
        float relY = pos.y - knuckleY;
        float relZ = pos.z - centerZ;
        pos.y = knuckleY + relY * cosA - relZ * sinA;
        pos.z = centerZ  + relY * sinA + relZ * cosA;

        // Rotate normal's Y/Z components the same way
        float nY = norm.y, nZ = norm.z;
        norm.y = nY * cosA - nZ * sinA;
        norm.z = nY * sinA + nZ * cosA;
    }

    return result;
}

inline RawMeshData generateDagger() {
    auto unitCube = generateCube(1.0f);

    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto box = [&](glm::vec3 pos, glm::vec3 size) {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), pos);
        t = glm::scale(t, size);
        parts.push_back({unitCube, t});
    };

    // Guard at y=0, blade extends in +Y, handle in -Y

    // Blade body
    box({0.0f, 0.17f, 0.0f}, {0.035f, 0.30f, 0.014f});

    // Blade tip — narrower for tapered look
    box({0.0f, 0.335f, 0.0f}, {0.020f, 0.04f, 0.008f});

    // Cross-guard
    box({0.0f, 0.0f, 0.0f}, {0.065f, 0.015f, 0.015f});

    // Handle/grip
    box({0.0f, -0.045f, 0.0f}, {0.016f, 0.075f, 0.016f});

    // Pommel
    box({0.0f, -0.093f, 0.0f}, {0.022f, 0.020f, 0.022f});

    return mergeMeshes(parts);
}

inline RawMeshData generateHand() {
    auto unitCube = generateCube(1.0f);

    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto box = [&](glm::vec3 pos, glm::vec3 size, float rotZ = 0.0f) {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), pos);
        if (rotZ != 0.0f)
            t = glm::rotate(t, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
        t = glm::scale(t, size);
        parts.push_back({unitCube, t});
    };

    // Palm
    box({0.0f, 0.0f, 0.0f}, {0.10f, 0.11f, 0.035f});

    // Forearm
    box({0.005f, -0.16f, 0.005f}, {0.07f, 0.22f, 0.05f});

    // Knuckle ridge
    box({0.0f, 0.048f, -0.002f}, {0.105f, 0.02f, 0.038f});

    // Index finger
    box({-0.030f, 0.095f, 0.0f}, {0.020f, 0.080f, 0.020f});

    // Middle finger (tallest)
    box({-0.007f, 0.102f, 0.0f}, {0.022f, 0.093f, 0.022f});

    // Ring finger
    box({0.017f, 0.093f, 0.0f}, {0.020f, 0.076f, 0.020f});

    // Pinky finger
    box({0.039f, 0.083f, 0.0f}, {0.017f, 0.058f, 0.017f});

    // Thumb (angled outward)
    box({-0.058f, 0.000f, 0.010f}, {0.024f, 0.055f, 0.024f}, -40.0f);

    return mergeMeshes(parts);
}

