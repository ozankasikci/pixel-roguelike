#include "engine/rendering/FrustumCulling.h"

std::array<glm::vec4, 6> extractFrustumPlanes(const glm::mat4& vp) {
    // Gribb-Hartmann method: extract planes from VP matrix columns
    // GLM is column-major: vp[col][row]
    std::array<glm::vec4, 6> planes;
    // Left:   row3 + row0
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                           vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right:  row3 - row0
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                           vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom: row3 + row1
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                           vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top:    row3 - row1
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                           vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near:   row3 + row2
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
                           vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far:    row3 - row2
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                           vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    return planes;
}

bool isAabbInsideFrustum(const glm::vec3& localAabbMin,
                          const glm::vec3& localAabbMax,
                          const glm::mat4& modelMatrix,
                          const std::array<glm::vec4, 6>& planes) {
    // Build 8 world-space corners from local AABB + model matrix
    const glm::vec3 extent = localAabbMax - localAabbMin;
    glm::vec4 corners[8];
    int idx = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                glm::vec3 local = localAabbMin + extent * glm::vec3(float(x), float(y), float(z));
                corners[idx++] = modelMatrix * glm::vec4(local, 1.0f);
            }
        }
    }

    // Test each plane: if ALL 8 corners are outside any single plane, cull
    for (const auto& plane : planes) {
        bool allOutside = true;
        for (int c = 0; c < 8; ++c) {
            if (glm::dot(plane, corners[c]) >= 0.0f) {
                allOutside = false;
                break;
            }
        }
        if (allOutside) return false;
    }
    return true;
}
