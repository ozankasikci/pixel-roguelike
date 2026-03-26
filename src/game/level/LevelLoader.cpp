#include "game/level/LevelLoader.h"

#include "game/content/ContentRegistry.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"
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
#include "game/components/ViewmodelComponent.h"
#include "game/rendering/RetroPalette.h"

#include "engine/core/Application.h"
#include "engine/core/PathUtils.h"

LevelLoader::LevelLoader(LevelBuildContext& context)
    : context_(context) {}

void LevelLoader::load(Application& app, const LevelLoadRequest& request) {
    auto& registry = context_.registry;
    auto& session = app.getService<RunSession>();
    auto& content = app.getService<ContentRegistry>();

    if (request.registerAssets) {
        request.registerAssets(context_.meshLibrary);
    }
    registry.ctx().insert_or_assign(MeshAssetProvider{&context_.meshLibrary});

    LevelBuilder builder(context_);
    if (request.buildScriptedGeometry) {
        request.buildScriptedGeometry(builder);
    }

    const LevelDef level = loadLevelDef(resolveProjectPath(request.levelPath));

    for (const auto& placement : level.meshes) {
        builder.addMesh(placement.meshId, placement.position, placement.scale, placement.rotation, placement.tint, placement.material);
    }

    for (const auto& placement : level.lights) {
        builder.addLight(placement.position, placement.color, placement.radius, placement.intensity);
    }

    for (const auto& placement : level.boxColliders) {
        builder.addBoxCollider(placement.position, placement.halfExtents);
    }

    for (const auto& placement : level.cylinderColliders) {
        builder.addCylinderCollider(placement.position, placement.radius, placement.halfHeight);
    }

    for (const auto& placement : level.archetypes) {
        const GameplayArchetypeDefinition* archetype = content.findArchetype(placement.archetypeId);
        if (archetype == nullptr) {
            continue;
        }
        (void)spawnGameplayPrefab(builder, instantiateGameplayArchetype(*archetype, placement.position, placement.yawDegrees));
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

    auto viewmodel = builder.createEntity();
    registry.emplace<MeshComponent>(
        viewmodel,
        MeshComponent{"hand", context_.meshLibrary.get("hand"), glm::mat4(1.0f), false, RetroPalette::Bone, MaterialKind::Viewmodel}
    );
    registry.emplace<ViewmodelComponent>(viewmodel);
}
