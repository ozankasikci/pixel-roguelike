#include "game/prefabs/GameplayPrefabs.h"

#include "game/level/LevelBuilder.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 makePivotLeafModel(const glm::vec3& basePos,
                              float closedYawDeg,
                              float currentYawDeg,
                              const glm::vec3& pivot,
                              const glm::vec3& meshCenter,
                              const glm::vec3& scale) {
    // Door meshes have their origin at the hinge edge, not at the geometric center.
    // Frame meshes are centered at origin. To align the door within the frame:
    //   T(-meshCenter) shifts the door so its AABB center is at the origin.
    //
    // When closed (deltaYaw=0), the door center aligns with the frame center:
    //   T(basePos) * R(closedYaw) * S(scale) * T(-meshCenter)
    // When opening, the door rotates around the pivot (hinge) in mesh-local space:
    //   T(basePos) * R(closedYaw) * S * T(-meshCenter) * T(pivot) * R(deltaYaw) * T(-pivot)
    const float deltaYaw = currentYawDeg - closedYawDeg;
    // Only center horizontally (X/Z). Both door and frame meshes start at Y=0 (floor).
    const glm::vec3 horizontalCenter(meshCenter.x, 0.0f, meshCenter.z);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), basePos);
    model = glm::rotate(model, glm::radians(closedYawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    model = glm::translate(model, -horizontalCenter);
    model = glm::translate(model, pivot);
    model = glm::rotate(model, glm::radians(deltaYaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, -pivot);
    return model;
}

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec) {
    auto checkpointLight = builder.addLight(
        spec.lightPosition,
        spec.lightColor,
        spec.lightRadius,
        spec.lightIntensity
    );

    auto checkpoint = builder.createTransformEntity(spec.position);
    builder.registry().emplace<CheckpointComponent>(
        checkpoint,
        CheckpointComponent{
            spec.respawnPosition,
            spec.interactDistance,
            spec.interactDotThreshold,
            false,
            checkpointLight
        }
    );
    builder.registry().emplace<InteractableComponent>(
        checkpoint,
        InteractableComponent{
            "E  KINDLE CHECKPOINT",
            "CHECKPOINT KINDLED",
            spec.interactDistance,
            spec.interactDotThreshold,
            true,
            false
        }
    );
    return checkpoint;
}

entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance) {
    switch (instance.type) {
    case GameplayPrefabType::Checkpoint:
        return spawnCheckpoint(builder, instance.checkpoint);
    }
    return entt::null;
}
