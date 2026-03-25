#include "game/levels/cathedral/CathedralLayout.h"

#include "game/levels/cathedral/CathedralBuilder.h"
#include "game/levels/cathedral/CathedralPrefabs.h"
#include "engine/rendering/MeshGeometry.h"
#include "game/levels/cathedral/CathedralSceneData.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/components/ViewmodelComponent.h"

#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace {

glm::mat4 makeModel(const glm::vec3& position,
                    const glm::vec3& scale,
                    const glm::vec3& rotation = glm::vec3(0.0f)) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

entt::entity spawnMesh(entt::registry& registry, Mesh* mesh, const glm::mat4& model) {
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity);
    registry.emplace<MeshComponent>(entity, MeshComponent{mesh, model, true});
    return entity;
}

entt::entity addColliderBox(entt::registry& registry,
                            std::vector<entt::entity>& entities,
                            glm::vec3 position,
                            glm::vec3 halfExtents) {
    auto e = registry.create();
    StaticColliderComponent sc;
    sc.shape = ColliderShape::Box;
    sc.position = position;
    sc.halfExtents = halfExtents;
    registry.emplace<StaticColliderComponent>(e, sc);
    entities.push_back(e);
    return e;
}

entt::entity addColliderCylinder(entt::registry& registry,
                                 std::vector<entt::entity>& entities,
                                 glm::vec3 position,
                                 float radius,
                                 float halfHeight) {
    auto e = registry.create();
    StaticColliderComponent sc;
    sc.shape = ColliderShape::Cylinder;
    sc.position = position;
    sc.radius = radius;
    sc.halfHeight = halfHeight;
    registry.emplace<StaticColliderComponent>(e, sc);
    entities.push_back(e);
    return e;
}

entt::entity addLightEntity(entt::registry& registry,
                            std::vector<entt::entity>& entities,
                            const glm::vec3& position,
                            const glm::vec3& color,
                            float radius,
                            float intensity) {
    auto e = registry.create();
    registry.emplace<TransformComponent>(e, TransformComponent{position});
    registry.emplace<LightComponent>(e, LightComponent{color, radius, intensity});
    entities.push_back(e);
    return e;
}

} // namespace

