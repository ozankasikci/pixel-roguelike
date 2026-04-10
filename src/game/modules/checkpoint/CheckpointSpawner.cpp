#include "game/modules/checkpoint/CheckpointSpawner.h"

#include "game/modules/checkpoint/CheckpointActionTypes.h"

#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"
#include "game/prefabs/GameplayPrefabData.h"

namespace {

entt::entity spawnCheckpointCommon(LevelBuilder& builder,
                                   const glm::vec3& position,
                                   const glm::vec3& respawnPosition,
                                   float interactDistance,
                                   float interactDotThreshold,
                                   const glm::vec3& lightPosition,
                                   const glm::vec3& lightColor,
                                   float lightRadius,
                                   float lightIntensity,
                                   const std::string& nodeId) {
    auto& registry = builder.registry();

    const entt::entity lightEntity = builder.addLight(lightPosition,
                                                      lightColor,
                                                      lightRadius,
                                                      lightIntensity);

    const entt::entity checkpointEntity = builder.createTransformEntity(position);
    registry.emplace<CheckpointComponent>(checkpointEntity,
                                          CheckpointComponent{
                                              respawnPosition,
                                              interactDistance,
                                              interactDotThreshold,
                                              false,
                                              lightEntity,
                                          });
    registry.emplace<InteractableComponent>(checkpointEntity,
                                            InteractableComponent{
                                                "E  KINDLE CHECKPOINT",
                                                "CHECKPOINT KINDLED",
                                                interactDistance,
                                                interactDotThreshold,
                                                true,
                                                false,
                                            });

    BehaviorComponent behavior;
    ActionEntry action;
    action.type = ActionType::ActivateCheckpoint;
    action.targetNodeId = "self";
    action.params = ActivateCheckpointParams{};
    behavior.onActivate.push_back(action);
    registry.emplace<BehaviorComponent>(checkpointEntity, std::move(behavior));

    if (!nodeId.empty()) {
        builder.attachNodeId(checkpointEntity, nodeId);
    }

    return checkpointEntity;
}

} // namespace

entt::entity spawnCheckpointEntity(LevelBuilder& builder,
                                   const LevelCheckpointPlacement& placement) {
    return spawnCheckpointCommon(builder,
                                 placement.position,
                                 placement.respawnPosition,
                                 placement.interactDistance,
                                 placement.interactDotThreshold,
                                 placement.position + placement.lightOffset,
                                 placement.lightColor,
                                 placement.lightRadius,
                                 placement.lightIntensity,
                                 placement.nodeId);
}

entt::entity spawnCheckpointPrefab(LevelBuilder& builder,
                                   const CheckpointSpawnSpec& spec) {
    return spawnCheckpointCommon(builder,
                                 spec.position,
                                 spec.respawnPosition,
                                 spec.interactDistance,
                                 spec.interactDotThreshold,
                                 spec.lightPosition,
                                 spec.lightColor,
                                 spec.lightRadius,
                                 spec.lightIntensity,
                                 "");
}
