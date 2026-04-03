#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include "game/behavior/BehaviorComponent.h"
#include "game/behavior/NodeIdComponent.h"
#include "game/behavior/TriggerComponent.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelDef.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>

class Mesh;

class LevelBuilder {
public:
    explicit LevelBuilder(LevelBuildContext& context);

    entt::registry& registry() const { return context_.registry; }
    entt::entity createEntity();
    entt::entity createTransformEntity(const glm::vec3& position,
                                       const glm::vec3& rotation = glm::vec3(0.0f),
                                       const glm::vec3& scale = glm::vec3(1.0f));
    void track(entt::entity entity);

    Mesh* mesh(const std::string& name) const;
    entt::entity addMesh(Mesh* mesh,
                         const glm::vec3& position,
                         const glm::vec3& scale,
                         const glm::vec3& rotation = glm::vec3(0.0f),
                         std::optional<glm::vec3> tint = std::nullopt,
                         std::optional<std::string> materialId = std::nullopt);
    entt::entity addMesh(const std::string& meshName,
                         const glm::vec3& position,
                         const glm::vec3& scale,
                         const glm::vec3& rotation = glm::vec3(0.0f),
                         std::optional<glm::vec3> tint = std::nullopt,
                         std::optional<std::string> materialId = std::nullopt);
    entt::entity addLight(const glm::vec3& position,
                          const glm::vec3& color,
                          float radius,
                          float intensity);
    entt::entity addLight(const glm::vec3& position,
                         const glm::vec3& color,
                         float radius,
                         float intensity,
                         LightType type,
                         const glm::vec3& direction,
                         float innerConeDegrees,
                         float outerConeDegrees,
                         bool castsShadows);
    entt::entity addBoxCollider(const glm::vec3& position,
                                const glm::vec3& halfExtents,
                                const glm::vec3& rotation = glm::vec3(0.0f));
    entt::entity addCylinderCollider(const glm::vec3& position,
                                     float radius,
                                     float halfHeight,
                                     const glm::vec3& rotation = glm::vec3(0.0f));
    entt::entity addReflectionProbe(const glm::vec3& position,
                                    const glm::vec3& extents,
                                    float blendDistance,
                                    float intensity,
                                    bool boxProjection);

    // Behavior and trigger attachment methods
    void attachNodeId(entt::entity entity, const std::string& nodeId);
    void attachBehaviors(entt::entity entity, const std::vector<BehaviorDeclaration>& declarations);
    void attachInteractable(entt::entity entity, const InteractableDeclaration& decl);
    entt::entity addTrigger(const TriggerPlacement& placement);

private:
    LevelBuildContext& context_;
};