void buildCathedralLayout(CathedralContext& context) {
    auto& registry = context.registry;
    auto& meshLibrary = context.meshLibrary;
    auto& entities = context.entities;
    CathedralBuilder builder(context);

    Mesh* cube = meshLibrary.get("cube");
    Mesh* leftDoorMesh = meshLibrary.get("door_leaf_left");
    Mesh* rightDoorMesh = meshLibrary.get("door_leaf_right");
    Mesh* plane = meshLibrary.get("plane");
    Mesh* cylinder = meshLibrary.get("cylinder");
    Mesh* pillarMesh = meshLibrary.get("pillar");
    Mesh* archMesh = meshLibrary.get("arch");

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

    auto addMesh = [&](Mesh* mesh,
                       const glm::vec3& position,
                       const glm::vec3& scale,
                       const glm::vec3& rotation = glm::vec3(0.0f)) {
        entities.push_back(spawnMesh(registry, mesh, makeModel(position, scale, rotation)));
    };

    auto addLight = [&](const glm::vec3& position,
                        const glm::vec3& color,
                        float radius,
                        float intensity) {
        (void)addLightEntity(registry, entities, position, color, radius, intensity);
    };

    addMesh(plane, glm::vec3(0.0f, 0.0f, centerZ), glm::vec3(roomWidth, 1.0f, roomDepth));
    addMesh(plane, glm::vec3(0.0f, 0.04f, centerZ - 1.0f), glm::vec3(6.5f, 1.0f, roomDepth - 4.5f));
    addMesh(plane, glm::vec3(0.0f, roomHeight, centerZ), glm::vec3(roomWidth, -1.0f, roomDepth));

    for (float z = frontZ - 1.5f; z > backZ + 1.5f; z -= 2.45f) {
        addMesh(cube, glm::vec3(0.0f, 0.02f, z), glm::vec3(roomWidth - 1.2f, 0.03f, 0.08f));
    }
    for (float x = -halfW + 2.2f; x < halfW - 1.0f; x += 2.7f) {
        addMesh(cube, glm::vec3(x, 0.02f, centerZ - 0.6f), glm::vec3(0.08f, 0.03f, roomDepth - 3.6f));
    }

    addMesh(cube, glm::vec3(-halfW, roomHeight * 0.5f, centerZ), glm::vec3(wallThickness, roomHeight, roomDepth));
    addMesh(cube, glm::vec3(halfW, roomHeight * 0.5f, centerZ), glm::vec3(wallThickness, roomHeight, roomDepth));
    addMesh(cube, glm::vec3(-(halfW - 2.2f), roomHeight * 0.5f, frontZ), glm::vec3(3.2f, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(halfW - 2.2f, roomHeight * 0.5f, frontZ), glm::vec3(3.2f, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(0.0f, roomHeight - 0.6f, frontZ), glm::vec3(6.2f, 1.2f, wallThickness));

    const float rearWingWidth = (roomWidth - portalHalfW * 2.0f) * 0.5f;
    addMesh(cube, glm::vec3(-(portalHalfW + rearWingWidth * 0.5f), roomHeight * 0.5f, backZ), glm::vec3(rearWingWidth, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(portalHalfW + rearWingWidth * 0.5f, roomHeight * 0.5f, backZ), glm::vec3(rearWingWidth, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(0.0f, portalHeight + (roomHeight - portalHeight) * 0.5f, backZ), glm::vec3(portalHalfW * 2.0f, roomHeight - portalHeight, wallThickness));

    addMesh(cube, glm::vec3(-(portalHalfW - 0.15f), portalHeight * 0.5f, backZ - 1.15f), glm::vec3(0.35f, portalHeight, 2.3f));
    addMesh(cube, glm::vec3(portalHalfW - 0.15f, portalHeight * 0.5f, backZ - 1.15f), glm::vec3(0.35f, portalHeight, 2.3f));
    addMesh(cube, glm::vec3(0.0f, portalHeight + 0.45f, backZ - 1.15f), glm::vec3(portalHalfW * 2.0f, 0.9f, 2.3f));

    addMesh(plane, glm::vec3(0.0f, -0.18f, corridorCenterZ), glm::vec3(corridorWidth, 1.0f, corridorLength + 1.6f));
    addMesh(plane, glm::vec3(0.0f, roomHeight - 0.35f, corridorCenterZ), glm::vec3(corridorWidth, -1.0f, corridorLength + 1.6f));
    addMesh(cube, glm::vec3(-corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ), glm::vec3(0.35f, roomHeight - 0.5f, corridorLength + 1.2f));
    addMesh(cube, glm::vec3(corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ), glm::vec3(0.35f, roomHeight - 0.5f, corridorLength + 1.2f));

    for (int i = 0; i < 3; ++i) {
        const float y = -0.05f - 0.12f * static_cast<float>(i);
        const float z = backZ - 1.8f - 1.05f * static_cast<float>(i);
        const glm::vec3 center(0.0f, y, z);
        const glm::vec3 scale(corridorWidth - 0.25f, 0.14f, 1.1f);
        addMesh(cube, center, scale);
        addColliderBox(registry, entities, center, scale * 0.5f);
    }

    addMesh(plane, glm::vec3(0.0f, -0.38f, chamberCenterZ), glm::vec3(chamberWidth, 1.0f, chamberDepth));
    addMesh(plane, glm::vec3(0.0f, roomHeight - 0.55f, chamberCenterZ), glm::vec3(chamberWidth, -1.0f, chamberDepth));
    addMesh(cube, glm::vec3(-chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ), glm::vec3(0.5f, roomHeight - 0.8f, chamberDepth));
    addMesh(cube, glm::vec3(chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ), glm::vec3(0.5f, roomHeight - 0.8f, chamberDepth));
    addMesh(cube, glm::vec3(0.0f, roomHeight * 0.42f, chamberCenterZ - chamberDepth * 0.5f), glm::vec3(chamberWidth, roomHeight - 0.8f, 0.5f));

    for (float side : {-1.0f, 1.0f}) {
        addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 0.8f), 2.2f, chamberCenterZ - 2.0f), glm::vec3(0.22f, 2.5f, 2.0f));
        addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 0.8f), 2.0f, chamberCenterZ + 3.0f), glm::vec3(0.22f, 2.2f, 1.8f));
        addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 1.4f), 0.3f, chamberCenterZ - 2.1f), glm::vec3(0.9f, 0.35f, 1.4f), glm::vec3(0.0f, side * 12.0f, 0.0f));
    }

    spawnCathedralCheckpointShrineGeometry(builder, chamberCenterZ);

    auto createDoorLeaf = [&](Mesh* mesh,
                              const glm::vec3& closedCenter,
                              const glm::vec3& hingePosition,
                              const glm::vec3& leafScale,
                              float closedYaw,
                              float openYaw) {
        auto leaf = registry.create();
        registry.emplace<TransformComponent>(leaf);
        registry.emplace<MeshComponent>(leaf, MeshComponent{
            mesh,
            makeModel(closedCenter, leafScale),
            true
        });

        StaticColliderComponent collider;
        collider.shape = ColliderShape::Box;
        collider.position = closedCenter;
        collider.rotation = glm::vec3(0.0f, closedYaw, 0.0f);
        collider.halfExtents = leafScale * 0.5f;
        registry.emplace<StaticColliderComponent>(leaf, collider);

        DoorLeafComponent doorLeaf;
        doorLeaf.hingePosition = hingePosition;
        doorLeaf.centerOffsetFromHinge = closedCenter - hingePosition;
        doorLeaf.closedScale = leafScale;
        doorLeaf.colliderHalfExtents = collider.halfExtents;
        doorLeaf.closedYaw = closedYaw;
        doorLeaf.openYaw = openYaw;
        registry.emplace<DoorLeafComponent>(leaf, doorLeaf);

        entities.push_back(leaf);
        return leaf;
    };

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
        Mesh* mesh = meshLibrary.get(placement.meshName);
        if (mesh == nullptr) {
            spdlog::warn("Cathedral scene data references missing mesh '{}'", placement.meshName);
            continue;
        }
        addMesh(mesh, placement.position, placement.scale, placement.rotation);
    }

    for (const auto& placement : sceneData.lights) {
        addLight(placement.position, placement.color, placement.radius, placement.intensity);
    }

    for (const auto& placement : sceneData.boxColliders) {
        addColliderBox(registry, entities, placement.position, placement.halfExtents);
    }
    for (const auto& placement : sceneData.cylinderColliders) {
        addColliderCylinder(registry, entities, placement.position, placement.radius, placement.halfHeight);
    }

    for (const auto& placement : sceneData.checkpoints) {
        auto checkpointLight = builder.addLight(
            placement.lightPosition,
            placement.lightColor,
            placement.lightRadius,
            placement.lightIntensity
        );

        auto checkpoint = registry.create();
        registry.emplace<TransformComponent>(checkpoint, TransformComponent{placement.position});
        registry.emplace<CheckpointComponent>(
            checkpoint,
            CheckpointComponent{
                placement.respawnPosition,
                placement.interactDistance,
                placement.interactDotThreshold,
                false,
                checkpointLight
            }
        );
        builder.track(checkpoint);
    }

    for (const auto& placement : sceneData.doors) {
        const glm::vec3 leftCenter = placement.leftHingePosition + glm::vec3(placement.leafScale.x * 0.5f, 0.0f, 0.0f);
        const glm::vec3 rightCenter = placement.rightHingePosition - glm::vec3(placement.leafScale.x * 0.5f, 0.0f, 0.0f);

        auto leftLeaf = createDoorLeaf(
            leftDoorMesh,
            leftCenter,
            placement.leftHingePosition,
            placement.leafScale,
            placement.closedYaw,
            placement.closedYaw - placement.openAngle
        );
        auto rightLeaf = createDoorLeaf(
            rightDoorMesh,
            rightCenter,
            placement.rightHingePosition,
            placement.leafScale,
            placement.closedYaw,
            placement.closedYaw + placement.openAngle
        );

        auto doorRoot = registry.create();
        registry.emplace<TransformComponent>(doorRoot, TransformComponent{placement.rootPosition});
        registry.emplace<DoorComponent>(
            doorRoot,
            DoorComponent{
                leftLeaf,
                rightLeaf,
                placement.interactDistance,
                placement.interactDotThreshold,
                placement.openDuration,
                0.0f,
                false,
                false
            }
        );
        builder.track(doorRoot);
    }

    auto player = registry.create();
    const glm::vec3 playerSpawnPosition = sceneData.hasPlayerSpawn
        ? sceneData.playerSpawn.position
        : glm::vec3(0.0f, 1.6f, 5.4f);
    const float playerFallRespawnY = sceneData.hasPlayerSpawn
        ? sceneData.playerSpawn.fallRespawnY
        : -8.0f;
    registry.emplace<TransformComponent>(player, TransformComponent{playerSpawnPosition});
    registry.emplace<CameraComponent>(player);
    registry.emplace<CharacterControllerComponent>(player);
    registry.emplace<PlayerMovementComponent>(player);
    registry.emplace<PlayerInteractionLockComponent>(player);
    registry.emplace<PlayerSpawnComponent>(player, PlayerSpawnComponent{playerSpawnPosition, playerFallRespawnY});
    builder.track(player);

    auto viewmodel = registry.create();
    registry.emplace<MeshComponent>(viewmodel, MeshComponent{meshLibrary.get("hand"), glm::mat4(1.0f), false});
    registry.emplace<ViewmodelComponent>(viewmodel);
    builder.track(viewmodel);
}
