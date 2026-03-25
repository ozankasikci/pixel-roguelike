#include "CathedralScene.h"

#include "engine/core/Application.h"
#include "engine/rendering/MeshGeometry.h"
#include "game/components/TransformComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/CameraComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/ViewmodelComponent.h"

#include <array>
#include <glm/gtc/matrix_transform.hpp>

static glm::mat4 makeModel(const glm::vec3& position,
                           const glm::vec3& scale,
                           const glm::vec3& rotation = glm::vec3(0.0f)) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

static std::unique_ptr<Mesh> createWoodDoorLeafMesh(bool leftLeaf) {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;
    const float hand = leftLeaf ? 1.0f : -1.0f;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Core slab.
    addBox(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.74f));

    // Raised perimeter frame.
    addBox(glm::vec3(0.0f, 0.44f, 0.08f), glm::vec3(0.94f, 0.08f, 0.18f));
    addBox(glm::vec3(0.0f, -0.44f, 0.08f), glm::vec3(0.94f, 0.08f, 0.18f));
    addBox(glm::vec3(-0.43f, 0.0f, 0.08f), glm::vec3(0.08f, 0.84f, 0.18f));
    addBox(glm::vec3(0.43f, 0.0f, 0.08f), glm::vec3(0.08f, 0.84f, 0.18f));

    // Vertical planks on the front face.
    for (float x : {-0.25f, -0.02f, 0.22f}) {
        addBox(glm::vec3(x, 0.0f, 0.12f), glm::vec3(0.18f, 0.82f, 0.10f));
    }

    // Back planks so the leaf still reads like wood when seen from behind.
    for (float x : {-0.28f, -0.04f, 0.20f}) {
        addBox(glm::vec3(x, 0.0f, -0.12f), glm::vec3(0.17f, 0.78f, 0.08f));
    }

    // Heavy iron/brace-like bars and diagonal timber braces.
    addBox(glm::vec3(0.0f, 0.24f, 0.16f), glm::vec3(0.78f, 0.08f, 0.08f));
    addBox(glm::vec3(0.0f, -0.18f, 0.16f), glm::vec3(0.78f, 0.08f, 0.08f));
    addBox(glm::vec3(0.02f * hand, 0.05f, 0.18f), glm::vec3(0.92f, 0.08f, 0.08f), glm::vec3(0.0f, 0.0f, 31.0f * hand));
    addBox(glm::vec3(-0.02f * hand, -0.03f, -0.18f), glm::vec3(0.92f, 0.08f, 0.08f), glm::vec3(0.0f, 0.0f, -31.0f * hand));

    // Center meeting stile with a beveled shadow seam so the two leaves read separately.
    const float meetingEdgeX = 0.46f * hand;
    addBox(glm::vec3(meetingEdgeX, 0.0f, 0.16f), glm::vec3(0.05f, 0.90f, 0.12f));
    addBox(glm::vec3(meetingEdgeX - 0.03f * hand, 0.0f, 0.22f), glm::vec3(0.018f, 0.90f, 0.08f));
    addBox(glm::vec3(meetingEdgeX - 0.055f * hand, 0.0f, 0.10f), glm::vec3(0.012f, 0.86f, 0.06f));

    // Outer hinge-side strap to break up the silhouette further.
    addBox(glm::vec3(-0.39f * hand, 0.0f, 0.18f), glm::vec3(0.06f, 0.84f, 0.10f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.indices);
}

static entt::entity spawnMesh(entt::registry& registry, Mesh* mesh, const glm::mat4& model) {
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity);
    registry.emplace<MeshComponent>(entity, MeshComponent{mesh, model, true});
    return entity;
}

static entt::entity addColliderBox(entt::registry& registry,
                                   std::vector<entt::entity>& entities,
                                   glm::vec3 position, glm::vec3 halfExtents) {
    auto e = registry.create();
    StaticColliderComponent sc;
    sc.shape = ColliderShape::Box;
    sc.position = position;
    sc.halfExtents = halfExtents;
    registry.emplace<StaticColliderComponent>(e, sc);
    entities.push_back(e);
    return e;
}

