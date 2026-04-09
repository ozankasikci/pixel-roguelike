#include "game/systems/KinematicColliderSystem.h"

#include "engine/physics/PhysicsSystem.h"
#include "game/components/ColliderComponent.h"
#include "game/components/KinematicLinkComponent.h"
#include "game/components/MeshComponent.h"
#include <spdlog/spdlog.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>

static bool s_loggedOnce = false;

void tickKinematicColliders(GameRegistry& registry, PhysicsSystem& physics, float deltaTime) {
    auto view = registry.view<ColliderComponent, KinematicLinkComponent>();
    int count = 0;
    for (auto [entity, collider, link] : view.each()) {
        if (link.parentMesh == entt::null) continue;
        if (!registry.valid(link.parentMesh)) continue;

        const auto* parentMesh = registry.try_get<MeshComponent>(link.parentMesh);
        if (!parentMesh || !parentMesh->useModelOverride) {
            if (!s_loggedOnce) {
                spdlog::warn("KinematicCollider: entity={} parent has no modelOverride (mesh={}, useOverride={})",
                             static_cast<uint32_t>(entity),
                             parentMesh != nullptr,
                             parentMesh ? parentMesh->useModelOverride : false);
            }
            continue;
        }

        // Apply the collider's authored local transform in the animated mesh's space.
        const glm::mat4 worldModel = parentMesh->modelOverride * link.localModel;
        glm::vec3 parentPos{0.0f};
        glm::vec3 scale{1.0f};
        glm::quat orientation;
        glm::vec3 skew{0.0f};
        glm::vec4 perspective{0.0f};
        glm::decompose(worldModel, scale, orientation, parentPos, skew, perspective);
        glm::vec3 eulerRad = glm::eulerAngles(orientation);
        glm::vec3 rotationDeg = glm::degrees(eulerRad);

        // Update ColliderComponent so debug vis stays in sync
        collider.position = parentPos;
        collider.rotation = rotationDeg;

        // Move the Jolt kinematic body with the same frame delta as the door animation.
        physics.moveKinematicBody(entity, parentPos, rotationDeg, deltaTime);
        ++count;
    }

    if (!s_loggedOnce && count > 0) {
        spdlog::info("KinematicCollider: ticking {} kinematic colliders", count);
        s_loggedOnce = true;
    }
}
