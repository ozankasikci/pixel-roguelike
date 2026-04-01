#pragma once
#include <array>
#include <glm/glm.hpp>

// Frustum culling utilities.
// Uses Gribb-Hartmann plane extraction from the view-projection matrix.

// Extract 6 clip planes from a view-projection matrix (column-major GLM layout).
// Planes are stored as (nx, ny, nz, d) where dot(plane, point) >= 0 means inside.
// Order: left, right, bottom, top, near, far.
std::array<glm::vec4, 6> extractFrustumPlanes(const glm::mat4& vp);

// Test whether a world-space AABB (transformed from local by modelMatrix) is at least
// partially inside the frustum defined by the 6 clip planes.
// Returns true if the AABB should be rendered (at least one corner inside every plane).
// Returns false if the AABB is fully outside any single plane (cull it).
bool isAabbInsideFrustum(const glm::vec3& localAabbMin,
                          const glm::vec3& localAabbMax,
                          const glm::mat4& modelMatrix,
                          const std::array<glm::vec4, 6>& planes);
