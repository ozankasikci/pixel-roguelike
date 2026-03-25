#include "game/level/LevelBuilder.h"

#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"

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

} // namespace

LevelBuilder::LevelBuilder(LevelBuildContext& context)
    : context_(context) {}

entt::entity LevelBuilder::createEntity() {
    auto entity = context_.registry.create();
    track(entity);
    return entity;
}

entt::entity LevelBuilder::createTransformEntity(const glm::vec3& position,
                                                 const glm::vec3& rotation,
                                                 const glm::vec3& scale) {
    auto entity = createEntity();
    context_.registry.emplace<TransformComponent>(entity, TransformComponent{position, rotation, scale});
    return entity;
}

void LevelBuilder::track(entt::entity entity) {
    context_.entities.push_back(entity);
}

Mesh* LevelBuilder::mesh(const std::string& name) const {
    return context_.meshLibrary.get(name);
}

entt::entity LevelBuilder::addMesh(Mesh* mesh,
                                   const glm::vec3& position,
                                   const glm::vec3& scale,
                                   const glm::vec3& rotation) {
    if (mesh == nullptr) {
        spdlog::warn("Level builder received null mesh");
        return entt::null;
    }

    auto entity = createEntity();
    context_.registry.emplace<TransformComponent>(entity);
    context_.registry.emplace<MeshComponent>(entity, MeshComponent{"", mesh, makeModel(position, scale, rotation), true});
    return entity;
}

entt::entity LevelBuilder::addMesh(const std::string& meshName,
                                   const glm::vec3& position,
                                   const glm::vec3& scale,
                                   const glm::vec3& rotation) {
    Mesh* found = mesh(meshName);
    if (found == nullptr) {
        spdlog::warn("Level builder missing mesh '{}'", meshName);
        return entt::null;
    }
    auto entity = addMesh(found, position, scale, rotation);
    if (entity != entt::null) {
        context_.registry.get<MeshComponent>(entity).meshId = meshName;
    }
    return entity;
}

entt::entity LevelBuilder::addLight(const glm::vec3& position,
                                    const glm::vec3& color,
                                    float radius,
                                    float intensity) {
    auto entity = createTransformEntity(position);
    context_.registry.emplace<LightComponent>(entity, LightComponent{color, radius, intensity});
    return entity;
}

entt::entity LevelBuilder::addBoxCollider(const glm::vec3& position, const glm::vec3& halfExtents) {
    auto entity = createEntity();
    StaticColliderComponent collider;
    collider.shape = ColliderShape::Box;
    collider.position = position;
    collider.halfExtents = halfExtents;
    context_.registry.emplace<StaticColliderComponent>(entity, collider);
    return entity;
}

entt::entity LevelBuilder::addCylinderCollider(const glm::vec3& position, float radius, float halfHeight) {
    auto entity = createEntity();
    StaticColliderComponent collider;
    collider.shape = ColliderShape::Cylinder;
    collider.position = position;
    collider.radius = radius;
    collider.halfHeight = halfHeight;
    context_.registry.emplace<StaticColliderComponent>(entity, collider);
    return entity;
}