static entt::entity addColliderCylinder(entt::registry& registry,
                                        std::vector<entt::entity>& entities,
                                        glm::vec3 position, float radius, float halfHeight) {
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

void CathedralScene::onEnter(Application& app) {
    entities_.clear();

    meshLibrary_.registerDefaults();
    meshLibrary_.registerMesh("door_leaf_left", createWoodDoorLeafMesh(true));
    meshLibrary_.registerMesh("door_leaf_right", createWoodDoorLeafMesh(false));
    meshLibrary_.loadFromFile("pillar", "assets/meshes/pillar.glb");
    meshLibrary_.loadFromFile("arch", "assets/meshes/arch.glb");
    meshLibrary_.loadFromFile("hand", "assets/meshes/hand_with_old_dagger.glb");

    entt::registry& registry = app.registry();

    Mesh* cube          = meshLibrary_.get("cube");
    Mesh* leftDoorMesh  = meshLibrary_.get("door_leaf_left");
    Mesh* rightDoorMesh = meshLibrary_.get("door_leaf_right");
    Mesh* plane        = meshLibrary_.get("plane");
    Mesh* cylinder     = meshLibrary_.get("cylinder");
    Mesh* pillarMesh   = meshLibrary_.get("pillar");
    Mesh* archMesh     = meshLibrary_.get("arch");

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
    const glm::vec3 torchColor{1.0f, 0.82f, 0.62f};
    const glm::vec3 moonColor{0.62f, 0.68f, 0.82f};

    auto addMesh = [&](Mesh* mesh,
                       const glm::vec3& position,
                       const glm::vec3& scale,
                       const glm::vec3& rotation = glm::vec3(0.0f)) {
        entities_.push_back(spawnMesh(registry, mesh, makeModel(position, scale, rotation)));
    };

    auto addLight = [&](const glm::vec3& position,
                        const glm::vec3& color,
                        float radius,
                        float intensity) {
        auto e = registry.create();
        registry.emplace<TransformComponent>(e, TransformComponent{position});
        registry.emplace<LightComponent>(e, LightComponent{color, radius, intensity});
        entities_.push_back(e);
    };

    // Floor, raised aisle, and overhead slab.
    addMesh(plane, glm::vec3(0.0f, 0.0f, centerZ), glm::vec3(roomWidth, 1.0f, roomDepth));
    addMesh(plane, glm::vec3(0.0f, 0.04f, centerZ - 1.0f), glm::vec3(6.5f, 1.0f, roomDepth - 4.5f));
    addMesh(plane, glm::vec3(0.0f, roomHeight, centerZ), glm::vec3(roomWidth, -1.0f, roomDepth));

    // Floor seams and broken flagstones.
    for (float z = frontZ - 1.5f; z > backZ + 1.5f; z -= 2.45f) {
        addMesh(cube, glm::vec3(0.0f, 0.02f, z), glm::vec3(roomWidth - 1.2f, 0.03f, 0.08f));
    }
    for (float x = -halfW + 2.2f; x < halfW - 1.0f; x += 2.7f) {
        addMesh(cube, glm::vec3(x, 0.02f, centerZ - 0.6f), glm::vec3(0.08f, 0.03f, roomDepth - 3.6f));
    }

    const std::array<glm::vec3, 6> slabOffsets = {
        glm::vec3(-5.8f, 0.07f, 2.8f),
        glm::vec3(-2.1f, 0.05f, -1.4f),
        glm::vec3(2.6f, 0.06f, -5.0f),
        glm::vec3(5.3f, 0.04f, -8.8f),
        glm::vec3(-4.4f, 0.08f, -11.6f),
        glm::vec3(0.7f, 0.05f, -14.1f),
    };
    const std::array<glm::vec3, 6> slabScales = {
        glm::vec3(2.0f, 0.12f, 1.5f),
        glm::vec3(1.6f, 0.10f, 1.2f),
        glm::vec3(1.8f, 0.11f, 1.4f),
        glm::vec3(1.4f, 0.09f, 1.0f),
        glm::vec3(2.2f, 0.13f, 1.6f),
        glm::vec3(1.5f, 0.10f, 1.1f),
    };
    for (size_t i = 0; i < slabOffsets.size(); ++i) {
        addMesh(cube, slabOffsets[i], slabScales[i], glm::vec3(0.0f, 12.0f * static_cast<float>(i % 3), 0.0f));
    }

    // Side walls and front framing.
    addMesh(cube, glm::vec3(-halfW, roomHeight * 0.5f, centerZ),
            glm::vec3(wallThickness, roomHeight, roomDepth));
    addMesh(cube, glm::vec3(halfW, roomHeight * 0.5f, centerZ),
            glm::vec3(wallThickness, roomHeight, roomDepth));

    addMesh(cube, glm::vec3(-(halfW - 2.2f), roomHeight * 0.5f, frontZ),
            glm::vec3(3.2f, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(halfW - 2.2f, roomHeight * 0.5f, frontZ),
            glm::vec3(3.2f, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(0.0f, roomHeight - 0.6f, frontZ),
            glm::vec3(6.2f, 1.2f, wallThickness));

    // Back wall broken by a tall dark portal.
    const float rearWingWidth = (roomWidth - portalHalfW * 2.0f) * 0.5f;
    addMesh(cube, glm::vec3(-(portalHalfW + rearWingWidth * 0.5f), roomHeight * 0.5f, backZ),
            glm::vec3(rearWingWidth, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(portalHalfW + rearWingWidth * 0.5f, roomHeight * 0.5f, backZ),
            glm::vec3(rearWingWidth, roomHeight, wallThickness));
    addMesh(cube, glm::vec3(0.0f, portalHeight + (roomHeight - portalHeight) * 0.5f, backZ),
            glm::vec3(portalHalfW * 2.0f, roomHeight - portalHeight, wallThickness));

    // Rear tunnel behind the doorway for a deeper silhouette.
    addMesh(cube, glm::vec3(-(portalHalfW - 0.15f), portalHeight * 0.5f, backZ - 1.15f),
            glm::vec3(0.35f, portalHeight, 2.3f));
    addMesh(cube, glm::vec3(portalHalfW - 0.15f, portalHeight * 0.5f, backZ - 1.15f),
            glm::vec3(0.35f, portalHeight, 2.3f));
    addMesh(cube, glm::vec3(0.0f, portalHeight + 0.45f, backZ - 1.15f),
            glm::vec3(portalHalfW * 2.0f, 0.9f, 2.3f));

    // Post-door corridor.
    addMesh(plane, glm::vec3(0.0f, -0.18f, corridorCenterZ), glm::vec3(corridorWidth, 1.0f, corridorLength + 1.6f));
    addMesh(plane, glm::vec3(0.0f, roomHeight - 0.35f, corridorCenterZ), glm::vec3(corridorWidth, -1.0f, corridorLength + 1.6f));
    addMesh(cube, glm::vec3(-corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ),
            glm::vec3(0.35f, roomHeight - 0.5f, corridorLength + 1.2f));
    addMesh(cube, glm::vec3(corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ),
            glm::vec3(0.35f, roomHeight - 0.5f, corridorLength + 1.2f));

    // Descending threshold steps.
    for (int i = 0; i < 3; ++i) {
        const float y = -0.05f - 0.12f * static_cast<float>(i);
        const float z = backZ - 1.8f - 1.05f * static_cast<float>(i);
        const glm::vec3 center(0.0f, y, z);
        const glm::vec3 scale(corridorWidth - 0.25f, 0.14f, 1.1f);
        addMesh(cube, center, scale);
        addColliderBox(registry, entities_, center, scale * 0.5f);
    }

    // Second sanctum beyond the door.
    addMesh(plane, glm::vec3(0.0f, -0.38f, chamberCenterZ), glm::vec3(chamberWidth, 1.0f, chamberDepth));
    addMesh(plane, glm::vec3(0.0f, roomHeight - 0.55f, chamberCenterZ), glm::vec3(chamberWidth, -1.0f, chamberDepth));
    addMesh(cube, glm::vec3(-chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ),
            glm::vec3(0.5f, roomHeight - 0.8f, chamberDepth));
    addMesh(cube, glm::vec3(chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ),
            glm::vec3(0.5f, roomHeight - 0.8f, chamberDepth));
    addMesh(cube, glm::vec3(0.0f, roomHeight * 0.42f, chamberCenterZ - chamberDepth * 0.5f),
            glm::vec3(chamberWidth, roomHeight - 0.8f, 0.5f));

    // Chamber side alcoves.
    for (float side : {-1.0f, 1.0f}) {
        addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 0.8f), 2.2f, chamberCenterZ - 2.0f),
                glm::vec3(0.22f, 2.5f, 2.0f));
        addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 0.8f), 2.0f, chamberCenterZ + 3.0f),
                glm::vec3(0.22f, 2.2f, 1.8f));
        addMesh(cube, glm::vec3(side * (chamberWidth * 0.5f - 1.4f), 0.3f, chamberCenterZ - 2.1f),
                glm::vec3(0.9f, 0.35f, 1.4f), glm::vec3(0.0f, side * 12.0f, 0.0f));
    }

    // Checkpoint shrine.
    const glm::vec3 shrineBase(0.0f, -0.02f, chamberCenterZ - 1.7f);
    addMesh(cube, shrineBase, glm::vec3(2.7f, 0.32f, 2.2f));
    addMesh(cube, shrineBase + glm::vec3(0.0f, 0.32f, 0.0f), glm::vec3(2.1f, 0.12f, 1.7f));
    addMesh(cube, glm::vec3(0.0f, 0.72f, chamberCenterZ - 1.7f), glm::vec3(0.45f, 1.1f, 0.45f));
    addMesh(cube, glm::vec3(0.0f, 1.55f, chamberCenterZ - 1.7f), glm::vec3(0.8f, 0.12f, 0.8f));
    addMesh(cylinder, glm::vec3(0.0f, 1.25f, chamberCenterZ - 1.7f), glm::vec3(0.26f, 0.5f, 0.26f));

    auto checkpointLight = registry.create();
    registry.emplace<TransformComponent>(checkpointLight, TransformComponent{
        glm::vec3(0.0f, 1.95f, chamberCenterZ - 1.7f)
    });
    registry.emplace<LightComponent>(checkpointLight, LightComponent{glm::vec3(1.0f, 0.7f, 0.42f), 7.0f, 1.15f});
    entities_.push_back(checkpointLight);

    auto checkpoint = registry.create();
    registry.emplace<TransformComponent>(checkpoint, TransformComponent{
        glm::vec3(0.0f, 1.3f, chamberCenterZ - 0.7f)
    });
    registry.emplace<CheckpointComponent>(checkpoint, CheckpointComponent{
        glm::vec3(0.0f, 1.6f, chamberCenterZ + 1.8f),
        2.8f,
        0.45f,
        false,
        checkpointLight
    });
    entities_.push_back(checkpoint);

    // Cross-room arches and clustered pillars.
    const std::array<float, 4> archRows = {4.4f, -2.9f, -10.2f, -16.8f};
    for (float z : archRows) {
        addMesh(archMesh, glm::vec3(0.0f, 0.0f, z), glm::vec3(1.2f, 1.15f, 1.0f));

        for (float side : {-1.0f, 1.0f}) {
            const float x = side * 3.45f;
            addMesh(pillarMesh, glm::vec3(x, 0.0f, z), glm::vec3(1.0f));
            addColliderCylinder(registry, entities_,
                glm::vec3(x, roomHeight * 0.5f, z), pillarRadius, roomHeight * 0.5f);
        }
    }

    const std::array<glm::vec2, 4> cornerPillars = {
        glm::vec2(-6.6f, 3.4f),
        glm::vec2(6.6f, 3.4f),
        glm::vec2(-6.6f, -13.9f),
        glm::vec2(6.6f, -13.9f),
    };
    for (const glm::vec2& pillarPos : cornerPillars) {
        addMesh(pillarMesh, glm::vec3(pillarPos.x, 0.0f, pillarPos.y), glm::vec3(1.05f));
        addColliderCylinder(registry, entities_,
            glm::vec3(pillarPos.x, roomHeight * 0.5f, pillarPos.y),
            pillarRadius * 1.05f, roomHeight * 0.5f);
    }

    // Rear shrine framing.
    addMesh(pillarMesh, glm::vec3(-2.95f, 0.0f, backZ + 0.8f), glm::vec3(1.0f));
    addMesh(pillarMesh, glm::vec3(2.95f, 0.0f, backZ + 0.8f), glm::vec3(1.0f));
    addMesh(archMesh, glm::vec3(0.0f, 0.0f, backZ + 0.55f), glm::vec3(1.08f, 1.18f, 1.0f));
    addColliderCylinder(registry, entities_,
        glm::vec3(-2.95f, roomHeight * 0.5f, backZ + 0.8f), pillarRadius, roomHeight * 0.5f);
    addColliderCylinder(registry, entities_,
        glm::vec3(2.95f, roomHeight * 0.5f, backZ + 0.8f), pillarRadius, roomHeight * 0.5f);

    // Double door at the rear gate. The leaves are animated by DoorSystem.
    const float doorLeafWidth = portalHalfW - 0.035f;
    const float doorLeafHeight = portalHeight - 0.04f;
    const float doorLeafThickness = 0.30f;
    const float doorZ = backZ + 0.22f;
    const float leftHingeX = -portalHalfW;
    const float rightHingeX = portalHalfW;

    auto createDoorLeaf = [&](Mesh* mesh,
                              const glm::vec3& closedCenter,
                              const glm::vec3& hingePosition,
                              float closedYaw,
                              float openYaw) {
        auto leaf = registry.create();
        registry.emplace<TransformComponent>(leaf);
        registry.emplace<MeshComponent>(leaf, MeshComponent{
            mesh,
            makeModel(closedCenter, glm::vec3(doorLeafWidth, doorLeafHeight, doorLeafThickness)),
            true
        });

        StaticColliderComponent collider;
        collider.shape = ColliderShape::Box;
        collider.position = closedCenter;
        collider.rotation = glm::vec3(0.0f, closedYaw, 0.0f);
        collider.halfExtents = glm::vec3(doorLeafWidth, doorLeafHeight, doorLeafThickness) * 0.5f;
        registry.emplace<StaticColliderComponent>(leaf, collider);

        DoorLeafComponent doorLeaf;
        doorLeaf.hingePosition = hingePosition;
        doorLeaf.centerOffsetFromHinge = closedCenter - hingePosition;
        doorLeaf.closedScale = glm::vec3(doorLeafWidth, doorLeafHeight, doorLeafThickness);
        doorLeaf.colliderHalfExtents = collider.halfExtents;
        doorLeaf.closedYaw = closedYaw;
        doorLeaf.openYaw = openYaw;
        registry.emplace<DoorLeafComponent>(leaf, doorLeaf);

        entities_.push_back(leaf);
        return leaf;
    };

    auto leftLeaf = createDoorLeaf(
        leftDoorMesh,
        glm::vec3(leftHingeX + doorLeafWidth * 0.5f, doorLeafHeight * 0.5f, doorZ),
        glm::vec3(leftHingeX, doorLeafHeight * 0.5f, doorZ),
        0.0f,
        -102.0f
    );
    auto rightLeaf = createDoorLeaf(
        rightDoorMesh,
        glm::vec3(rightHingeX - doorLeafWidth * 0.5f, doorLeafHeight * 0.5f, doorZ),
        glm::vec3(rightHingeX, doorLeafHeight * 0.5f, doorZ),
        0.0f,
        102.0f
    );

    auto doorRoot = registry.create();
    registry.emplace<TransformComponent>(doorRoot, TransformComponent{
        glm::vec3(0.0f, doorLeafHeight * 0.5f, backZ + 1.1f)
    });
    registry.emplace<DoorComponent>(doorRoot, DoorComponent{
        leftLeaf,
        rightLeaf,
        3.4f,
        0.74f,
        2.6f,
        0.0f,
        false,
        false
    });
    entities_.push_back(doorRoot);

    // Side shrine bays with bars and shallow ornament.
    const std::array<float, 3> shrineRows = {2.0f, -6.1f, -14.0f};
    for (float side : {-1.0f, 1.0f}) {
        const float wallX = side * (halfW - 0.25f);
        const float frameX = side * (halfW - 1.1f);

        for (float z : shrineRows) {
            addMesh(cube, glm::vec3(wallX, 2.45f, z), glm::vec3(0.12f, 2.6f, 1.8f));
            addMesh(cube, glm::vec3(frameX, 4.3f, z), glm::vec3(0.35f, 0.35f, 2.2f));
            addMesh(cube, glm::vec3(frameX, 0.95f, z), glm::vec3(0.35f, 0.2f, 2.2f));
            addMesh(cube, glm::vec3(frameX, 2.55f, z - 1.0f), glm::vec3(0.35f, 2.2f, 0.18f));
            addMesh(cube, glm::vec3(frameX, 2.55f, z + 1.0f), glm::vec3(0.35f, 2.2f, 0.18f));

            for (float barOffset : {-0.7f, 0.0f, 0.7f}) {
                addMesh(cube, glm::vec3(side * (halfW - 1.45f), 2.55f, z + barOffset),
                        glm::vec3(0.1f, 1.9f, 0.08f));
            }

            addMesh(cube, glm::vec3(side * (halfW - 1.85f), 0.35f, z),
                    glm::vec3(0.8f, 0.45f, 1.1f),
                    glm::vec3(0.0f, 0.0f, side * 6.0f));
        }
    }

    // Stairs and dais leading up to the rear gate.
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
        addMesh(cube, step.center, step.scale);
        addColliderBox(registry, entities_, step.center, step.scale * 0.5f);
    }

    // Hanging roots and ceiling drips.
    const std::array<glm::vec3, 10> drips = {
        glm::vec3(-7.2f, roomHeight - 1.1f, 5.2f),
        glm::vec3(-5.0f, roomHeight - 1.6f, 1.8f),
        glm::vec3(-1.6f, roomHeight - 0.9f, -0.5f),
        glm::vec3(2.4f, roomHeight - 1.7f, -4.0f),
        glm::vec3(6.4f, roomHeight - 1.0f, -6.4f),
        glm::vec3(-6.1f, roomHeight - 1.4f, -8.8f),
        glm::vec3(-0.9f, roomHeight - 1.8f, -10.7f),
        glm::vec3(3.7f, roomHeight - 1.2f, -13.4f),
        glm::vec3(7.0f, roomHeight - 1.9f, -15.6f),
        glm::vec3(-2.8f, roomHeight - 1.1f, -18.1f),
    };
    const std::array<glm::vec3, 10> dripScales = {
        glm::vec3(0.14f, 1.2f, 0.14f),
        glm::vec3(0.16f, 1.7f, 0.12f),
        glm::vec3(0.12f, 0.9f, 0.12f),
        glm::vec3(0.14f, 1.8f, 0.12f),
        glm::vec3(0.15f, 1.1f, 0.15f),
        glm::vec3(0.12f, 1.4f, 0.12f),
        glm::vec3(0.14f, 1.9f, 0.10f),
        glm::vec3(0.13f, 1.0f, 0.13f),
        glm::vec3(0.11f, 2.0f, 0.11f),
        glm::vec3(0.15f, 1.3f, 0.14f),
    };
    for (size_t i = 0; i < drips.size(); ++i) {
        addMesh(cylinder, drips[i], dripScales[i], glm::vec3(0.0f, 0.0f, 0.0f));
    }

    // Ground clutter.
    const std::array<glm::vec3, 9> debris = {
        glm::vec3(-6.5f, 0.18f, 4.8f),
        glm::vec3(-7.3f, 0.12f, -4.2f),
        glm::vec3(-6.1f, 0.16f, -10.8f),
        glm::vec3(-1.5f, 0.12f, -16.2f),
        glm::vec3(1.8f, 0.10f, -15.4f),
        glm::vec3(6.7f, 0.15f, -8.3f),
        glm::vec3(7.1f, 0.12f, -2.5f),
        glm::vec3(5.6f, 0.16f, 3.1f),
        glm::vec3(0.0f, 0.14f, backZ + 3.2f),
    };
    const std::array<glm::vec3, 9> debrisScales = {
        glm::vec3(0.55f, 0.16f, 0.42f),
        glm::vec3(0.38f, 0.12f, 0.30f),
        glm::vec3(0.62f, 0.18f, 0.46f),
        glm::vec3(0.30f, 0.11f, 0.26f),
        glm::vec3(0.35f, 0.10f, 0.24f),
        glm::vec3(0.44f, 0.14f, 0.32f),
        glm::vec3(0.28f, 0.11f, 0.22f),
        glm::vec3(0.49f, 0.15f, 0.38f),
        glm::vec3(0.82f, 0.18f, 0.54f),
    };
    for (size_t i = 0; i < debris.size(); ++i) {
        addMesh(cube, debris[i], debrisScales[i],
                glm::vec3(0.0f, 17.0f * static_cast<float>(i), 10.0f * static_cast<float>(i % 2)));
    }

    // Point lights push the eye toward the rear gate while keeping the room murky.
    addLight(glm::vec3(0.0f, roomHeight - 1.3f, frontZ - 1.8f), moonColor, 13.5f, 0.7f);
    addLight(glm::vec3(-5.0f, 3.8f, 2.1f), torchColor, 10.0f, 1.25f);
    addLight(glm::vec3(5.0f, 3.8f, -5.7f), torchColor, 10.0f, 1.15f);
    addLight(glm::vec3(-4.2f, 3.4f, -13.8f), torchColor, 9.0f, 1.1f);
    addLight(glm::vec3(4.2f, 3.6f, -13.0f), torchColor, 9.0f, 1.05f);
    addLight(glm::vec3(-2.2f, 4.8f, backZ + 2.2f), torchColor, 8.0f, 1.45f);
    addLight(glm::vec3(2.2f, 4.8f, backZ + 2.2f), torchColor, 8.0f, 1.45f);
    addLight(glm::vec3(0.0f, 4.4f, corridorCenterZ - 0.5f), glm::vec3(0.72f, 0.74f, 0.92f), 8.5f, 0.55f);
    addLight(glm::vec3(-3.4f, 3.2f, chamberCenterZ - 4.0f), torchColor, 8.5f, 1.1f);
    addLight(glm::vec3(3.4f, 3.2f, chamberCenterZ + 2.6f), torchColor, 8.5f, 1.1f);

    // ===== Collision geometry =====

    // Floor collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, -0.05f, centerZ),
        glm::vec3(halfW, 0.05f, roomDepth * 0.5f));

    // Ceiling collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, roomHeight + 0.05f, centerZ),
        glm::vec3(halfW, 0.05f, roomDepth * 0.5f));

    // Left wall collider
    addColliderBox(registry, entities_,
        glm::vec3(-halfW, roomHeight * 0.5f, centerZ),
        glm::vec3(wallThickness * 0.5f, roomHeight * 0.5f, roomDepth * 0.5f));

    // Right wall collider
    addColliderBox(registry, entities_,
        glm::vec3(halfW, roomHeight * 0.5f, centerZ),
        glm::vec3(wallThickness * 0.5f, roomHeight * 0.5f, roomDepth * 0.5f));

    // Front boundary is invisible so the opening reads as a silhouette from inside.
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, roomHeight * 0.5f, frontZ + 0.7f),
        glm::vec3(halfW, roomHeight * 0.5f, 0.15f));

    // Rear wall segments and tunnel seal.
    addColliderBox(registry, entities_,
        glm::vec3(-(portalHalfW + rearWingWidth * 0.5f), roomHeight * 0.5f, backZ),
        glm::vec3(rearWingWidth * 0.5f, roomHeight * 0.5f, wallThickness * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(portalHalfW + rearWingWidth * 0.5f, roomHeight * 0.5f, backZ),
        glm::vec3(rearWingWidth * 0.5f, roomHeight * 0.5f, wallThickness * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, portalHeight + (roomHeight - portalHeight) * 0.5f, backZ),
        glm::vec3(portalHalfW, (roomHeight - portalHeight) * 0.5f, wallThickness * 0.5f));

    // Corridor and chamber collision.
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, -0.44f, corridorCenterZ),
        glm::vec3(corridorWidth * 0.5f, 0.08f, (corridorLength + 1.6f) * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, roomHeight - 0.29f, corridorCenterZ),
        glm::vec3(corridorWidth * 0.5f, 0.08f, (corridorLength + 1.6f) * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(-corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ),
        glm::vec3(0.18f, (roomHeight - 0.5f) * 0.5f, (corridorLength + 1.2f) * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(corridorWidth * 0.5f, roomHeight * 0.45f, corridorCenterZ),
        glm::vec3(0.18f, (roomHeight - 0.5f) * 0.5f, (corridorLength + 1.2f) * 0.5f));

    addColliderBox(registry, entities_,
        glm::vec3(0.0f, -0.64f, chamberCenterZ),
        glm::vec3(chamberWidth * 0.5f, 0.08f, chamberDepth * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, roomHeight - 0.49f, chamberCenterZ),
        glm::vec3(chamberWidth * 0.5f, 0.08f, chamberDepth * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(-chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ),
        glm::vec3(0.26f, (roomHeight - 0.8f) * 0.5f, chamberDepth * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(chamberWidth * 0.5f, roomHeight * 0.42f, chamberCenterZ),
        glm::vec3(0.26f, (roomHeight - 0.8f) * 0.5f, chamberDepth * 0.5f));
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, roomHeight * 0.42f, chamberCenterZ - chamberDepth * 0.5f),
        glm::vec3(chamberWidth * 0.5f, (roomHeight - 0.8f) * 0.5f, 0.26f));
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, 0.16f, chamberCenterZ - chamberDepth * 0.5f - 0.2f),
        glm::vec3(chamberWidth * 0.5f, 1.2f, 0.26f));

    // Camera / Player
    {
        auto e = registry.create();
        registry.emplace<TransformComponent>(e, TransformComponent{glm::vec3(0.0f, 1.6f, 5.4f)});
        registry.emplace<CameraComponent>(e);
        registry.emplace<CharacterControllerComponent>(e);
        registry.emplace<PlayerMovementComponent>(e);
        registry.emplace<PlayerInteractionLockComponent>(e);
        registry.emplace<PlayerSpawnComponent>(e, PlayerSpawnComponent{glm::vec3(0.0f, 1.6f, 5.4f), -8.0f});
        entities_.push_back(e);
    }

    // Viewmodel hand (first-person)
    {
        auto e = registry.create();
        registry.emplace<MeshComponent>(e, MeshComponent{meshLibrary_.get("hand"), glm::mat4(1.0f), false});
        registry.emplace<ViewmodelComponent>(e);
        entities_.push_back(e);
    }
}

void CathedralScene::onExit(Application& app) {
    for (auto e : entities_) {
        if (app.registry().valid(e)) {
            app.registry().destroy(e);
        }
    }
    entities_.clear();
    meshLibrary_.clear();
}
