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
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> tangents;
    std::vector<uint32_t> indices;
};

inline void generateTangents(RawMeshData& mesh) {
    if (mesh.positions.size() != mesh.normals.size() ||
        mesh.positions.size() != mesh.uvs.size()) {
        mesh.tangents.clear();
        return;
    }

    mesh.tangents.assign(mesh.positions.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(mesh.positions.size(), glm::vec3(0.0f));

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t ia = mesh.indices[i + 0];
        const uint32_t ib = mesh.indices[i + 1];
        const uint32_t ic = mesh.indices[i + 2];

        const glm::vec3& p0 = mesh.positions[ia];
        const glm::vec3& p1 = mesh.positions[ib];
        const glm::vec3& p2 = mesh.positions[ic];
        const glm::vec2& uv0 = mesh.uvs[ia];
        const glm::vec2& uv1 = mesh.uvs[ib];
        const glm::vec2& uv2 = mesh.uvs[ic];

        const glm::vec3 edge1 = p1 - p0;
        const glm::vec3 edge2 = p2 - p0;
        const glm::vec2 deltaUv1 = uv1 - uv0;
        const glm::vec2 deltaUv2 = uv2 - uv0;
        const float det = deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y;
        if (std::abs(det) <= 1e-6f) {
            continue;
        }

        const float invDet = 1.0f / det;
        const glm::vec3 tangent = (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * invDet;
        const glm::vec3 bitangent = (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * invDet;

        mesh.tangents[ia] += tangent;
        mesh.tangents[ib] += tangent;
        mesh.tangents[ic] += tangent;
        bitangents[ia] += bitangent;
        bitangents[ib] += bitangent;
        bitangents[ic] += bitangent;
    }

    for (size_t i = 0; i < mesh.tangents.size(); ++i) {
        glm::vec3 tangent = mesh.tangents[i];
        const glm::vec3 normal = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::dot(tangent, tangent) <= 1e-12f) {
            tangent = std::abs(normal.y) < 0.999f
                ? glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), normal))
                : glm::normalize(glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), normal));
        } else {
            tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
        }

        if (glm::dot(tangent, tangent) <= 1e-12f) {
            tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        mesh.tangents[i] = tangent;
    }
}

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

    std::vector<glm::vec2> uvs = {
        {0,0}, {1,0}, {1,1}, {0,1},
        {0,0}, {1,0}, {1,1}, {0,1},
        {0,0}, {1,0}, {1,1}, {0,1},
        {0,0}, {1,0}, {1,1}, {0,1},
        {0,0}, {1,0}, {1,1}, {0,1},
        {0,0}, {1,0}, {1,1}, {0,1},
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

    RawMeshData mesh{positions, normals, uvs, {}, indices};
    generateTangents(mesh);
    return mesh;
}

inline RawMeshData generatePlane(float size) {
    float h = size * 0.5f;
    RawMeshData mesh{
        {{-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h}},
        {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}},
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}},
        {},
        {0, 1, 2, 0, 2, 3}
    };
    generateTangents(mesh);
    return mesh;
}

inline RawMeshData generateCylinder(float radius, float height, int segments) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;

    constexpr float PI = 3.14159265f;
    segments = std::max(segments, 3);

    // Side vertices: 2 rings
    for (int ring = 0; ring <= 1; ++ring) {
        float y = ring * height;
        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(i) / segments * 2.0f * PI;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            positions.push_back(glm::vec3(x, y, z));
            normals.push_back(glm::normalize(glm::vec3(std::cos(angle), 0.0f, std::sin(angle))));
            uvs.push_back(glm::vec2(static_cast<float>(i) / segments, static_cast<float>(ring)));
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
    uvs.push_back(glm::vec2(0.5f, 0.5f));
    uint32_t botRingStart = static_cast<uint32_t>(positions.size());
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * 2.0f * PI;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        positions.push_back(glm::vec3(x, 0.0f, z));
        normals.push_back(glm::vec3(0, -1, 0));
        uvs.push_back(glm::vec2(std::cos(angle) * 0.5f + 0.5f, std::sin(angle) * 0.5f + 0.5f));
    }
    for (int i = 0; i < segments; ++i) {
        indices.push_back(botCenter);
        indices.push_back(botRingStart + i + 1);
        indices.push_back(botRingStart + i);
    }

    // Top cap
    uint32_t topCenter = static_cast<uint32_t>(positions.size());
    positions.push_back(glm::vec3(0, height, 0));
    normals.push_back(glm::vec3(0, 1, 0));
    uvs.push_back(glm::vec2(0.5f, 0.5f));
    uint32_t topRingStart = static_cast<uint32_t>(positions.size());
    for (int i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) / segments * 2.0f * PI;
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        positions.push_back(glm::vec3(x, height, z));
        normals.push_back(glm::vec3(0, 1, 0));
        uvs.push_back(glm::vec2(std::cos(angle) * 0.5f + 0.5f, std::sin(angle) * 0.5f + 0.5f));
    }
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(topRingStart + i);
        indices.push_back(topRingStart + i + 1);
    }

    RawMeshData mesh{positions, normals, uvs, {}, indices};
    generateTangents(mesh);
    return mesh;
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
            if (i < mesh.uvs.size()) {
                result.uvs.push_back(mesh.uvs[i]);
            } else {
                result.uvs.push_back(glm::vec2(0.0f));
            }
            if (i < mesh.tangents.size()) {
                result.tangents.push_back(glm::normalize(normalMatrix * mesh.tangents[i]));
            } else {
                result.tangents.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
            }
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

        if (i < result.tangents.size()) {
            glm::vec3& tangent = result.tangents[i];
            float tY = tangent.y;
            float tZ = tangent.z;
            tangent.y = tY * cosA - tZ * sinA;
            tangent.z = tY * sinA + tZ * cosA;
        }
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
