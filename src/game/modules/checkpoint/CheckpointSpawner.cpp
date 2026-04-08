#include "game/modules/checkpoint/CheckpointSpawner.h"

#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"

entt::entity spawnCheckpointEntity(LevelBuilder& builder,
                                   const LevelCheckpointPlacement& placement) {
    auto& reg = builder.registry();

    // Spawn the checkpoint light
    entt::entity lightEntity = builder.addLight(
        placement.position + placement.lightOffset,
        placement.lightColor,
        placement.lightRadius,
        placement.lightIntensity);

    // Create root transform entity at the checkpoint position
    auto root = builder.createTransformEntity(placement.position);

    // Emplace CheckpointComponent
    CheckpointComponent cp;
    cp.respawnPosition = placement.respawnPosition;
    cp.interactDistance = placement.interactDistance;
    cp.interactDotThreshold = placement.interactDotThreshold;
    cp.lightEntity = lightEntity;
    reg.emplace<CheckpointComponent>(root, cp);

    // Emplace InteractableComponent
    reg.emplace<InteractableComponent>(root,
        InteractableComponent{
            "E  KINDLE CHECKPOINT",
            "CHECKPOINT KINDLED",
            placement.interactDistance,
            placement.interactDotThreshold,
            true,
            false
        });

    // BehaviorComponent with ActivateCheckpoint action
    BehaviorComponent behavior;
    ActionEntry activateAction;
    activateAction.type = ActionType::ActivateCheckpoint;
    activateAction.targetNodeId = "self";
    activateAction.fireOnce = true;
    behavior.onActivate.push_back(activateAction);
    reg.emplace<BehaviorComponent>(root, behavior);

    // Attach nodeId if present
    if (!placement.nodeId.empty()) {
        builder.attachNodeId(root, placement.nodeId);
    }

    return root;
}
