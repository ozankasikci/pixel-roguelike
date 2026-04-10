#include "engine/ecs/GameRegistry.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"
#include "game/modules/checkpoint/CheckpointFeedbackState.h"
#include "game/modules/checkpoint/CheckpointModule.h"
#include "game/modules/checkpoint/CheckpointSpawner.h"
#include "game/modules/checkpoint/CheckpointSystem.h"
#include "game/runtime/GameplayBehaviors.h"
#include "game/runtime/GameplayEventSink.h"
#include "game/session/RunSession.h"
#include "game/ui/InteractionFocusState.h"

#include <cassert>

int main() {
    registerCheckpointModule();

    GameRegistry registry;
    MeshLibrary meshLibrary;
    std::vector<entt::entity> entities;
    LevelBuildContext context{registry, meshLibrary, entities};
    LevelBuilder builder(context);

    const entt::entity player = registry.create();
    registry.emplace<PlayerSpawnComponent>(player, PlayerSpawnComponent{glm::vec3(0.0f, 1.6f, 0.0f), -8.0f});
    registry.emplace<PlayerTag>(player);

    LevelCheckpointPlacement firstPlacement;
    firstPlacement.name = "First";
    firstPlacement.position = glm::vec3(0.0f, 0.0f, 0.0f);
    firstPlacement.respawnPosition = glm::vec3(0.0f, 1.6f, 2.5f);
    firstPlacement.lightIntensity = 1.0f;
    firstPlacement.nodeId = "checkpoint_first";

    LevelCheckpointPlacement secondPlacement;
    secondPlacement.name = "Second";
    secondPlacement.position = glm::vec3(5.0f, 0.0f, -3.0f);
    secondPlacement.respawnPosition = glm::vec3(5.0f, 1.6f, -0.5f);
    secondPlacement.lightIntensity = 1.0f;
    secondPlacement.nodeId = "checkpoint_second";

    const entt::entity firstCheckpoint = spawnCheckpointEntity(builder, firstPlacement);
    const entt::entity secondCheckpoint = spawnCheckpointEntity(builder, secondPlacement);
    registry.get<CheckpointComponent>(firstCheckpoint).active = true;

    RunSession session;
    registry.ctx().insert_or_assign<RunSession*>(&session);
    registry.ctx().insert_or_assign<InteractionFocusState>(
        InteractionFocusState{player, secondCheckpoint, true, false});

    GameplayBehaviors behaviors;
    GameplayEventSink events;
    behaviors.tick(registry, 0.0f, events);

    const auto& firstCheckpointState = registry.get<CheckpointComponent>(firstCheckpoint);
    const auto& secondCheckpointState = registry.get<CheckpointComponent>(secondCheckpoint);
    const auto& playerSpawn = registry.get<PlayerSpawnComponent>(player);
    const auto& checkpointInteractable = registry.get<InteractableComponent>(secondCheckpoint);

    assert(!firstCheckpointState.active);
    assert(secondCheckpointState.active);
    assert(playerSpawn.respawnPosition == secondPlacement.respawnPosition);
    assert(session.respawnPosition == secondPlacement.respawnPosition);
    assert(registry.ctx().contains<RuntimeCheckpointFeedbackState>());
    assert(registry.ctx().get<RuntimeCheckpointFeedbackState>().messageTimer == 2.5f);
    assert(checkpointInteractable.busy);

    tickCheckpointFeedback(registry, 0.0f);
    const auto& checkpointLight = registry.get<LightComponent>(secondCheckpointState.lightEntity);
    assert(checkpointInteractable.promptText == "RESPAWN ATTUNED");
    assert(checkpointLight.intensity >= 2.2f);
    assert(checkpointLight.radius >= 10.0f);

    tickCheckpointFeedback(registry, 3.0f);
    assert(!registry.get<InteractableComponent>(secondCheckpoint).busy);

    const auto& checkpointBehavior = registry.get<BehaviorComponent>(secondCheckpoint);
    assert(checkpointBehavior.onActivate.size() == 1);

    return 0;
}
