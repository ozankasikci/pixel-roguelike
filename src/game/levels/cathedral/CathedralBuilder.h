#pragma once

#include "game/levels/cathedral/CathedralContext.h"

#include <glm/glm.hpp>

#include <string>

class Mesh;

class CathedralBuilder {
public:
    explicit CathedralBuilder(CathedralContext& context);

    entt::registry& registry() const { return context_.registry; }
    void track(entt::entity entity);

    Mesh* mesh(const std::string& name) const;
    entt::entity addMesh(Mesh* mesh,
                         const glm::vec3& position,
                         const glm::vec3& scale,
                         const glm::vec3& rotation = glm::vec3(0.0f));
    entt::entity addMesh(const std::string& meshName,
                         const glm::vec3& position,
                         const glm::vec3& scale,
                         const glm::vec3& rotation = glm::vec3(0.0f));
    entt::entity addLight(const glm::vec3& position,
                          const glm::vec3& color,
                          float radius,
                          float intensity);
    entt::entity addBoxCollider(const glm::vec3& position, const glm::vec3& halfExtents);
    entt::entity addCylinderCollider(const glm::vec3& position, float radius, float halfHeight);

private:
    CathedralContext& context_;
};
