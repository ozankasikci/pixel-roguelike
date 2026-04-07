#include "game/components/CheckpointComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/TransformComponent.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"

#include <cassert>

int main() {
    entt::registry registry;
    MeshLibrary meshLibrary;
    std::vector<entt::entity> entities;
    LevelBuildContext context{registry, meshLibrary, entities};
    LevelBuilder builder(context);

    const CheckpointSpawnSpec checkpointPlacement{
        glm::vec3(0.0f, 1.3f, -35.3f),
        glm::vec3(0.0f, 1.6f, -32.8f),
        2.4f,
        0.55f,
        glm::vec3(0.0f, 2.6f, -35.3f),
        glm::vec3(1.0f, 0.82f, 0.62f),
        5.5f,
        4.0f
    };

    const entt::entity checkpoint = spawnCheckpoint(builder, checkpointPlacement);
    assert(registry.valid(checkpoint));
    assert(entities.size() == 2);
    assert((registry.all_of<TransformComponent, CheckpointComponent>(checkpoint)));
    assert(registry.get<TransformComponent>(checkpoint).position == checkpointPlacement.position);

    const auto& checkpointComponent = registry.get<CheckpointComponent>(checkpoint);
    const auto& checkpointInteractable = registry.get<InteractableComponent>(checkpoint);
    assert(checkpointComponent.respawnPosition == checkpointPlacement.respawnPosition);
    assert(checkpointComponent.interactDistance == checkpointPlacement.interactDistance);
    assert(checkpointComponent.interactDotThreshold == checkpointPlacement.interactDotThreshold);
    assert(checkpointInteractable.promptText == "E  KINDLE CHECKPOINT");
    assert(registry.valid(checkpointComponent.lightEntity));
    assert((registry.all_of<TransformComponent, LightComponent>(checkpointComponent.lightEntity)));
    assert(registry.get<TransformComponent>(checkpointComponent.lightEntity).position == checkpointPlacement.lightPosition);

    return 0;
}
