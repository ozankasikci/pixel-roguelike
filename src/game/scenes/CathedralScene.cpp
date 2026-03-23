#include "CathedralScene.h"

#include "engine/core/Application.h"
#include "game/components/TransformComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/CameraComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Helper: spawn a mesh entity with a model override
static entt::entity spawnMesh(entt::registry& registry, Mesh* mesh, const glm::mat4& model) {
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity);
    registry.emplace<MeshComponent>(entity, MeshComponent{mesh, model, true});
    return entity;
}

// Helper: create arch segments between two x positions
static void addArch(entt::registry& registry,
                    std::vector<entt::entity>& entities,
                    Mesh* cubeMesh,
                    float x1, float x2, float baseY, float archHeight, float z, float thickness) {
    int segments = 12;
    float midX = (x1 + x2) * 0.5f;
    float halfSpan = (x2 - x1) * 0.5f;

    for (int i = 0; i < segments; ++i) {
        float a0 = 3.14159265f * i / segments;
        float a1 = 3.14159265f * (i + 1) / segments;

        float cx0 = midX - halfSpan * std::cos(a0);
        float cy0 = baseY + archHeight * std::sin(a0);
        float cx1 = midX - halfSpan * std::cos(a1);
        float cy1 = baseY + archHeight * std::sin(a1);

        float midAX = (cx0 + cx1) * 0.5f;
        float midAY = (cy0 + cy1) * 0.5f;

        float dx = cx1 - cx0;
        float dy = cy1 - cy0;
        float segLen = std::sqrt(dx*dx + dy*dy);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(midAX, midAY, z));
        float angle = std::atan2(dy, dx);
        model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(segLen, thickness, thickness));

        entities.push_back(spawnMesh(registry, cubeMesh, model));
    }
}

void CathedralScene::onEnter(Application& app) {
    entities_.clear();

    meshLibrary_.registerDefaults();

    entt::registry& registry = app.registry();

    Mesh* cube     = meshLibrary_.get("cube");
    Mesh* plane    = meshLibrary_.get("plane");
    Mesh* cyl      = meshLibrary_.get("cylinder");
    Mesh* cylWide  = meshLibrary_.get("cylinder_wide");
    Mesh* cylCap   = meshLibrary_.get("cylinder_cap");

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

    // Pillars
    float pillarRadius  = 0.35f;
    float pillarSpacing = 4.0f;
    int pillarCount = (int)(corridorLength / pillarSpacing);

    for (int i = 0; i < pillarCount; ++i) {
        float z = -2.0f - i * pillarSpacing;

        for (float side : {-1.0f, 1.0f}) {
            float x = side * (halfW - 1.2f);

            // Pillar shaft
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
                m = glm::scale(m, glm::vec3(pillarRadius, wallHeight, pillarRadius));
                entities_.push_back(spawnMesh(registry, cyl, m));
            }

            // Pillar base
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
                float baseScale = pillarRadius * 1.5f;
                m = glm::scale(m, glm::vec3(baseScale, 0.3f, baseScale));
                entities_.push_back(spawnMesh(registry, cylWide, m));
            }

            // Pillar capital
            {
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, wallHeight - 0.25f, z));
                float capScale = pillarRadius * 1.4f;
                m = glm::scale(m, glm::vec3(capScale, 0.25f, capScale));
                entities_.push_back(spawnMesh(registry, cylCap, m));
            }
        }

        // Arches
        float archX1 = -(halfW - 1.2f);
        float archX2 =  (halfW - 1.2f);
        addArch(registry, entities_, cube, archX1, archX2, wallHeight - 0.5f, 0.8f, z, 0.25f);
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

    // Camera
    {
        auto e = registry.create();
        registry.emplace<TransformComponent>(e, TransformComponent{glm::vec3(0.0f, 2.0f, 5.0f)});
        registry.emplace<CameraComponent>(e);
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
