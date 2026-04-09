#include "game/modules/door/DoorSpawner.h"

#include "game/modules/door/DoorComponents.h"
#include "game/modules/door/DoorMath.h"
#include "game/behavior/ActionTypes.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/MeshComponent.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"
#include "engine/rendering/geometry/Mesh.h"

#include <spdlog/spdlog.h>

#include <cmath>

entt::entity spawnDoorGroup(LevelBuilder& builder,
                             const LevelDoorPlacement& group,
                             const LevelDef& level) {
    auto& reg = builder.registry();

    // Spawn child meshes that belong to this door group from the scene definition.
    // LevelLoader skips these in the normal mesh loop, so we must spawn them here.
    entt::entity leafEntity = entt::null;
    for (const auto& m : level.meshes) {
        if (m.parentNodeId != group.nodeId) continue;

        // m.scale already includes group.scale from resolveLevelHierarchy —
        // the parent door group's transform is baked into child mesh transforms.
        auto entity = builder.addMesh(m.meshId, m.position, m.scale, m.rotation, m.tint,
                                      m.materialId.empty() ? std::optional<std::string>{}
                                                           : std::optional<std::string>{m.materialId});
        if (entity == entt::null) continue;

        if (!m.nodeId.empty()) {
            builder.attachNodeId(entity, m.nodeId);
        }

        // The child mesh with a pivot is the door leaf (animatable panel)
        if (m.pivot.has_value()) {
            leafEntity = entity;
            const glm::vec3 leafPivot = *m.pivot;

            // Compute mesh AABB center so the door's geometric center aligns with the
            // frame center when closed. Door FBX meshes have their origin at the hinge
            // edge, not at the geometric center — without this offset there's a visible gap.
            Mesh* leafMeshPtr = builder.mesh(m.meshId);
            const glm::vec3 meshCenter = leafMeshPtr
                ? (leafMeshPtr->aabbMin() + leafMeshPtr->aabbMax()) * 0.5f
                : glm::vec3(0.0f);
            const glm::mat4 leafModel = makePivotLeafModel(
                m.position, group.yawDegrees, group.yawDegrees,
                leafPivot, meshCenter, m.scale);

            // Override model matrix for pivot-based rendering
            if (auto* meshComp = reg.try_get<MeshComponent>(entity)) {
                meshComp->modelOverride = leafModel;
                meshComp->useModelOverride = true;
            }

            // Attach DoorLeafComponent for swing animation
            DoorLeafComponent doorLeaf;
            doorLeaf.basePosition = m.position;
            doorLeaf.pivot = leafPivot;
            doorLeaf.meshCenter = meshCenter;
            doorLeaf.closedScale = m.scale;
            doorLeaf.closedYaw = group.yawDegrees;
            doorLeaf.openYaw = group.yawDegrees - group.openAngle;
            reg.emplace<DoorLeafComponent>(entity, doorLeaf);
        }
    }

    if (leafEntity == entt::null) {
        spdlog::warn("spawnDoorGroup: no leaf mesh (with pivot) found for door group '{}'",
                     group.name);
        return entt::null;
    }

    // Create door root entity with DoorConfigComponent + DoorStateComponent + InteractableComponent
    auto doorRoot = builder.createTransformEntity(group.position + glm::vec3(0.0f, 1.0f, 0.0f));

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
        DoorConfigComponent config;
        config.leftLeaf = leafEntity;
        config.rightLeaf = entt::null;
        config.interactDistance = group.interactDistance;
        config.interactDotThreshold = group.interactDotThreshold;
        config.openDuration = group.openDuration;
        config.openAngle = group.openAngle;
        config.locked = group.locked;
        config.lockedPrompt = group.lockedPrompt;
        reg.emplace<DoorConfigComponent>(doorRoot, config);
        reg.emplace<DoorStateComponent>(doorRoot);

        reg.emplace<InteractableComponent>(doorRoot,
            InteractableComponent{
                "E  Open",
                "",
                group.interactDistance,
                group.interactDotThreshold,
                true,
                false
            });

        // BehaviorComponent with ToggleDoor so GameplayBehaviors processes the interaction
        BehaviorComponent behavior;
        ActionEntry toggleAction;
        toggleAction.type = ActionType::ToggleDoor;
        toggleAction.targetNodeId = "self";
        toggleAction.params = DoorActionParams{group.openDuration};
        behavior.onActivate.push_back(toggleAction);
        reg.emplace<BehaviorComponent>(doorRoot, behavior);
    }

    if (!group.nodeId.empty()) {
        builder.attachNodeId(doorRoot, group.nodeId);
    }

    return doorRoot;
}
