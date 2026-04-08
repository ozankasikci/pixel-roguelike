#include "game/systems/KinematicColliderSystem.h"

#include "engine/physics/PhysicsSystem.h"
#include "game/components/ColliderComponent.h"
#include "game/components/KinematicLinkComponent.h"
#include "game/components/MeshComponent.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>

void tickKinematicColliders(entt::registry& registry, PhysicsSystem& physics) {
    auto view = registry.view<ColliderComponent, KinematicLinkComponent>();
    for (auto [entity, collider, link] : view.each()) {
        if (link.parentMesh == entt::null) continue;
        if (!registry.valid(link.parentMesh)) continue;

        const auto* parentMesh = registry.try_get<MeshComponent>(link.parentMesh);
        if (!parentMesh || !parentMesh->useModelOverride) continue;

        // Decompose parent mesh's modelOverride into position and rotation
        const glm::mat4& model = parentMesh->modelOverride;
        glm::vec3 position{0.0f};
        glm::vec3 scale{1.0f};
        glm::quat orientation;
        glm::vec3 skew{0.0f};
        glm::vec4 perspective{0.0f};
        glm::decompose(model, scale, orientation, position, skew, perspective);
        glm::vec3 eulerRad = glm::eulerAngles(orientation);
        glm::vec3 rotationDeg = glm::degrees(eulerRad);

        // Update ColliderComponent so debug vis stays in sync
        collider.position = position;
        collider.rotation = rotationDeg;

        // Move the Jolt kinematic body smoothly
        physics.moveKinematicBody(entity, position, rotationDeg);
    }
}
