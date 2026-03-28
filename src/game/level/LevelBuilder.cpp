#include "game/level/LevelBuilder.h"

#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/RetroPalette.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <string_view>

namespace {

EnvironmentProfile activeEnvironmentProfile(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (ctx.contains<ActiveEnvironmentProfile>()) {
        return ctx.get<ActiveEnvironmentProfile>().profile;
    }
    return EnvironmentProfile::Neutral;
}

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
                             const glm::vec3& scale,
                             EnvironmentProfile profile) {
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
        if (profile == EnvironmentProfile::CathedralArcade) {
            if (scale.y < 0.0f) {
                return glm::vec3(0.44f, 0.43f, 0.40f);
            }
            return glm::vec3(0.24f, 0.23f, 0.21f);
        }
        if (scale.y < 0.0f) {
            return RetroPalette::CarvedStone;
        }
        return RetroPalette::LimestoneLight;
    }
    if (meshName == "cube") {
        if (profile == EnvironmentProfile::CathedralArcade) {
            if (scale.y <= 0.24f && (scale.x >= 1.0f || scale.z >= 1.0f)) {
                return glm::vec3(0.20f, 0.19f, 0.18f);
            }
            if ((scale.x <= 0.28f || scale.z <= 0.28f) && scale.y >= 1.5f) {
                return glm::vec3(0.42f, 0.40f, 0.38f);
            }
            if (position.y >= 7.0f) {
                return glm::vec3(0.38f, 0.37f, 0.35f);
            }
            return glm::vec3(0.30f, 0.29f, 0.27f);
        }
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
        if (profile == EnvironmentProfile::CathedralArcade) {
            return glm::vec3(0.46f, 0.45f, 0.42f);
        }
        return RetroPalette::CarvedStone;
    }
    if (meshName == "cylinder" || meshName == "cylinder_wide" || meshName == "cylinder_cap") {
        if (profile == EnvironmentProfile::CathedralArcade) {
            return glm::vec3(0.32f, 0.29f, 0.24f);
        }
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

std::string defaultMaterialIdForMesh(std::string_view meshName) {
    return std::string(defaultMaterialIdForKind(defaultMaterialForMesh(meshName)));
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
                                   std::optional<MaterialKind> material,
                                   std::optional<std::string> materialId) {
    if (mesh == nullptr) {
        spdlog::warn("Level builder received null mesh");
        return entt::null;
    }

    const MaterialKind resolvedMaterial = material.value_or(MaterialKind::Stone);
    auto entity = createEntity();
    context_.registry.emplace<TransformComponent>(entity);
    context_.registry.emplace<MeshComponent>(
        entity,
        MeshComponent{
            "",
            mesh,
            makeModel(position, scale, rotation),
            true,
            tint.value_or(glm::vec3(1.0f)),
            resolvedMaterial,
            materialId.value_or(std::string(defaultMaterialIdForKind(resolvedMaterial)))
        }
    );
    return entity;
}

entt::entity LevelBuilder::addMesh(const std::string& meshName,
                                   const glm::vec3& position,
                                   const glm::vec3& scale,
                                   const glm::vec3& rotation,
                                   std::optional<glm::vec3> tint,
                                   std::optional<MaterialKind> material,
                                   std::optional<std::string> materialId) {
    Mesh* found = mesh(meshName);
    if (found == nullptr) {
        spdlog::warn("Level builder missing mesh '{}'", meshName);
        return entt::null;
    }
    const MaterialKind resolvedMaterial = material.value_or(defaultMaterialForMesh(meshName));
    auto entity = addMesh(
        found,
        position,
        scale,
        rotation,
        tint.value_or(defaultTintForMesh(meshName, position, scale, activeEnvironmentProfile(context_.registry))),
        resolvedMaterial,
        materialId.value_or(
            material.has_value()
                ? std::string(defaultMaterialIdForKind(*material))
                : defaultMaterialIdForMesh(meshName))
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

entt::entity LevelBuilder::addBoxCollider(const glm::vec3& position,
                                          const glm::vec3& halfExtents,
                                          const glm::vec3& rotation) {
    auto entity = createEntity();
    StaticColliderComponent collider;
    collider.shape = ColliderShape::Box;
    collider.position = position;
    collider.rotation = rotation;
    collider.halfExtents = halfExtents;
    context_.registry.emplace<StaticColliderComponent>(entity, collider);
    return entity;
}

entt::entity LevelBuilder::addCylinderCollider(const glm::vec3& position,
                                               float radius,
                                               float halfHeight,
                                               const glm::vec3& rotation) {
    auto entity = createEntity();
    StaticColliderComponent collider;
    collider.shape = ColliderShape::Cylinder;
    collider.position = position;
    collider.rotation = rotation;
    collider.radius = radius;
    collider.halfHeight = halfHeight;
    context_.registry.emplace<StaticColliderComponent>(entity, collider);
    return entity;
}
