#include "editor/EditorPreviewWorld.h"

#include "engine/core/PathUtils.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelBuilder.h"
#include "game/levels/cathedral/CathedralAssets.h"
#include "game/prefabs/GameplayPrefabs.h"
#include "game/rendering/EnvironmentProfile.h"
#include "game/rendering/MeshAssetProvider.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <filesystem>

namespace {

glm::vec3 transformPoint(const glm::mat4& matrix, const glm::vec3& point) {
    return glm::vec3(matrix * glm::vec4(point, 1.0f));
}

EditorObjectBounds transformBounds(const glm::vec3& boundsMin,
                                   const glm::vec3& boundsMax,
                                   const glm::mat4& matrix) {
    static constexpr glm::vec3 corners[8]{
        {-1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f},
        {-1.0f,  1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f},
        { 1.0f, -1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f},
    };

    EditorObjectBounds result;
    for (const auto& cornerSign : corners) {
        const glm::vec3 corner = glm::mix(boundsMin, boundsMax, (cornerSign + 1.0f) * 0.5f);
        result.expand(transformPoint(matrix, corner));
    }
    return result;
}

EditorObjectBounds sphereBounds(const glm::vec3& center, float radius) {
    EditorObjectBounds result;
    result.expand(center - glm::vec3(radius));
    result.expand(center + glm::vec3(radius));
    return result;
}

EditorObjectBounds colliderBounds(const StaticColliderComponent& collider) {
    glm::mat4 matrix(1.0f);
    matrix = glm::translate(matrix, collider.position);
    matrix = glm::rotate(matrix, glm::radians(collider.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(collider.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(collider.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    if (collider.shape == ColliderShape::Box) {
        return transformBounds(-collider.halfExtents, collider.halfExtents, matrix);
    }
    return transformBounds(glm::vec3(-collider.radius, -collider.halfHeight, -collider.radius),
                           glm::vec3(collider.radius, collider.halfHeight, collider.radius),
                           matrix);
}

bool decomposeTransformMatrix(const glm::mat4& matrix,
                              glm::vec3& position,
                              glm::vec3& rotationDegrees,
                              glm::vec3& scale) {
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::quat orientation;
    if (!glm::decompose(matrix, scale, orientation, position, skew, perspective)) {
        return false;
    }
    rotationDegrees = glm::degrees(glm::eulerAngles(orientation));
    return true;
}

} // namespace

void EditorObjectBounds::expand(const glm::vec3& point) {
    if (!valid) {
        min = point;
        max = point;
        valid = true;
        return;
    }
    min = glm::min(min, point);
    max = glm::max(max, point);
}

void EditorObjectBounds::expand(const EditorObjectBounds& other) {
    if (!other.valid) {
        return;
    }
    expand(other.min);
    expand(other.max);
}

EditorPreviewWorld::EditorPreviewWorld() {
    registerCathedralAssets(meshLibrary_);
    const std::filesystem::path meshDirectory(resolveProjectPath("assets/meshes"));
    if (std::filesystem::exists(meshDirectory)) {
        for (const auto& entry : std::filesystem::directory_iterator(meshDirectory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string extension = entry.path().extension().string();
            if (extension != ".glb" && extension != ".gltf") {
                continue;
            }
            const std::string meshId = entry.path().stem().string();
            if (!meshLibrary_.has(meshId)) {
                meshLibrary_.loadFromFile(meshId, std::filesystem::relative(entry.path(), std::filesystem::current_path()).generic_string());
            }
        }
    }
    const std::string staticDoorPath = resolveProjectPath("assets/meshes/gothic_door_static.glb");
    if (std::filesystem::exists(staticDoorPath) && !meshLibrary_.has("gothic_door_static")) {
        meshLibrary_.loadFromFile("gothic_door_static", "assets/meshes/gothic_door_static.glb");
    }
}

void EditorPreviewWorld::rebuild(const EditorSceneDocument& document, const ContentRegistry& content) {
    clearEntities();
    ownerMap_.clear();
    objectBounds_.clear();
    sceneBounds_ = EditorObjectBounds{};

    registry_.ctx().insert_or_assign(MeshAssetProvider{&meshLibrary_});
    registry_.ctx().insert_or_assign(
        ActiveEnvironmentProfile{"editor_preview", document.environmentId(), document.legacyEnvironmentProfile()});

    LevelBuildContext context{
        .registry = registry_,
        .meshLibrary = meshLibrary_,
        .entities = entities_,
    };
    LevelBuilder builder(context);

    for (const auto& object : document.objects()) {
        const std::size_t previousCount = entities_.size();

        switch (object.kind) {
        case EditorSceneObjectKind::Mesh: {
            auto placement = std::get<LevelMeshPlacement>(object.payload);
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(document.worldTransformMatrix(object.id), position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.scale = scale;
            }
            builder.addMesh(placement.meshId,
                            placement.position,
                            placement.scale,
                            placement.rotation,
                            placement.tint,
                            placement.material,
                            placement.materialId.empty()
                                ? std::optional<std::string>{}
                                : std::optional<std::string>{placement.materialId});
            break;
        }
        case EditorSceneObjectKind::Light: {
            const auto& placement = std::get<LevelLightPlacement>(object.payload);
            builder.addLight(placement.position,
                             placement.color,
                             placement.radius,
                             placement.intensity,
                             placement.type,
                             placement.direction,
                             placement.innerConeDegrees,
                             placement.outerConeDegrees,
                             placement.castsShadows);
            break;
        }
        case EditorSceneObjectKind::BoxCollider: {
            auto placement = std::get<LevelBoxColliderPlacement>(object.payload);
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(document.worldTransformMatrix(object.id), position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.halfExtents = glm::max(scale * 0.5f, glm::vec3(0.001f));
            }
            builder.addBoxCollider(placement.position, placement.halfExtents, placement.rotation);
            break;
        }
        case EditorSceneObjectKind::CylinderCollider: {
            auto placement = std::get<LevelCylinderColliderPlacement>(object.payload);
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(document.worldTransformMatrix(object.id), position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.radius = std::max(0.001f, (std::abs(scale.x) + std::abs(scale.z)) * 0.25f);
                placement.halfHeight = std::max(0.001f, std::abs(scale.y) * 0.5f);
            }
            builder.addCylinderCollider(placement.position, placement.radius, placement.halfHeight, placement.rotation);
            break;
        }
        case EditorSceneObjectKind::PlayerSpawn:
            break;
        case EditorSceneObjectKind::Archetype: {
            auto placement = std::get<LevelArchetypePlacement>(object.payload);
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(document.worldTransformMatrix(object.id), position, rotation, scale)) {
                placement.position = position;
                placement.yawDegrees = rotation.y;
            }
            if (const auto* archetype = content.findArchetype(placement.archetypeId)) {
                (void)spawnGameplayPrefab(
                    builder,
                    instantiateGameplayArchetype(*archetype, placement.position, placement.yawDegrees));
            }
            break;
        }
        }

        for (std::size_t index = previousCount; index < entities_.size(); ++index) {
            ownerMap_[entities_[index]] = object.id;
        }
    }

    rebuildBounds();
}

void EditorPreviewWorld::clearEntities() {
    for (auto entity : entities_) {
        if (registry_.valid(entity)) {
            registry_.destroy(entity);
        }
    }
    entities_.clear();

    auto& ctx = registry_.ctx();
    if (ctx.contains<MeshAssetProvider>()) {
        ctx.erase<MeshAssetProvider>();
    }
    if (ctx.contains<ActiveEnvironmentProfile>()) {
        ctx.erase<ActiveEnvironmentProfile>();
    }
}

const EditorObjectBounds* EditorPreviewWorld::findObjectBounds(std::uint64_t objectId) const {
    auto it = objectBounds_.find(objectId);
    return it == objectBounds_.end() ? nullptr : &it->second;
}

void EditorPreviewWorld::rebuildBounds() {
    objectBounds_.clear();
    sceneBounds_ = EditorObjectBounds{};

    auto meshView = registry_.view<TransformComponent, MeshComponent>();
    for (auto [entity, transform, mesh] : meshView.each()) {
        auto ownerIt = ownerMap_.find(entity);
        if (ownerIt == ownerMap_.end() || mesh.mesh == nullptr) {
            continue;
        }
        const glm::mat4 modelMatrix = mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix();
        const EditorObjectBounds bounds = transformBounds(mesh.mesh->aabbMin(), mesh.mesh->aabbMax(), modelMatrix);
        objectBounds_[ownerIt->second].expand(bounds);
        sceneBounds_.expand(bounds);
    }

    auto colliderView = registry_.view<StaticColliderComponent>();
    for (auto [entity, collider] : colliderView.each()) {
        auto ownerIt = ownerMap_.find(entity);
        if (ownerIt == ownerMap_.end()) {
            continue;
        }
        const EditorObjectBounds bounds = colliderBounds(collider);
        objectBounds_[ownerIt->second].expand(bounds);
        sceneBounds_.expand(bounds);
    }

    auto lightView = registry_.view<TransformComponent, LightComponent>();
    for (auto [entity, transform, light] : lightView.each()) {
        auto ownerIt = ownerMap_.find(entity);
        if (ownerIt == ownerMap_.end()) {
            continue;
        }
        const float helperRadius = light.type == LightType::Directional ? 0.35f : std::max(0.25f, light.radius * 0.08f);
        const EditorObjectBounds bounds = sphereBounds(transform.position, helperRadius);
        objectBounds_[ownerIt->second].expand(bounds);
        sceneBounds_.expand(bounds);
    }
}
