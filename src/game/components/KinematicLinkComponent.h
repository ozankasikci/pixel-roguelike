#pragma once
#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct KinematicLinkComponent {
    entt::entity parentMesh = entt::null;
    glm::vec3 localOffset{0.0f};  // Collider's local offset from parent mesh origin
    glm::mat4 localModel{1.0f};   // Full collider local transform relative to parent mesh model
};
