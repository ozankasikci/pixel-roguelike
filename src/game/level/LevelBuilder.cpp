#include "game/level/LevelBuilder.h"

#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/rendering/RetroPalette.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <string_view>

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

glm::vec3 defaultTintForMesh(std::string_view meshName,
                             const glm::vec3& position,
                             const glm::vec3& scale) {
    if (meshName == "door_leaf_left" || meshName == "door_leaf_right") {
        return RetroPalette::OldWood;
    }
    if (meshName == "door_frame_romanesque") {
        return glm::vec3(0.82f, 0.81f, 0.76f);
    }
    if (meshName == "gothic_door_static") {
        return glm::vec3(0.10f, 0.10f, 0.11f);
    }
    if (meshName == "hand") {
        return RetroPalette::Bone;
    }
    if (meshName == "plane") {
        if (scale.y < 0.0f) {
            return RetroPalette::CarvedStone;
        }
        return RetroPalette::LimestoneLight;
    }
    if (meshName == "cube") {
        if (scale.y <= 0.24f && (scale.x >= 1.0f || scale.z >= 1.0f)) {
            return RetroPalette::Flagstone;
        }
        if ((scale.x <= 0.28f || scale.z <= 0.28f) && scale.y >= 1.5f) {
            return RetroPalette::CarvedStone;
        }
        if (position.y >= 7.0f) {
            return RetroPalette::CarvedStone;
        }
        return RetroPalette::Sandstone;
    }
    if (meshName == "pillar" || meshName == "arch") {
        return RetroPalette::CarvedStone;
    }
    if (meshName == "cylinder" || meshName == "cylinder_wide" || meshName == "cylinder_cap") {
        return RetroPalette::Stone;
    }
    return glm::vec3(1.0f);
}

MaterialKind defaultMaterialForMesh(std::string_view meshName) {
    if (meshName == "door_leaf_left" || meshName == "door_leaf_right") {
        return MaterialKind::Wood;
    }
    if (meshName == "door_frame_romanesque") {
        return MaterialKind::Stone;
    }
    if (meshName == "gothic_door_static") {
        return MaterialKind::Wood;
    }
    if (meshName == "hand") {
        return MaterialKind::Viewmodel;
    }
    return MaterialKind::Stone;
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
                                   const glm::vec3& rotation,
                                   std::optional<glm::vec3> tint,
                                   std::optional<MaterialKind> material) {
    if (mesh == nullptr) {
        spdlog::warn("Level builder received null mesh");
        return entt::null;
    }

    auto entity = createEntity();
    context_.registry.emplace<TransformComponent>(entity);
    context_.registry.emplace<MeshComponent>(
        entity,
        MeshComponent{"", mesh, makeModel(position, scale, rotation), true, tint.value_or(glm::vec3(1.0f)), material.value_or(MaterialKind::Stone)}
    );
    return entity;
}

entt::entity LevelBuilder::addMesh(const std::string& meshName,
                                   const glm::vec3& position,
                                   const glm::vec3& scale,
                                   const glm::vec3& rotation,
                                   std::optional<glm::vec3> tint,
                                   std::optional<MaterialKind> material) {
    Mesh* found = mesh(meshName);
    if (found == nullptr) {
        spdlog::warn("Level builder missing mesh '{}'", meshName);
        return entt::null;
    }
    auto entity = addMesh(
        found,
        position,
        scale,
        rotation,
        tint.value_or(defaultTintForMesh(meshName, position, scale)),
        material.value_or(defaultMaterialForMesh(meshName))
    );
    if (entity != entt::null) {
        context_.registry.get<MeshComponent>(entity).meshId = meshName;
    }
    return entity;
}

entt::entity LevelBuilder::addLight(const glm::vec3& position,
                                    const glm::vec3& color,
                                    float radius,
                                    float intensity) {
    return addLight(position,
                    color,
                    radius,
                    intensity,
                    LightType::Point,
                    glm::vec3(0.0f, -1.0f, 0.0f),
                    20.0f,
                    30.0f,
                    false);
}

entt::entity LevelBuilder::addLight(const glm::vec3& position,
                                    const glm::vec3& color,
                                    float radius,
                                    float intensity,
                                    LightType type,
                                    const glm::vec3& direction,
                                    float innerConeDegrees,
                                    float outerConeDegrees,
                                    bool castsShadows) {
    auto entity = createTransformEntity(position);
    LightComponent light;
    light.type = type;
    light.color = color;
    light.radius = radius;
    light.intensity = intensity;
    light.direction = direction;
    light.innerConeDegrees = innerConeDegrees;
    light.outerConeDegrees = outerConeDegrees;
    light.castsShadows = castsShadows;
    context_.registry.emplace<LightComponent>(entity, light);
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
