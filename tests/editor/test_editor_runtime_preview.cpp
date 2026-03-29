#include "editor/core/EditorRuntimePreviewSession.h"
#include "editor/scene/EditorSceneDocument.h"
#include "engine/core/Window.h"
#include "game/components/CameraComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/ui/InventoryMenuState.h"

#include <entt/entt.hpp>
#include <glm/geometric.hpp>

#include <cassert>
#include <cmath>

namespace {

constexpr float kEpsilon = 0.0001f;

bool nearlyEqual(float a, float b) {
    return std::abs(a - b) <= kEpsilon;
}

} // namespace

int main() {
    Window window(320, 200, "test_editor_runtime_preview");

    ContentRegistry content;
    content.loadDefaults();

    EditorSceneDocument document;
    document.loadFromSceneFile(SILOS_CLOISTER_SCENE_FILE, content);

    EditorRuntimePreviewSession preview;
    preview.rebuild(document, content);

    const RuntimeSessionPerformanceStats& rebuildStats = preview.performanceStats();
    assert(rebuildStats.rebuildMs > 0.0);

    preview.prewarmRenderer(content);
    const RuntimeSessionPerformanceStats& prewarmStats = preview.performanceStats();
    assert(prewarmStats.rendererInitMs > 0.0);
    assert(prewarmStats.rendererPrewarmMs > 0.0);

    preview.render(1.0f / 60.0f, 320, 180, 320, 180, 0);
    const RuntimeSessionPerformanceStats& firstRenderStats = preview.performanceStats();
    assert(firstRenderStats.lastRenderMs > 0.0);

    auto playerView = preview.registry().view<TransformComponent,
                                              CameraComponent,
                                              PlayerMovementComponent,
                                              PlayerInteractionLockComponent,
                                              PlayerSpawnComponent,
                                              PlayerTag>();
    assert(playerView.begin() != playerView.end());
    const entt::entity player = *playerView.begin();

    const TransformComponent baselineTransform = playerView.get<TransformComponent>(player);
    const CameraComponent baselineCamera = playerView.get<CameraComponent>(player);
    const PlayerMovementComponent baselineMovement = playerView.get<PlayerMovementComponent>(player);
    const PlayerInteractionLockComponent baselineLock = playerView.get<PlayerInteractionLockComponent>(player);
    const PlayerSpawnComponent baselineSpawn = playerView.get<PlayerSpawnComponent>(player);
    const RunSession baselineSession = preview.runSession();

    auto checkpointView = preview.registry().view<CheckpointComponent>();
    const bool hasCheckpoint = checkpointView.begin() != checkpointView.end();
    CheckpointComponent baselineCheckpoint{};
    entt::entity checkpointEntity = entt::null;
    if (hasCheckpoint) {
        checkpointEntity = *checkpointView.begin();
        baselineCheckpoint = checkpointView.get<CheckpointComponent>(checkpointEntity);
    }

    document.environment().post.fogDensity += 0.013f;
    document.environment().sky.sunGlow = 0.27f;
    document.markEnvironmentDirty();
    preview.syncEnvironment(document);

    assert(preview.registry().ctx().contains<RuntimeEnvironmentOverride>());
    const auto& environmentOverride = preview.registry().ctx().get<RuntimeEnvironmentOverride>().definition;
    assert(nearlyEqual(environmentOverride.post.fogDensity, document.environment().post.fogDensity));
    assert(nearlyEqual(environmentOverride.sky.sunGlow, document.environment().sky.sunGlow));

    preview.registry().get<TransformComponent>(player).position += glm::vec3(2.0f, 0.0f, -1.0f);
    preview.registry().get<CameraComponent>(player).yaw += 21.0f;
    preview.registry().get<CameraComponent>(player).pitch -= 9.0f;
    preview.registry().get<PlayerMovementComponent>(player).velocity = glm::vec3(3.0f, -1.0f, 2.0f);
    preview.registry().get<PlayerInteractionLockComponent>(player).active = true;
    preview.registry().get<PlayerInteractionLockComponent>(player).remainingTime = 1.25f;
    preview.registry().get<PlayerSpawnComponent>(player).respawnPosition = glm::vec3(5.0f, 2.0f, -3.0f);
    preview.runSession().respawnPosition = glm::vec3(5.0f, 2.0f, -3.0f);
    preview.runSession().fallRespawnY = -42.0f;

    assert(preview.registry().ctx().contains<InventoryMenuState>());
    preview.registry().ctx().get<InventoryMenuState>().open = true;

    if (hasCheckpoint) {
        preview.registry().get<CheckpointComponent>(checkpointEntity).active = !baselineCheckpoint.active;
    }

    preview.resetForPlay();
    const RuntimeSessionPerformanceStats& resetStats = preview.performanceStats();
    assert(resetStats.resetForPlayMs > 0.0);

    const auto& resetTransform = preview.registry().get<TransformComponent>(player);
    const auto& resetCamera = preview.registry().get<CameraComponent>(player);
    const auto& resetMovement = preview.registry().get<PlayerMovementComponent>(player);
    const auto& resetLock = preview.registry().get<PlayerInteractionLockComponent>(player);
    const auto& resetSpawn = preview.registry().get<PlayerSpawnComponent>(player);

    assert(glm::length(resetTransform.position - baselineTransform.position) <= kEpsilon);
    assert(nearlyEqual(resetCamera.yaw, baselineCamera.yaw));
    assert(nearlyEqual(resetCamera.pitch, baselineCamera.pitch));
    assert(glm::length(resetMovement.velocity - baselineMovement.velocity) <= kEpsilon);
    assert(resetLock.active == baselineLock.active);
    assert(nearlyEqual(resetLock.remainingTime, baselineLock.remainingTime));
    assert(glm::length(resetSpawn.respawnPosition - baselineSpawn.respawnPosition) <= kEpsilon);
    assert(nearlyEqual(resetSpawn.fallRespawnY, baselineSpawn.fallRespawnY));
    assert(glm::length(preview.runSession().respawnPosition - baselineSession.respawnPosition) <= kEpsilon);
    assert(nearlyEqual(preview.runSession().fallRespawnY, baselineSession.fallRespawnY));
    assert(preview.registry().ctx().contains<InventoryMenuState>());
    assert(!preview.registry().ctx().get<InventoryMenuState>().open);

    assert(preview.registry().ctx().contains<RuntimeEnvironmentOverride>());
    const auto& resetOverride = preview.registry().ctx().get<RuntimeEnvironmentOverride>().definition;
    assert(nearlyEqual(resetOverride.post.fogDensity, document.environment().post.fogDensity));
    assert(nearlyEqual(resetOverride.sky.sunGlow, document.environment().sky.sunGlow));

    if (hasCheckpoint) {
        assert(preview.registry().get<CheckpointComponent>(checkpointEntity).active == baselineCheckpoint.active);
    }

    preview.render(1.0f / 60.0f, 320, 180, 320, 180, 0);
    assert(preview.performanceStats().lastRenderMs > 0.0);
    return 0;
}
