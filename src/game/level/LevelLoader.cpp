#include "game/level/LevelLoader.h"

#include "game/behavior/NodeIdComponent.h"
#include "game/behavior/NodeIndex.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/session/RunSession.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "engine/core/PathUtils.h"

#include <cassert>

LevelLoader::LevelLoader(LevelBuildContext& context)
    : context_(context) {}

void LevelLoader::load(const LevelLoadRequest& request, const LevelLoadArgs& args) {
    assert(args.content != nullptr && "LevelLoadArgs::content must not be null");
    assert(args.session != nullptr && "LevelLoadArgs::session must not be null");

    // If no pre-loaded LevelDef, load from file
    LevelDef loadedDef;
    const LevelDef& rawLevel = args.levelDef
        ? *args.levelDef
        : (loadedDef = loadLevelDef(resolveProjectPath(request.levelPath)), loadedDef);

    ContentRegistry& content = *args.content;
    RunSession& session      = *args.session;
    auto& registry           = context_.registry;

    if (request.registerAssets) {
        request.registerAssets(context_.meshLibrary);
    }
    registry.ctx().insert_or_assign(MeshAssetProvider{&context_.meshLibrary});

    const LevelDef level = resolveLevelHierarchy(rawLevel);
    registry.ctx().insert_or_assign(ActiveEnvironmentProfile{request.levelId, level.environmentId, level.environmentProfile});

    LevelBuilder builder(context_);
    if (request.buildScriptedGeometry) {
        request.buildScriptedGeometry(builder);
    }

    for (const auto& placement : level.meshes) {
        auto entity = builder.addMesh(placement.meshId,
                                      placement.position,
                                      placement.scale,
                                      placement.rotation,
                                      placement.tint,
                                      placement.materialId.empty()
                                          ? std::optional<std::string>{}
                                          : std::optional<std::string>{placement.materialId});
        builder.attachNodeId(entity, placement.nodeId);
        if (!placement.behaviors.empty()) {
            builder.attachBehaviors(entity, placement.behaviors);
        }
        if (placement.interactable.has_value()) {
            builder.attachInteractable(entity, *placement.interactable);
        }
    }

    for (const auto& placement : level.lights) {
        auto entity = builder.addLight(placement.position,
                                       placement.color,
                                       placement.radius,
                                       placement.intensity,
                                       placement.type,
                                       placement.direction,
                                       placement.innerConeDegrees,
                                       placement.outerConeDegrees,
                                       placement.castsShadows);
        builder.attachNodeId(entity, placement.nodeId);
        if (!placement.behaviors.empty()) {
            builder.attachBehaviors(entity, placement.behaviors);
        }
    }

    for (const auto& placement : level.colliders) {
        builder.addCollider(placement);
    }

    for (const auto& placement : level.reflectionProbes) {
        auto entity = builder.addReflectionProbe(placement.position,
                                                 placement.extents,
                                                 placement.blendDistance,
                                                 placement.intensity,
                                                 placement.boxProjection);
        builder.attachNodeId(entity, placement.nodeId);
    }

    for (const auto& placement : level.archetypes) {
        const GameplayArchetypeDefinition* archetype = content.findArchetype(placement.archetypeId);
        if (archetype == nullptr) {
            continue;
        }
        (void)spawnGameplayPrefab(builder, instantiateGameplayArchetype(*archetype, placement.position, placement.yawDegrees));
    }

    // Build NodeIndex from all entities that received a NodeIdComponent
    // This must happen AFTER all placement loops (including scripted geometry)
    {
        NodeIndex nodeIndex;
        auto nodeView = registry.view<NodeIdComponent>();
        for (auto entity : nodeView) {
            const auto& nodeComp = nodeView.get<NodeIdComponent>(entity);
            nodeIndex.add(nodeComp.nodeId, entity);
        }
        registry.ctx().insert_or_assign<NodeIndex>(std::move(nodeIndex));
    }

    if (session.currentLevelId != request.levelId) {
        session.currentLevelId = request.levelId;
        if (level.hasPlayerSpawn) {
            session.respawnPosition = level.playerSpawn.position;
            session.fallRespawnY = level.playerSpawn.fallRespawnY;
        }
    } else if (level.hasPlayerSpawn && session.respawnPosition == glm::vec3(0.0f)) {
        session.respawnPosition = level.playerSpawn.position;
        session.fallRespawnY = level.playerSpawn.fallRespawnY;
    }

    const glm::vec3 defaultSpawn = level.hasPlayerSpawn ? level.playerSpawn.position : glm::vec3(0.0f, 1.6f, 5.4f);
    const glm::vec3 spawnPosition = session.respawnPosition == glm::vec3(0.0f) ? defaultSpawn : session.respawnPosition;
    const float fallRespawnY = level.hasPlayerSpawn ? level.playerSpawn.fallRespawnY : session.fallRespawnY;

    auto player = builder.createTransformEntity(spawnPosition);
    registry.emplace<PlayerTag>(player);
    registry.emplace<ControllableTag>(player);
    registry.emplace<PrimaryCameraTag>(player);
    registry.emplace<CameraComponent>(player);
    registry.emplace<CharacterControllerComponent>(player);
    registry.emplace<PlayerMovementComponent>(player);
    registry.emplace<PlayerInteractionLockComponent>(player);
    registry.emplace<PlayerSpawnComponent>(player, PlayerSpawnComponent{session.respawnPosition, fallRespawnY});
}
