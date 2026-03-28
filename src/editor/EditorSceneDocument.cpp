#include "editor/EditorSceneDocument.h"

#include "editor/EditorSceneSerializer.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace {

std::string defaultEnvironmentAssetPath(const std::string& id) {
    return "assets/defs/environments/" + id + ".environment";
}

} // namespace

void EditorSceneDocument::clear() {
    scenePath_.clear();
    objects_.clear();
    environment_ = makeEnvironmentDefinition(EnvironmentProfile::Neutral);
    legacyEnvironmentProfile_ = EnvironmentProfile::Neutral;
    nextObjectId_ = 1;
    sceneRevision_ = 0;
    environmentRevision_ = 0;
    sceneDirty_ = false;
    environmentDirty_ = false;
}

void EditorSceneDocument::loadFromSceneFile(const std::string& scenePath, const ContentRegistry& content) {
    clear();
    scenePath_ = scenePath;
    const LevelDef level = EditorSceneSerializer::load(scenePath);
    legacyEnvironmentProfile_ = level.environmentProfile;
    loadEnvironment(content, level);

    for (const auto& mesh : level.meshes) {
        addMesh(mesh);
    }
    for (const auto& light : level.lights) {
        addLight(light);
    }
    for (const auto& box : level.boxColliders) {
        addBoxCollider(box);
    }
    for (const auto& cylinder : level.cylinderColliders) {
        addCylinderCollider(cylinder);
    }
    if (level.hasPlayerSpawn) {
        setPlayerSpawn(level.playerSpawn);
    }
    for (const auto& archetype : level.archetypes) {
        addArchetype(archetype);
    }

    sceneDirty_ = false;
    environmentDirty_ = false;
    ++sceneRevision_;
    ++environmentRevision_;
}

void EditorSceneDocument::save(const ContentRegistry& content) {
    EditorSceneSerializer::save(scenePath_, toLevelDef());
    saveEnvironmentDefinitionAsset(environmentAssetPath(content), environment_);
    markSaved();
}

void EditorSceneDocument::setScenePath(std::string scenePath) {
    scenePath_ = std::move(scenePath);
    markSceneDirty();
}

void EditorSceneDocument::setEnvironmentId(const std::string& environmentId, const ContentRegistry& content) {
    if (environment_.id == environmentId) {
        return;
    }

    const EnvironmentDefinition* definition = content.findEnvironment(environmentId);
    if (definition != nullptr) {
        environment_ = *definition;
    } else {
        EnvironmentProfile builtIn = EnvironmentProfile::Neutral;
        environment_ = makeEnvironmentDefinition(tryParseEnvironmentProfileToken(environmentId, builtIn)
            ? builtIn
            : EnvironmentProfile::Neutral);
        environment_.id = environmentId;
    }
    environment_.id = environmentId;
    if (tryParseEnvironmentProfileToken(environmentId, legacyEnvironmentProfile_)) {
        // keep parsed legacy profile for backward-compatible defaults and tests
    } else {
        legacyEnvironmentProfile_ = EnvironmentProfile::Neutral;
    }
    markSceneDirty();
    markEnvironmentDirty();
}

EditorSceneObject* EditorSceneDocument::findObject(std::uint64_t id) {
    auto it = std::find_if(objects_.begin(), objects_.end(), [&](const EditorSceneObject& object) {
        return object.id == id;
    });
    return it == objects_.end() ? nullptr : &*it;
}

const EditorSceneObject* EditorSceneDocument::findObject(std::uint64_t id) const {
    auto it = std::find_if(objects_.begin(), objects_.end(), [&](const EditorSceneObject& object) {
        return object.id == id;
    });
    return it == objects_.end() ? nullptr : &*it;
}

std::uint64_t EditorSceneDocument::addMesh(const LevelMeshPlacement& placement) {
    return addObject(EditorSceneObjectKind::Mesh, placement);
}

std::uint64_t EditorSceneDocument::addLight(const LevelLightPlacement& placement) {
    return addObject(EditorSceneObjectKind::Light, placement);
}

std::uint64_t EditorSceneDocument::addBoxCollider(const LevelBoxColliderPlacement& placement) {
    return addObject(EditorSceneObjectKind::BoxCollider, placement);
}

std::uint64_t EditorSceneDocument::addCylinderCollider(const LevelCylinderColliderPlacement& placement) {
    return addObject(EditorSceneObjectKind::CylinderCollider, placement);
}

std::uint64_t EditorSceneDocument::setPlayerSpawn(const LevelPlayerSpawn& placement) {
    for (auto& object : objects_) {
        if (object.kind == EditorSceneObjectKind::PlayerSpawn) {
            object.payload = placement;
            markSceneDirty();
            return object.id;
        }
    }
    return addObject(EditorSceneObjectKind::PlayerSpawn, placement);
}

std::uint64_t EditorSceneDocument::addArchetype(const LevelArchetypePlacement& placement) {
    return addObject(EditorSceneObjectKind::Archetype, placement);
}

std::uint64_t EditorSceneDocument::duplicateObject(std::uint64_t id) {
    const EditorSceneObject* object = findObject(id);
    if (object == nullptr || object->kind == EditorSceneObjectKind::PlayerSpawn) {
        return 0;
    }
    return addObject(object->kind, object->payload);
}

void EditorSceneDocument::eraseObjects(const std::vector<std::uint64_t>& ids) {
    if (ids.empty()) {
        return;
    }
    objects_.erase(
        std::remove_if(objects_.begin(), objects_.end(), [&](const EditorSceneObject& object) {
            return std::find(ids.begin(), ids.end(), object.id) != ids.end();
        }),
        objects_.end());
    markSceneDirty();
}

