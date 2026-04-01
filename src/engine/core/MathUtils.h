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

// Extract normalized rotation matrix from a model matrix (strips translation and scale).
inline glm::mat3 extractRotationMatrix(const glm::mat4& matrix) {
    glm::mat3 basis(matrix);
    for (int column = 0; column < 3; ++column) {
        const float len = glm::length(basis[column]);
        if (len > 0.0001f) {
            basis[column] /= len;
        }
    }
    return basis;
}

// Build a model matrix from position, scale, and optional XYZ Euler rotation (degrees).
inline glm::mat4 makeModelMatrix(const glm::vec3& position,
                                  const glm::vec3& scale,
                                  const glm::vec3& rotation = glm::vec3(0.0f)) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    if (rotation.x != 0.0f) m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    if (rotation.y != 0.0f) m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    if (rotation.z != 0.0f) m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    return glm::scale(m, scale);
}
