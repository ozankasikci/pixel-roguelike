#pragma once

#include <glm/glm.hpp>

// Pivot-based door leaf model matrix computation.
// groupWorldPos: world position of the door group root (= frame center).
// closedYawDeg: Y rotation of the closed door assembly in degrees.
// currentYawDeg: current Y rotation (= closedYawDeg when closed, interpolated when opening).
// pivot: local-space hinge offset on the leaf mesh.
// meshCenter: AABB center of the door mesh (for frame alignment).
// scale: world scale of the leaf mesh.
glm::mat4 makePivotLeafModel(const glm::vec3& groupWorldPos,
                              float closedYawDeg,
                              float currentYawDeg,
                              const glm::vec3& pivot,
                              const glm::vec3& meshCenter,
                              const glm::vec3& scale);
