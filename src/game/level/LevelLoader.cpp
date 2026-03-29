#include "game/level/LevelLoader.h"

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
#include "game/components/ScriptAttachmentComponent.h"
#include "game/components/ScriptHostComponent.h"
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

namespace {

entt::entity spawnViewmodelMesh(entt::registry& registry,
                                MeshLibrary& meshLibrary,
                                const std::string& meshId,
                                const glm::vec3& tint,
                                MaterialKind material,
                                const glm::vec3& viewOffset,
                                const glm::vec3& rotation,
                                const glm::vec3& scale,
                                const glm::vec3& meshCenter = glm::vec3(0.0f),
                                float bobAmplitude = 0.002f) {
    auto entity = registry.create();
    registry.emplace<MeshComponent>(
        entity,
        MeshComponent{
            meshId,
            meshLibrary.get(meshId),
            glm::mat4(1.0f),
            false,
            tint,
            std::string(defaultMaterialIdForKind(material))
        }
    );

    ViewmodelComponent viewmodel;
    viewmodel.viewOffset = viewOffset;
    viewmodel.rotation = rotation;
    viewmodel.scale = scale;
    viewmodel.meshCenter = meshCenter;
    viewmodel.bobAmplitude = bobAmplitude;
    registry.emplace<ViewmodelComponent>(entity, viewmodel);
    return entity;
}

template <typename AttachmentT>
std::vector<ScriptAttachment> applyScriptOverrides(const std::vector<ScriptAttachment>& baseScripts,
                                                   const std::vector<AttachmentT>& overrides) {
    std::vector<ScriptAttachment> resolved = baseScripts;
    for (const auto& overrideAttachment : overrides) {
        for (auto& attachment : resolved) {
            if (attachment.scriptId != overrideAttachment.scriptId) {
                continue;
            }
            attachment.enabled = overrideAttachment.enabled;
            for (const auto& [propertyName, propertyValue] : overrideAttachment.propertyValues) {
                attachment.propertyValues[propertyName] = propertyValue;
            }
        }
    }
    return resolved;
}

void attachScriptHost(entt::registry& registry,
                      entt::entity entity,
                      ScriptHostKind kind,
                      const std::string& nodeId,
                      const std::vector<ScriptAttachment>& attachments) {
    if (entity == entt::null || nodeId.empty()) {
        return;
    }
    registry.emplace_or_replace<ScriptHostComponent>(entity, ScriptHostComponent{nodeId, kind});
    if (!attachments.empty()) {
        registry.emplace_or_replace<ScriptAttachmentComponent>(entity, ScriptAttachmentComponent{attachments});
    }
}

} // namespace

void LevelLoader::load(Application& app, const LevelLoadRequest& request) {
    load(app.getService<ContentRegistry>(),
         app.getService<RunSession>(),
         request,
         loadLevelDef(resolveProjectPath(request.levelPath)));
}

void LevelLoader::load(ContentRegistry& content,
                       RunSession& session,
                       const LevelLoadRequest& request,
                       const LevelDef& rawLevel) {
    auto& registry = context_.registry;

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
        entt::entity entity = builder.addMesh(placement.meshId,
                                              placement.position,
                                              placement.scale,
                                              placement.rotation,
                                              placement.tint,
                                              placement.material,
                                              placement.materialId.empty()
                                                  ? std::optional<std::string>{}
                                                  : std::optional<std::string>{placement.materialId});
        attachScriptHost(registry, entity, ScriptHostKind::Mesh, placement.nodeId, placement.scripts);
    }

    for (const auto& placement : level.lights) {
        entt::entity entity = builder.addLight(placement.position,
                                               placement.color,
                                               placement.radius,
                                               placement.intensity,
                                               placement.type,
                                               placement.direction,
                                               placement.innerConeDegrees,
                                               placement.outerConeDegrees,
                                               placement.castsShadows);
        attachScriptHost(registry, entity, ScriptHostKind::Light, placement.nodeId, placement.scripts);
    }

    for (const auto& placement : level.boxColliders) {
        entt::entity entity = builder.addBoxCollider(placement.position, placement.halfExtents, placement.rotation);
        attachScriptHost(registry, entity, ScriptHostKind::BoxCollider, placement.nodeId, placement.scripts);
    }

    for (const auto& placement : level.cylinderColliders) {
        entt::entity entity = builder.addCylinderCollider(placement.position, placement.radius, placement.halfHeight, placement.rotation);
        attachScriptHost(registry, entity, ScriptHostKind::CylinderCollider, placement.nodeId, placement.scripts);
    }

    for (const auto& placement : level.archetypes) {
        const GameplayArchetypeDefinition* archetype = content.findArchetype(placement.archetypeId);
        if (archetype == nullptr) {
            continue;
        }
        entt::entity root = spawnGameplayPrefab(builder, instantiateGameplayArchetype(*archetype, placement.position, placement.yawDegrees));
        attachScriptHost(registry,
                         root,
                         ScriptHostKind::Archetype,
                         placement.nodeId,
                         applyScriptOverrides(archetype->scripts, placement.scriptOverrides));
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

    spawnViewmodelMesh(
        registry,
        context_.meshLibrary,
        "hand",
        RetroPalette::Bone,
        MaterialKind::Viewmodel,
        glm::vec3(0.090f, -0.255f, -0.395f),
        glm::vec3(-40.5f, -53.5f, 215.5f),
        glm::vec3(0.003f),
        glm::vec3(-136.5f, -363.5f, -0.6f)
    );

    spawnViewmodelMesh(
        registry,
        context_.meshLibrary,
        "cylinder",
        RetroPalette::OldWoodDark,
        MaterialKind::Wood,
        glm::vec3(-0.225f, -0.245f, -0.365f),
        glm::vec3(14.0f, -18.0f, -26.0f),
        glm::vec3(0.022f, 0.165f, 0.022f),
        glm::vec3(0.0f),
        0.0012f
    );

    spawnViewmodelMesh(
        registry,
        context_.meshLibrary,
        "cube",
        glm::vec3(0.97f, 0.55f, 0.16f),
        MaterialKind::Wax,
        glm::vec3(-0.182f, -0.118f, -0.347f),
        glm::vec3(10.0f, -16.0f, -22.0f),
        glm::vec3(0.055f, 0.072f, 0.055f),
        glm::vec3(0.0f),
        0.0010f
    );

    spawnViewmodelMesh(
        registry,
        context_.meshLibrary,
        "cube",
        glm::vec3(1.00f, 0.83f, 0.40f),
        MaterialKind::Wax,
        glm::vec3(-0.174f, -0.050f, -0.338f),
        glm::vec3(4.0f, -10.0f, -18.0f),
        glm::vec3(0.026f, 0.034f, 0.026f),
        glm::vec3(0.0f),
        0.0014f
    );
}
