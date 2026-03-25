#include "game/levels/cathedral/CathedralLayout.h"

#include "game/levels/cathedral/CathedralPrefabs.h"
#include "game/levels/cathedral/CathedralSceneData.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/TransformComponent.h"
#include "game/components/ViewmodelComponent.h"

#include <array>

void buildCathedralLayout(LevelBuildContext& context) {
    auto& registry = context.registry;
    LevelBuilder builder(context);

    Mesh* cube = builder.mesh("cube");
    Mesh* plane = builder.mesh("plane");

    constexpr float roomWidth = 18.0f;
    constexpr float roomDepth = 28.0f;
    constexpr float roomHeight = 9.0f;
    constexpr float wallThickness = 0.7f;
    constexpr float frontZ = 7.5f;
    constexpr float backZ = frontZ - roomDepth;
    constexpr float halfW = roomWidth * 0.5f;
    constexpr float centerZ = (frontZ + backZ) * 0.5f;
    constexpr float portalHalfW = 2.35f;
    constexpr float portalHeight = 6.3f;
    constexpr float pillarRadius = 0.42f;
    constexpr float corridorWidth = 4.8f;
    constexpr float corridorLength = 8.0f;
    constexpr float chamberWidth = 13.0f;
    constexpr float chamberDepth = 15.0f;
    const float corridorCenterZ = backZ - corridorLength * 0.5f;
    const float chamberCenterZ = backZ - corridorLength - chamberDepth * 0.5f;
    const CathedralSceneData sceneData = loadCathedralSceneData("assets/scenes/cathedral.scene");

    builder.addMesh(plane, glm::vec3(0.0f, 0.0f, centerZ), glm::vec3(roomWidth, 1.0f, roomDepth));
    builder.addMesh(plane, glm::vec3(0.0f, 0.04f, centerZ - 1.0f), glm::vec3(6.5f, 1.0f, roomDepth - 4.5f));
    builder.addMesh(plane, glm::vec3(0.0f, roomHeight, centerZ), glm::vec3(roomWidth, -1.0f, roomDepth));

    for (float z = frontZ - 1.5f; z > backZ + 1.5f; z -= 2.45f) {
        builder.addMesh(cube, glm::vec3(0.0f, 0.02f, z), glm::vec3(roomWidth - 1.2f, 0.03f, 0.08f));
    }
    for (float x = -halfW + 2.2f; x < halfW - 1.0f; x += 2.7f) {
        builder.addMesh(cube, glm::vec3(x, 0.02f, centerZ - 0.6f), glm::vec3(0.08f, 0.03f, roomDepth - 3.6f));
    }

    builder.addMesh(cube, glm::vec3(-halfW, roomHeight * 0.5f, centerZ), glm::vec3(wallThickness, roomHeight, roomDepth));
    builder.addMesh(cube, glm::vec3(halfW, roomHeight * 0.5f, centerZ), glm::vec3(wallThickness, roomHeight, roomDepth));
    builder.addMesh(cube, glm::vec3(-(halfW - 2.2f), roomHeight * 0.5f, frontZ), glm::vec3(3.2f, roomHeight, wallThickness));
    builder.addMesh(cube, glm::vec3(halfW - 2.2f, roomHeight * 0.5f, frontZ), glm::vec3(3.2f, roomHeight, wallThickness));
    builder.addMesh(cube, glm::vec3(0.0f, roomHeight - 0.6f, frontZ), glm::vec3(6.2f, 1.2f, wallThickness));

    const float rearWingWidth = (roomWidth - portalHalfW * 2.0f) * 0.5f;
    builder.addMesh(cube, glm::vec3(-(portalHalfW + rearWingWidth * 0.5f), roomHeight * 0.5f, backZ), glm::vec3(rearWingWidth, roomHeight, wallThickness));
    builder.addMesh(cube, glm::vec3(portalHalfW + rearWingWidth * 0.5f, roomHeight * 0.5f, backZ), glm::vec3(rearWingWidth, roomHeight, wallThickness));
    builder.addMesh(cube, glm::vec3(0.0f, portalHeight + (roomHeight - portalHeight) * 0.5f, backZ), glm::vec3(portalHalfW * 2.0f, roomHeight - portalHeight, wallThickness));

    builder.addMesh(cube, glm::vec3(-(portalHalfW - 0.15f), portalHeight * 0.5f, backZ - 1.15f), glm::vec3(0.35f, portalHeight, 2.3f));
    builder.addMesh(cube, glm::vec3(portalHalfW - 0.15f, portalHeight * 0.5f, backZ - 1.15f), glm::vec3(0.35f, portalHeight, 2.3f));
    builder.addMesh(cube, glm::vec3(0.0f, portalHeight + 0.45f, backZ - 1.15f), glm::vec3(portalHalfW * 2.0f, 0.9f, 2.3f));

    builder.addMesh(plane, glm::vec3(0.0f, -0.18f, corridorCenterZ), glm::vec3(corridorWidth, 1.0f, corridorLength + 1.6f));
    builder.addMesh(plane, glm::vec3(0.0f, roomHeight - 0.35f, corridorCenterZ), glm::vec3(corridorWidth, -1.0f, corridorLength + 1.6f));
    builder.addMesh(cube, glm::vec3(-corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ), glm::vec3(0.35f, roomHeight - 0.5f, corridorLength + 1.2f));
    builder.addMesh(cube, glm::vec3(corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ), glm::vec3(0.35f, roomHeight - 0.5f, corridorLength + 1.2f));

    for (int i = 0; i < 3; ++i) {
        const float y = -0.05f - 0.12f * static_cast<float>(i);
        const float z = backZ - 1.8f - 1.05f * static_cast<float>(i);
        const glm::vec3 center(0.0f, y, z);
        const glm::vec3 scale(corridorWidth - 0.25f, 0.14f, 1.1f);
        builder.addMesh(cube, center, scale);
        builder.addBoxCollider(center, scale * 0.5f);
    }

    builder.addMesh(plane, glm::vec3(0.0f, -0.38f, chamberCenterZ), glm::vec3(chamberWidth, 1.0f, chamberDepth));
    builder.addMesh(plane, glm::vec3(0.0f, roomHeight - 0.55f, chamberCenterZ), glm::vec3(chamberWidth, -1.0f, chamberDepth));
    builder.addMesh(cube, glm::vec3(-chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ), glm::vec3(0.5f, roomHeight - 0.8f, chamberDepth));
    builder.addMesh(cube, glm::vec3(chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ), glm::vec3(0.5f, roomHeight - 0.8f, chamberDepth));
    builder.addMesh(cube, glm::vec3(0.0f, roomHeight * 0.42f, chamberCenterZ - chamberDepth * 0.5f), glm::vec3(chamberWidth, roomHeight - 0.8f, 0.5f));

    for (float side : {-1.0f, 1.0f}) {
        builder.addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 0.8f), 2.2f, chamberCenterZ - 2.0f), glm::vec3(0.22f, 2.5f, 2.0f));
        builder.addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 0.8f), 2.0f, chamberCenterZ + 3.0f), glm::vec3(0.22f, 2.2f, 1.8f));
        builder.addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 1.4f), 0.3f, chamberCenterZ - 2.1f), glm::vec3(0.9f, 0.35f, 1.4f), glm::vec3(0.0f, side * 12.0f, 0.0f));
    }

    spawnCathedralCheckpointShrineGeometry(builder, chamberCenterZ);

    const std::array<float, 3> shrineRows = {2.0f, -6.1f, -14.0f};
    for (float side : {-1.0f, 1.0f}) {
        for (float z : shrineRows) {
            spawnCathedralSideShrineBay(builder, side, halfW, z);
        }
    }

    struct StepSpec {
        glm::vec3 center;
        glm::vec3 scale;
    };
    const std::array<StepSpec, 4> steps = {
        StepSpec{glm::vec3(0.0f, 0.12f, backZ + 4.0f), glm::vec3(6.8f, 0.24f, 1.2f)},
        StepSpec{glm::vec3(0.0f, 0.30f, backZ + 2.8f), glm::vec3(6.2f, 0.18f, 1.1f)},
        StepSpec{glm::vec3(0.0f, 0.47f, backZ + 1.7f), glm::vec3(5.6f, 0.16f, 1.0f)},
        StepSpec{glm::vec3(0.0f, 0.67f, backZ + 0.4f), glm::vec3(5.0f, 0.24f, 1.4f)},
    };
    for (const StepSpec& step : steps) {
        spawnCathedralDaisStep(builder, step.center, step.scale);
    }

    for (const auto& placement : sceneData.meshes) {
        builder.addMesh(placement.meshName, placement.position, placement.scale, placement.rotation);
    }

    for (const auto& placement : sceneData.lights) {
        builder.addLight(placement.position, placement.color, placement.radius, placement.intensity);
    }

    for (const auto& placement : sceneData.boxColliders) {
        builder.addBoxCollider(placement.position, placement.halfExtents);
    }
    for (const auto& placement : sceneData.cylinderColliders) {
        builder.addCylinderCollider(placement.position, placement.radius, placement.halfHeight);
    }

    for (const auto& prefab : sceneData.prefabs) {
        (void)spawnGameplayPrefab(builder, prefab);
    }

    auto player = builder.createTransformEntity(
        sceneData.hasPlayerSpawn ? sceneData.playerSpawn.position : glm::vec3(0.0f, 1.6f, 5.4f)
    );
    const glm::vec3 playerSpawnPosition = sceneData.hasPlayerSpawn
        ? sceneData.playerSpawn.position
        : glm::vec3(0.0f, 1.6f, 5.4f);
    const float playerFallRespawnY = sceneData.hasPlayerSpawn
        ? sceneData.playerSpawn.fallRespawnY
        : -8.0f;
    registry.emplace<CameraComponent>(player);
    registry.emplace<CharacterControllerComponent>(player);
    registry.emplace<PlayerMovementComponent>(player);
    registry.emplace<PlayerInteractionLockComponent>(player);
    registry.emplace<PlayerSpawnComponent>(player, PlayerSpawnComponent{playerSpawnPosition, playerFallRespawnY});

    auto viewmodel = builder.createEntity();
    registry.emplace<MeshComponent>(viewmodel, MeshComponent{builder.mesh("hand"), glm::mat4(1.0f), false});
    registry.emplace<ViewmodelComponent>(viewmodel);
}
