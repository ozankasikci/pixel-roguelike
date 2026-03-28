#pragma once

#include "game/level/LevelDef.h"
#include "game/rendering/EnvironmentDefinition.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

class ContentRegistry;

enum class EditorSceneObjectKind {
    Mesh,
    Light,
    BoxCollider,
    CylinderCollider,
    PlayerSpawn,
    Archetype,
};

using EditorSceneObjectPayload = std::variant<
    LevelMeshPlacement,
    LevelLightPlacement,
    LevelBoxColliderPlacement,
    LevelCylinderColliderPlacement,
    LevelPlayerSpawn,
    LevelArchetypePlacement>;

struct EditorSceneObject {
    std::uint64_t id = 0;
    EditorSceneObjectKind kind = EditorSceneObjectKind::Mesh;
    EditorSceneObjectPayload payload;
};

struct EditorSceneDocumentState {
    std::string scenePath;
    std::vector<EditorSceneObject> objects;
    EnvironmentDefinition environment;
    EnvironmentProfile legacyEnvironmentProfile = EnvironmentProfile::Neutral;
    std::uint64_t nextObjectId = 1;
    bool sceneDirty = false;
    bool environmentDirty = false;
};

class EditorSceneDocument {
public:
    void clear();
    void loadFromSceneFile(const std::string& scenePath, const ContentRegistry& content);
    void save(const ContentRegistry& content);

    std::string scenePath() const { return scenePath_; }
    void setScenePath(std::string scenePath);

    const std::string& environmentId() const { return environment_.id; }
    EnvironmentProfile legacyEnvironmentProfile() const { return legacyEnvironmentProfile_; }
    EnvironmentDefinition& environment() { return environment_; }
    const EnvironmentDefinition& environment() const { return environment_; }
    void setEnvironmentId(const std::string& environmentId, const ContentRegistry& content);

    const std::vector<EditorSceneObject>& objects() const { return objects_; }
    std::vector<EditorSceneObject>& objects() { return objects_; }
    EditorSceneObject* findObject(std::uint64_t id);
    const EditorSceneObject* findObject(std::uint64_t id) const;

    std::uint64_t addMesh(const LevelMeshPlacement& placement);
    std::uint64_t addLight(const LevelLightPlacement& placement);
    std::uint64_t addBoxCollider(const LevelBoxColliderPlacement& placement);
    std::uint64_t addCylinderCollider(const LevelCylinderColliderPlacement& placement);
    std::uint64_t setPlayerSpawn(const LevelPlayerSpawn& placement);
    std::uint64_t addArchetype(const LevelArchetypePlacement& placement);
    std::uint64_t duplicateObject(std::uint64_t id);
    void eraseObjects(const std::vector<std::uint64_t>& ids);

    bool sceneDirty() const { return sceneDirty_; }
    bool environmentDirty() const { return environmentDirty_; }
    bool dirty() const { return sceneDirty_ || environmentDirty_; }
    std::uint64_t sceneRevision() const { return sceneRevision_; }
    std::uint64_t environmentRevision() const { return environmentRevision_; }
    void markSceneDirty() {
        sceneDirty_ = true;
        ++sceneRevision_;
    }
    void markEnvironmentDirty() {
        environmentDirty_ = true;
        ++environmentRevision_;
    }

    LevelDef toLevelDef() const;
    std::string environmentAssetPath(const ContentRegistry& content) const;
    EditorSceneDocumentState captureState() const;
    void restoreState(const EditorSceneDocumentState& state);
    void markSaved();
    void setDirtyFlags(bool sceneDirty, bool environmentDirty);

private:
    std::uint64_t addObject(EditorSceneObjectKind kind, const EditorSceneObjectPayload& payload);
    void loadEnvironment(const ContentRegistry& content, const LevelDef& level);

    std::string scenePath_;
    std::vector<EditorSceneObject> objects_;
    EnvironmentDefinition environment_;
    EnvironmentProfile legacyEnvironmentProfile_ = EnvironmentProfile::Neutral;
    std::uint64_t nextObjectId_ = 1;
    std::uint64_t sceneRevision_ = 0;
    std::uint64_t environmentRevision_ = 0;
    bool sceneDirty_ = false;
    bool environmentDirty_ = false;
};

const char* editorSceneObjectKindName(EditorSceneObjectKind kind);
std::string editorSceneObjectLabel(const EditorSceneObject& object);
glm::vec3 editorSceneObjectAnchor(const EditorSceneObject& object);
