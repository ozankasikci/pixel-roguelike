#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};  // euler angles in degrees (XYZ intrinsic / ZYX extrinsic)
    glm::vec3 scale{1.0f};

    glm::mat4 modelMatrix() const {
        // Build rotation via quaternion in extrinsic XYZ order (= intrinsic ZYX).
        // Matches the editor's extractEulerAngleXYZ decompose for clean round-trips.
        const glm::quat q = glm::angleAxis(glm::radians(rotation.x), glm::vec3(1, 0, 0))
                          * glm::angleAxis(glm::radians(rotation.y), glm::vec3(0, 1, 0))
                          * glm::angleAxis(glm::radians(rotation.z), glm::vec3(0, 0, 1));
        glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
        m = m * glm::mat4_cast(q);
        m = glm::scale(m, scale);
        return m;
    }
};
