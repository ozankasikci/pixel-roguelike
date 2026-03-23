#include "CathedralScene.h"

#include "engine/core/Application.h"
#include "game/components/TransformComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/CameraComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/PlayerMovementComponent.h"

#include <glm/gtc/matrix_transform.hpp>

// Helper: spawn a mesh entity with a model override
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
    meshLibrary_.loadFromFile("pillar", "assets/meshes/pillar.glb");
    meshLibrary_.loadFromFile("arch", "assets/meshes/arch.glb");

    entt::registry& registry = app.registry();

    Mesh* cube       = meshLibrary_.get("cube");
    Mesh* plane      = meshLibrary_.get("plane");
    Mesh* pillarMesh = meshLibrary_.get("pillar");
    Mesh* archMesh   = meshLibrary_.get("arch");

    float corridorLength = 30.0f;
    float corridorWidth  = 8.0f;
    float wallHeight     = 6.0f;
    float halfW          = corridorWidth * 0.5f;

    // Floor
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -corridorLength * 0.5f));
        m = glm::scale(m, glm::vec3(corridorWidth, 1.0f, corridorLength));
        entities_.push_back(spawnMesh(registry, plane, m));
    }

    // Ceiling
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wallHeight, -corridorLength * 0.5f));
        m = glm::scale(m, glm::vec3(corridorWidth, -1.0f, corridorLength));
        entities_.push_back(spawnMesh(registry, plane, m));
    }

    // Side walls
    for (float side : {-1.0f, 1.0f}) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f),
            glm::vec3(side * halfW, wallHeight * 0.5f, -corridorLength * 0.5f));
        m = glm::scale(m, glm::vec3(0.3f, wallHeight, corridorLength));
        entities_.push_back(spawnMesh(registry, cube, m));
    }

    // Back wall
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f),
            glm::vec3(0.0f, wallHeight * 0.5f, -corridorLength));
        m = glm::scale(m, glm::vec3(corridorWidth, wallHeight, 0.3f));
        entities_.push_back(spawnMesh(registry, cube, m));
    }

    // Pillars and arches (loaded from .glb mesh files)
    float pillarRadius  = 0.35f;
    float pillarSpacing = 4.0f;
    int pillarCount = static_cast<int>(corridorLength / pillarSpacing);

    for (int i = 0; i < pillarCount; ++i) {
        float z = -2.0f - i * pillarSpacing;

        for (float side : {-1.0f, 1.0f}) {
            float x = side * (halfW - 1.2f);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
            entities_.push_back(spawnMesh(registry, pillarMesh, m));
        }

        // Arch — baked at correct XY, just translate Z
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, z));
        entities_.push_back(spawnMesh(registry, archMesh, m));
    }

    // Floor tile lines
    float tileSize = 2.0f;
    for (float tz = 0; tz > -corridorLength; tz -= tileSize) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.01f, tz));
        m = glm::scale(m, glm::vec3(corridorWidth, 0.02f, 0.05f));
        entities_.push_back(spawnMesh(registry, cube, m));
    }

    // Steps
    for (int s = 0; s < 4; ++s) {
        float sz = -corridorLength + 2.0f + s * 0.6f;
        float sy = s * 0.15f;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, sy + 0.075f, sz));
        m = glm::scale(m, glm::vec3(corridorWidth * 0.6f, 0.15f, 0.6f));
        entities_.push_back(spawnMesh(registry, cube, m));
    }

    // Lights
    for (int i = 0; i < pillarCount; i += 2) {
        float z = -2.0f - i * pillarSpacing;
        for (float side : {-1.0f, 1.0f}) {
            auto e = registry.create();
            registry.emplace<TransformComponent>(e, TransformComponent{
                glm::vec3(side * (halfW - 1.0f), wallHeight * 0.65f, z)});
            registry.emplace<LightComponent>(e, LightComponent{glm::vec3(1.0f), 12.0f, 1.2f});
            entities_.push_back(e);
        }
    }

    // Entrance light
    {
        auto e = registry.create();
        registry.emplace<TransformComponent>(e, TransformComponent{
            glm::vec3(0.0f, wallHeight * 0.7f, 2.0f)});
        registry.emplace<LightComponent>(e, LightComponent{glm::vec3(1.0f), 12.0f, 0.8f});
        entities_.push_back(e);
    }

    // ===== Collision geometry =====

    // Floor collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, -0.05f, -corridorLength * 0.5f),
        glm::vec3(halfW, 0.05f, corridorLength * 0.5f));

    // Ceiling collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, wallHeight + 0.05f, -corridorLength * 0.5f),
        glm::vec3(halfW, 0.05f, corridorLength * 0.5f));

    // Left wall collider
    addColliderBox(registry, entities_,
        glm::vec3(-halfW, wallHeight * 0.5f, -corridorLength * 0.5f),
        glm::vec3(0.15f, wallHeight * 0.5f, corridorLength * 0.5f));

    // Right wall collider
    addColliderBox(registry, entities_,
        glm::vec3(halfW, wallHeight * 0.5f, -corridorLength * 0.5f),
        glm::vec3(0.15f, wallHeight * 0.5f, corridorLength * 0.5f));

    // Back wall collider
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, wallHeight * 0.5f, -corridorLength),
        glm::vec3(halfW, wallHeight * 0.5f, 0.15f));

    // Front wall (invisible, prevents walking out the entrance)
    addColliderBox(registry, entities_,
        glm::vec3(0.0f, wallHeight * 0.5f, 1.0f),
        glm::vec3(halfW, wallHeight * 0.5f, 0.15f));

    // Pillar colliders
    for (int i = 0; i < pillarCount; ++i) {
        float z = -2.0f - i * pillarSpacing;
        for (float side : {-1.0f, 1.0f}) {
            float x = side * (halfW - 1.2f);
            addColliderCylinder(registry, entities_,
                glm::vec3(x, wallHeight * 0.5f, z),
                pillarRadius, wallHeight * 0.5f);
        }
    }

    // Step colliders
    for (int s = 0; s < 4; ++s) {
        float sz = -corridorLength + 2.0f + s * 0.6f;
        float sy = s * 0.15f;
        addColliderBox(registry, entities_,
            glm::vec3(0.0f, sy + 0.075f, sz),
            glm::vec3(corridorWidth * 0.3f, 0.075f, 0.3f));
    }

    // Camera / Player
    {
        auto e = registry.create();
        // Spawn at eye height — PhysicsSystem derives capsule center from this
        registry.emplace<TransformComponent>(e, TransformComponent{glm::vec3(0.0f, 1.6f, -2.0f)});
        registry.emplace<CameraComponent>(e);
        registry.emplace<CharacterControllerComponent>(e);
        registry.emplace<PlayerMovementComponent>(e);
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
