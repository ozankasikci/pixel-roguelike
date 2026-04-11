#include "game/modules/particles/ParticleSpawner.h"

#include "game/components/ParticleEmitterComponent.h"
#include "game/level/LevelBuilder.h"

entt::entity spawnParticleEmitter(LevelBuilder& builder,
                                  const std::string& emitterId,
                                  const glm::vec3& position,
                                  const std::string& nodeId,
                                  const std::string& parentNodeId) {
    auto entity = builder.createTransformEntity(position);
    builder.registry().emplace<ParticleEmitterComponent>(
        entity, ParticleEmitterComponent{emitterId, true});
    if (!nodeId.empty()) {
        builder.attachNodeId(entity, nodeId);
    }
    builder.track(entity);
    return entity;
}