LevelDef EditorSceneDocument::toLevelDef() const {
    LevelDef level;
    level.environmentId = environment_.id;
    level.environmentProfile = legacyEnvironmentProfile_;
    if (level.environmentId.empty()) {
        level.environmentId = environmentProfileName(level.environmentProfile);
    }

    for (const auto& object : objects_) {
        switch (object.kind) {
        case EditorSceneObjectKind::Mesh:
            level.meshes.push_back(std::get<LevelMeshPlacement>(object.payload));
            break;
        case EditorSceneObjectKind::Light:
            level.lights.push_back(std::get<LevelLightPlacement>(object.payload));
            break;
        case EditorSceneObjectKind::BoxCollider:
            level.boxColliders.push_back(std::get<LevelBoxColliderPlacement>(object.payload));
            break;
        case EditorSceneObjectKind::CylinderCollider:
            level.cylinderColliders.push_back(std::get<LevelCylinderColliderPlacement>(object.payload));
            break;
        case EditorSceneObjectKind::PlayerSpawn:
            level.playerSpawn = std::get<LevelPlayerSpawn>(object.payload);
            level.hasPlayerSpawn = true;
            break;
        case EditorSceneObjectKind::Archetype:
            level.archetypes.push_back(std::get<LevelArchetypePlacement>(object.payload));
            break;
        }
    }

    return level;
}

std::string EditorSceneDocument::environmentAssetPath(const ContentRegistry& content) const {
    if (const std::string* existingPath = content.findEnvironmentPath(environment_.id)) {
        return *existingPath;
    }
    return defaultEnvironmentAssetPath(environment_.id.empty() ? "neutral" : environment_.id);
}

EditorSceneDocumentState EditorSceneDocument::captureState() const {
    return EditorSceneDocumentState{
        scenePath_,
        objects_,
        environment_,
        legacyEnvironmentProfile_,
        nextObjectId_,
        sceneDirty_,
        environmentDirty_,
    };
}

void EditorSceneDocument::restoreState(const EditorSceneDocumentState& state) {
    scenePath_ = state.scenePath;
    objects_ = state.objects;
    environment_ = state.environment;
    legacyEnvironmentProfile_ = state.legacyEnvironmentProfile;
    nextObjectId_ = state.nextObjectId;
    sceneDirty_ = state.sceneDirty;
    environmentDirty_ = state.environmentDirty;
    ++sceneRevision_;
    ++environmentRevision_;
}

void EditorSceneDocument::markSaved() {
    setDirtyFlags(false, false);
}

void EditorSceneDocument::setDirtyFlags(bool sceneDirty, bool environmentDirty) {
    sceneDirty_ = sceneDirty;
    environmentDirty_ = environmentDirty;
}

std::uint64_t EditorSceneDocument::addObject(EditorSceneObjectKind kind, const EditorSceneObjectPayload& payload) {
    const std::uint64_t objectId = nextObjectId_++;
    objects_.push_back(EditorSceneObject{objectId, kind, payload});
    markSceneDirty();
    return objectId;
}

void EditorSceneDocument::loadEnvironment(const ContentRegistry& content, const LevelDef& level) {
    environment_ = makeEnvironmentDefinition(level.environmentProfile);
    environment_.id = level.environmentId.empty()
        ? std::string(environmentProfileName(level.environmentProfile))
        : level.environmentId;
    if (const EnvironmentDefinition* definition = content.findEnvironment(environment_.id)) {
        environment_ = *definition;
    }
}

const char* editorSceneObjectKindName(EditorSceneObjectKind kind) {
    switch (kind) {
    case EditorSceneObjectKind::Mesh:
        return "Mesh";
    case EditorSceneObjectKind::Light:
        return "Light";
    case EditorSceneObjectKind::BoxCollider:
        return "Box Collider";
    case EditorSceneObjectKind::CylinderCollider:
        return "Cylinder Collider";
    case EditorSceneObjectKind::PlayerSpawn:
        return "Player Spawn";
    case EditorSceneObjectKind::Archetype:
        return "Archetype";
    }
    return "Object";
}

std::string editorSceneObjectLabel(const EditorSceneObject& object) {
    std::ostringstream label;
    label << editorSceneObjectKindName(object.kind) << " #" << object.id;
    switch (object.kind) {
    case EditorSceneObjectKind::Mesh:
        label << " [" << std::get<LevelMeshPlacement>(object.payload).meshId << "]";
        break;
    case EditorSceneObjectKind::Light: {
        const auto& light = std::get<LevelLightPlacement>(object.payload);
        if (light.type == LightType::Directional) {
            label << " [dir]";
        } else if (light.type == LightType::Spot) {
            label << " [spot]";
        } else {
            label << " [point]";
        }
        break;
    }
    case EditorSceneObjectKind::Archetype:
        label << " [" << std::get<LevelArchetypePlacement>(object.payload).archetypeId << "]";
        break;
    default:
        break;
    }
    return label.str();
}

glm::vec3 editorSceneObjectAnchor(const EditorSceneObject& object) {
    switch (object.kind) {
    case EditorSceneObjectKind::Mesh:
        return std::get<LevelMeshPlacement>(object.payload).position;
    case EditorSceneObjectKind::Light:
        return std::get<LevelLightPlacement>(object.payload).position;
    case EditorSceneObjectKind::BoxCollider:
        return std::get<LevelBoxColliderPlacement>(object.payload).position;
    case EditorSceneObjectKind::CylinderCollider:
        return std::get<LevelCylinderColliderPlacement>(object.payload).position;
    case EditorSceneObjectKind::PlayerSpawn:
        return std::get<LevelPlayerSpawn>(object.payload).position;
    case EditorSceneObjectKind::Archetype:
        return std::get<LevelArchetypePlacement>(object.payload).position;
    }
    return glm::vec3(0.0f);
}
