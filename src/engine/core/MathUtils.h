#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

inline glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

inline glm::mat4 makeTransformMatrix(const glm::vec3& position,
                                     const glm::vec3& rotationDegrees,
                                     const glm::vec3& scale) {
    // Build rotation via quaternion in extrinsic XYZ order (= intrinsic ZYX).
    // Matches the editor's extractEulerAngleXYZ decompose for clean round-trips.
    const glm::quat q = glm::angleAxis(glm::radians(rotationDegrees.x), glm::vec3(1, 0, 0))
                      * glm::angleAxis(glm::radians(rotationDegrees.y), glm::vec3(0, 1, 0))
                      * glm::angleAxis(glm::radians(rotationDegrees.z), glm::vec3(0, 0, 1));
    glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
    matrix = matrix * glm::mat4_cast(q);
    matrix = glm::scale(matrix, scale);
    return matrix;
}
