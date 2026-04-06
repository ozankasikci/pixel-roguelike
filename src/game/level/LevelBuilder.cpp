#include "game/level/LevelBuilder.h"

#include "game/behavior/BehaviorComponent.h"
#include "game/behavior/NodeIdComponent.h"
#include "game/components/ColliderComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/ReflectionProbeComponent.h"
#include "game/rendering/RetroPalette.h"
#include "game/components/TransformComponent.h"
#include "game/prefabs/GameplayPrefabs.h"

#include "engine/core/MathUtils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <string_view>

namespace {

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

std::string defaultMaterialIdForMesh(std::string_view meshName) {
    if (meshName == "door_leaf_left" || meshName == "door_leaf_right") {
        return "wood_default";
    }
    if (meshName == "door_frame_romanesque") {
        return "stone_default";
    }
    if (meshName == "gothic_door_static") {
        return "wood_default";
    }
    if (meshName == "hand") {
        return "viewmodel_default";
    }
    return "stone_default";
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
                                   std::optional<std::string> materialId) {
    if (mesh == nullptr) {
        spdlog::warn("Level builder received null mesh");
        return entt::null;
    }

    auto entity = createEntity();
    context_.registry.emplace<TransformComponent>(entity);
    context_.registry.emplace<MeshComponent>(
        entity,
        MeshComponent{
            "",
            mesh,
            makeModelMatrix(position, scale, rotation),
            true,
            tint.value_or(glm::vec3(1.0f)),
            materialId.value_or("stone_default")
        }
    );
    return entity;
}

entt::entity LevelBuilder::addMesh(const std::string& meshName,
                                   const glm::vec3& position,
                                   const glm::vec3& scale,
                                   const glm::vec3& rotation,
                                   std::optional<glm::vec3> tint,
                                   std::optional<std::string> materialId) {
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
        materialId.value_or(defaultMaterialIdForMesh(meshName))
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

entt::entity LevelBuilder::addCollider(const LevelColliderPlacement& placement) {
    auto entity = createTransformEntity(placement.position, placement.rotation);

    ColliderComponent collider;
    collider.shape = placement.shape;
    collider.mode = placement.mode;
    collider.position = placement.position;
    collider.rotation = placement.rotation;
    collider.halfExtents = placement.halfExtents;
    collider.radius = placement.radius;
    collider.halfHeight = placement.halfHeight;
    collider.fireOnce = placement.fireOnce;
    collider.enabled = true;
    context_.registry.emplace<ColliderComponent>(entity, collider);

    if (!placement.nodeId.empty()) {
        attachNodeId(entity, placement.nodeId);
    }

    if (!placement.behaviors.empty()) {
        attachBehaviors(entity, placement.behaviors);
    }

    return entity;
}

entt::entity LevelBuilder::addReflectionProbe(const glm::vec3& position,
                                              const glm::vec3& extents,
                                              float blendDistance,
                                              float intensity,
                                              bool boxProjection) {
    auto entity = createTransformEntity(position);
    ReflectionProbeComponent probe;
    probe.extents = extents;
    probe.blendDistance = blendDistance;
    probe.intensity = intensity;
    probe.boxProjection = boxProjection;
    probe.dirty = true;
    context_.registry.emplace<ReflectionProbeComponent>(entity, probe);
    return entity;
}

void LevelBuilder::attachNodeId(entt::entity entity, const std::string& nodeId) {
    if (entity == entt::null || nodeId.empty()) {
        return;
    }
    context_.registry.emplace_or_replace<NodeIdComponent>(entity, NodeIdComponent{nodeId});
}

void LevelBuilder::attachBehaviors(entt::entity entity,
                                   const std::vector<BehaviorDeclaration>& declarations) {
    if (entity == entt::null || declarations.empty()) {
        return;
    }
    BehaviorComponent& behavior = context_.registry.get_or_emplace<BehaviorComponent>(entity);
    for (const auto& decl : declarations) {
        if (decl.eventType == "on_activate") {
            behavior.onActivate.push_back(decl.action);
        } else if (decl.eventType == "on_enter") {
            behavior.onEnter.push_back(decl.action);
        } else if (decl.eventType == "on_exit") {
            behavior.onExit.push_back(decl.action);
        } else if (decl.eventType == "on_timer") {
            behavior.onTimer.push_back(decl.action);
        } else {
            spdlog::warn("LevelBuilder: unknown behavior event type '{}'", decl.eventType);
        }
    }
}

void LevelBuilder::attachInteractable(entt::entity entity, const InteractableDeclaration& decl) {
    if (entity == entt::null) {
        return;
    }
    InteractableComponent& ic = context_.registry.get_or_emplace<InteractableComponent>(entity);
    if (!decl.promptText.empty()) {
        ic.promptText = decl.promptText;
    }
    ic.interactDistance = decl.distance;
    ic.interactDotThreshold = decl.dotThreshold;
}

entt::entity LevelBuilder::addDoorGroup(const LevelDoorGroupPlacement& group, const LevelDef& level) {
    // Find child meshes belonging to this door group by parentNodeId
    const LevelMeshPlacement* framePlacement = nullptr;
    const LevelMeshPlacement* leafPlacement = nullptr;
    for (const auto& m : level.meshes) {
        if (m.parentNodeId == group.nodeId) {
            if (m.pivot.has_value()) {
                leafPlacement = &m;
            } else {
                framePlacement = &m;
            }
        }
    }

    if (!leafPlacement) {
        spdlog::warn("LevelBuilder::addDoorGroup: no leaf mesh found for door group '{}'", group.name);
        return entt::null;
    }

    // Spawn frame mesh as a normal static mesh (if exists)
    if (framePlacement) {
        auto frameEntity = addMesh(framePlacement->meshId,
            framePlacement->position,
            framePlacement->scale,
            framePlacement->rotation,
            framePlacement->tint,
            framePlacement->materialId.empty()
                ? std::optional<std::string>{}
                : std::optional<std::string>{framePlacement->materialId});
        if (frameEntity != entt::null && !framePlacement->nodeId.empty()) {
            attachNodeId(frameEntity, framePlacement->nodeId);
        }
    }

    // Use the leaf's resolved position (respects editor edits) for pivot math
    const glm::vec3 leafBasePos = leafPlacement->position;
    const glm::vec3 pivot = leafPlacement->pivot.value();
    const glm::vec3 hingeWorldPos = computeHingeWorldPos(leafBasePos, group.yawDegrees, pivot);
    const glm::mat4 leafModel = makePivotLeafModel(leafBasePos, group.yawDegrees, pivot, leafPlacement->scale);

    // Spawn the leaf mesh entity
    Mesh* leafMeshPtr = mesh(leafPlacement->meshId);
    auto leafEntity = addMesh(leafMeshPtr,
        leafBasePos,
        leafPlacement->scale,
        glm::vec3(0.0f, group.yawDegrees, 0.0f),
        leafPlacement->tint,
        leafPlacement->materialId.empty()
            ? std::optional<std::string>{}
            : std::optional<std::string>{leafPlacement->materialId});

    if (leafEntity == entt::null) {
        spdlog::warn("LevelBuilder::addDoorGroup: failed to spawn leaf mesh '{}' for door group '{}'",
                     leafPlacement->meshId, group.name);
        return entt::null;
    }

    auto& reg = context_.registry;

    // Override the model matrix for correct pivot-based rendering
    if (auto* meshComp = reg.try_get<MeshComponent>(leafEntity)) {
        meshComp->modelOverride = leafModel;
        meshComp->useModelOverride = true;
    }

    // Attach ColliderComponent for physics and DoorAnimationSystem
    const glm::vec3 leafCenter = glm::vec3(leafModel * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    ColliderComponent collider;
    collider.shape = ColliderShape::Box;
    collider.mode = ColliderMode::Solid;
    collider.position = leafCenter;
    collider.rotation = glm::vec3(0.0f, group.yawDegrees, 0.0f);
    collider.halfExtents = glm::vec3(0.445f, 1.01f, 0.05f);
    reg.emplace<ColliderComponent>(leafEntity, collider);

    // Attach DoorLeafComponent for swing animation
    DoorLeafComponent doorLeaf;
    doorLeaf.hingePosition = hingeWorldPos;
    doorLeaf.centerOffsetFromHinge = -pivot;  // derive from authored pivot, not hardcoded
    doorLeaf.closedScale = leafPlacement->scale;
    doorLeaf.colliderHalfExtents = glm::vec3(std::abs(pivot.x), 1.01f, 0.05f);
    doorLeaf.closedYaw = group.yawDegrees;
    doorLeaf.openYaw = group.yawDegrees - group.openAngle;
    reg.emplace<DoorLeafComponent>(leafEntity, doorLeaf);

    if (!leafPlacement->nodeId.empty()) {
        attachNodeId(leafEntity, leafPlacement->nodeId);
    }

    // Create door root entity with DoorComponent + InteractableComponent
    // Place at leaf base position so the interact prompt is near the actual door
    auto doorRoot = createTransformEntity(leafBasePos + glm::vec3(0.0f, 1.0f, 0.0f));

    if (group.locked) {
        // Locked door: only InteractableComponent, no DoorComponent
        reg.emplace<InteractableComponent>(doorRoot,
            InteractableComponent{
                group.lockedPrompt,
                "",
                group.interactDistance,
                group.interactDotThreshold,
                true,
                false
            });
    } else {
        DoorComponent door;
        door.leftLeaf = leafEntity;
        door.rightLeaf = entt::null;
        door.interactDistance = group.interactDistance;
        door.interactDotThreshold = group.interactDotThreshold;
        door.openDuration = group.openDuration;
        reg.emplace<DoorComponent>(doorRoot, door);

        reg.emplace<InteractableComponent>(doorRoot,
            InteractableComponent{
                "E  Open",
                "",
                group.interactDistance,
                group.interactDotThreshold,
                true,
                false
            });

        // BehaviorComponent with ToggleDoor so BehaviorSystem processes the interaction
        BehaviorComponent behavior;
        ActionEntry toggleAction;
        toggleAction.type = ActionType::ToggleDoor;
        toggleAction.targetNodeId = "self";
        toggleAction.params = DoorActionParams{group.openDuration};
        behavior.onActivate.push_back(toggleAction);
        reg.emplace<BehaviorComponent>(doorRoot, behavior);
    }

    if (!group.nodeId.empty()) {
        attachNodeId(doorRoot, group.nodeId);
    }

    return doorRoot;
}

