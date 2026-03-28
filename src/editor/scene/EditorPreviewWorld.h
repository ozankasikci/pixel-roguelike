#pragma once

#include "editor/scene/EditorSceneDocument.h"
#include "game/level/LevelBuildContext.h"

#include <glm/glm.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

class ContentRegistry;
class MaterialTextureLibrary;

struct EditorObjectBounds {
    bool valid = false;
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    void expand(const glm::vec3& point);
    void expand(const EditorObjectBounds& other);
    glm::vec3 center() const { return (min + max) * 0.5f; }
};

class EditorPreviewWorld {
public:
    EditorPreviewWorld();

    void rebuild(const EditorSceneDocument& document, const ContentRegistry& content);

    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }
    MeshLibrary& meshLibrary() { return meshLibrary_; }
    const MeshLibrary& meshLibrary() const { return meshLibrary_; }

    const std::unordered_map<entt::entity, std::uint64_t>& ownerMap() const { return ownerMap_; }
    const std::unordered_map<std::uint64_t, EditorObjectBounds>& objectBounds() const { return objectBounds_; }
    const EditorObjectBounds* findObjectBounds(std::uint64_t objectId) const;
    const EditorObjectBounds& sceneBounds() const { return sceneBounds_; }

private:
    void clearEntities();
    void rebuildBounds();

    entt::registry registry_;
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    std::unordered_map<entt::entity, std::uint64_t> ownerMap_;
    std::unordered_map<std::uint64_t, EditorObjectBounds> objectBounds_;
    EditorObjectBounds sceneBounds_;
};
