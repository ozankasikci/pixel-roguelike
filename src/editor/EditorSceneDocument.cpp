#include "editor/EditorSceneDocument.h"

#include "editor/EditorSceneSerializer.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {

std::string defaultEnvironmentAssetPath(const std::string& id) {
    return "assets/defs/environments/" + id + ".environment";
}

glm::mat4 makeTransformMatrix(const glm::vec3& position,
                              const glm::vec3& rotationDegrees,
                              const glm::vec3& scale) {
    glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    matrix = glm::scale(matrix, scale);
    return matrix;
}

glm::mat3 extractRotationMatrix(const glm::mat4& matrix) {
    glm::mat3 basis(matrix);
    for (int column = 0; column < 3; ++column) {
        const float length = glm::length(basis[column]);
        if (length > 0.0001f) {
            basis[column] /= length;
        }
    }
    return basis;
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

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
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

void EditorSceneDocument::renameEnvironmentId(std::string environmentId) {
    if (environment_.id == environmentId) {
        return;
    }
    environment_.id = std::move(environmentId);
    if (tryParseEnvironmentProfileToken(environment_.id, legacyEnvironmentProfile_)) {
        // keep parsed legacy profile for backward-compatible defaults and tests
    } else {
        legacyEnvironmentProfile_ = EnvironmentProfile::Neutral;
    }
    markSceneDirty();
    markEnvironmentDirty();
}

bool EditorSceneDocument::reloadEnvironmentFromRegistry(const ContentRegistry& content) {
    if (environment_.id.empty()) {
        return false;
    }
    const std::string environmentId = environment_.id;

    if (const EnvironmentDefinition* definition = content.findEnvironment(environmentId)) {
        environment_ = *definition;
    } else {
        EnvironmentProfile builtIn = EnvironmentProfile::Neutral;
        environment_ = makeEnvironmentDefinition(tryParseEnvironmentProfileToken(environmentId, builtIn)
            ? builtIn
            : EnvironmentProfile::Neutral);
        environment_.id = environmentId;
    }

    if (tryParseEnvironmentProfileToken(environmentId, legacyEnvironmentProfile_)) {
        // keep parsed legacy profile for backward-compatible defaults and tests
    } else {
        legacyEnvironmentProfile_ = EnvironmentProfile::Neutral;
    }
    markEnvironmentDirty();
    return true;
}

void EditorSceneDocument::markEnvironmentSaved() {
    setDirtyFlags(sceneDirty_, false);
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

std::uint64_t EditorSceneDocument::parentObjectId(std::uint64_t id) const {
    const EditorSceneObject* object = findObject(id);
    if (object == nullptr) {
        return 0;
    }
    const std::string* parentNodeId = parentNodeIdPtr(*object);
    if (parentNodeId == nullptr || parentNodeId->empty()) {
        return 0;
    }
    return findObjectIdByNodeId(*parentNodeId);
}

std::vector<std::uint64_t> EditorSceneDocument::childObjectIds(std::uint64_t id) const {
    std::vector<std::uint64_t> children;
    const EditorSceneObject* parent = findObject(id);
    if (parent == nullptr) {
        return children;
    }
    const std::string* nodeId = nodeIdPtr(*parent);
    if (nodeId == nullptr || nodeId->empty()) {
        return children;
    }
    for (const auto& object : objects_) {
        const std::string* parentNodeId = parentNodeIdPtr(object);
        if (parentNodeId != nullptr && *parentNodeId == *nodeId) {
            children.push_back(object.id);
        }
    }
    return children;
}

std::vector<std::uint64_t> EditorSceneDocument::rootObjectIds() const {
    std::vector<std::uint64_t> roots;
    for (const auto& object : objects_) {
        if (parentObjectId(object.id) == 0) {
            roots.push_back(object.id);
        }
    }
    return roots;
}

bool EditorSceneDocument::supportsParenting(std::uint64_t id) const {
    const EditorSceneObject* object = findObject(id);
    if (object == nullptr) {
        return false;
    }
    return object->kind == EditorSceneObjectKind::Mesh
        || object->kind == EditorSceneObjectKind::BoxCollider
        || object->kind == EditorSceneObjectKind::CylinderCollider
        || object->kind == EditorSceneObjectKind::Archetype;
}

bool EditorSceneDocument::canSetParent(std::uint64_t childId, std::uint64_t parentId) const {
    if (childId == 0 || parentId == 0 || childId == parentId) {
        return false;
    }
    if (!supportsParenting(childId) || !supportsParenting(parentId)) {
        return false;
    }
    const EditorSceneObject* child = findObject(childId);
    const EditorSceneObject* parent = findObject(parentId);
    if (child == nullptr || parent == nullptr) {
        return false;
    }

    std::uint64_t probe = parentId;
    while (probe != 0) {
        if (probe == childId) {
            return false;
        }
        probe = parentObjectId(probe);
    }
    return true;
}

bool EditorSceneDocument::setParent(std::uint64_t childId, std::uint64_t parentId) {
    if (!canSetParent(childId, parentId)) {
        return false;
    }

    EditorSceneObject* child = findObject(childId);
    EditorSceneObject* parent = findObject(parentId);
    if (child == nullptr || parent == nullptr) {
        return false;
    }

    const glm::mat4 childWorld = worldTransformMatrix(childId);
    ensureObjectNodeId(*parent);
    std::string* childParentNodeId = parentNodeIdPtr(*child);
    const std::string* parentNodeId = nodeIdPtr(*parent);
    if (childParentNodeId == nullptr || parentNodeId == nullptr) {
        return false;
    }
    *childParentNodeId = *parentNodeId;
    if (!applyWorldTransform(childId, childWorld)) {
        return false;
    }
    markSceneDirty();
    return true;
}

bool EditorSceneDocument::clearParent(std::uint64_t childId) {
    EditorSceneObject* child = findObject(childId);
    if (child == nullptr) {
        return false;
    }
    std::string* parentNodeId = parentNodeIdPtr(*child);
    if (parentNodeId == nullptr || parentNodeId->empty()) {
        return false;
    }
    const glm::mat4 childWorld = worldTransformMatrix(childId);
    parentNodeId->clear();
    if (!applyWorldTransform(childId, childWorld)) {
        return false;
    }
    markSceneDirty();
    return true;
}

std::vector<std::uint64_t> EditorSceneDocument::subtreeObjectIds(std::uint64_t rootId) const {
    std::vector<std::uint64_t> subtree;
    const EditorSceneObject* root = findObject(rootId);
    if (root == nullptr) {
        return subtree;
    }

    subtree.push_back(rootId);
    const std::string* rootNodeId = nodeIdPtr(*root);
    if (rootNodeId == nullptr || rootNodeId->empty()) {
        return subtree;
    }

    for (const auto& object : objects_) {
        std::uint64_t probe = parentObjectId(object.id);
        while (probe != 0) {
            if (probe == rootId) {
                subtree.push_back(object.id);
                break;
            }
            probe = parentObjectId(probe);
        }
    }
    return subtree;
}

bool EditorSceneDocument::reorderObjectBlock(std::uint64_t objectId, std::uint64_t targetId, bool after) {
    if (objectId == 0 || targetId == 0 || objectId == targetId) {
        return false;
    }

    const std::vector<std::uint64_t> movedIds = subtreeObjectIds(objectId);
    if (movedIds.empty()) {
        return false;
    }
    if (std::find(movedIds.begin(), movedIds.end(), targetId) != movedIds.end()) {
        return false;
    }

    const std::vector<std::uint64_t> targetIds = after ? subtreeObjectIds(targetId) : std::vector<std::uint64_t>{targetId};
    if (targetIds.empty()) {
        return false;
    }

    auto isMoved = [&](std::uint64_t id) {
        return std::find(movedIds.begin(), movedIds.end(), id) != movedIds.end();
    };
    auto isTarget = [&](std::uint64_t id) {
        return std::find(targetIds.begin(), targetIds.end(), id) != targetIds.end();
    };

    std::vector<EditorSceneObject> movedBlock;
    std::vector<EditorSceneObject> remaining;
    movedBlock.reserve(movedIds.size());
    remaining.reserve(objects_.size() - movedIds.size());
    for (const auto& object : objects_) {
        if (isMoved(object.id)) {
            movedBlock.push_back(object);
        } else {
            remaining.push_back(object);
        }
    }

    std::size_t insertIndex = remaining.size();
    if (after) {
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            if (isTarget(remaining[i].id)) {
                insertIndex = i + 1;
            }
        }
    } else {
        for (std::size_t i = 0; i < remaining.size(); ++i) {
            if (remaining[i].id == targetId) {
                insertIndex = i;
                break;
            }
        }
    }

    if (insertIndex > remaining.size()) {
        return false;
    }

    remaining.insert(remaining.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                     movedBlock.begin(),
                     movedBlock.end());
    objects_ = std::move(remaining);
    markSceneDirty();
    return true;
}

bool EditorSceneDocument::moveObjectBefore(std::uint64_t objectId, std::uint64_t targetId) {
    if (objectId == 0 || targetId == 0 || objectId == targetId) {
        return false;
    }

    const std::uint64_t targetParentId = parentObjectId(targetId);
    const std::uint64_t currentParentId = parentObjectId(objectId);
    const glm::mat4 world = worldTransformMatrix(objectId);

    bool parentChanged = false;
    if (currentParentId != targetParentId) {
        if (targetParentId == 0) {
            parentChanged = clearParent(objectId);
        } else {
            parentChanged = setParent(objectId, targetParentId);
        }
        if (!parentChanged) {
            return false;
        }
        if (!applyWorldTransform(objectId, world)) {
            return false;
        }
    }

    return reorderObjectBlock(objectId, targetId, false);
}

bool EditorSceneDocument::moveObjectAfter(std::uint64_t objectId, std::uint64_t targetId) {
    if (objectId == 0 || targetId == 0 || objectId == targetId) {
        return false;
    }

    const std::uint64_t targetParentId = parentObjectId(targetId);
    const std::uint64_t currentParentId = parentObjectId(objectId);
    const glm::mat4 world = worldTransformMatrix(objectId);

    bool parentChanged = false;
    if (currentParentId != targetParentId) {
        if (targetParentId == 0) {
            parentChanged = clearParent(objectId);
        } else {
            parentChanged = setParent(objectId, targetParentId);
        }
        if (!parentChanged) {
            return false;
        }
        if (!applyWorldTransform(objectId, world)) {
            return false;
        }
    }

    return reorderObjectBlock(objectId, targetId, true);
}

bool EditorSceneDocument::moveObjectToRootEnd(std::uint64_t objectId) {
    if (objectId == 0) {
        return false;
    }

    const glm::mat4 world = worldTransformMatrix(objectId);
    if (parentObjectId(objectId) != 0) {
        if (!clearParent(objectId)) {
            return false;
        }
        if (!applyWorldTransform(objectId, world)) {
            return false;
        }
    }

    const std::vector<std::uint64_t> movedIds = subtreeObjectIds(objectId);
    if (movedIds.empty()) {
        return false;
    }

    auto isMoved = [&](std::uint64_t id) {
        return std::find(movedIds.begin(), movedIds.end(), id) != movedIds.end();
    };

    std::vector<EditorSceneObject> movedBlock;
    std::vector<EditorSceneObject> remaining;
    for (const auto& object : objects_) {
        if (isMoved(object.id)) {
            movedBlock.push_back(object);
        } else {
            remaining.push_back(object);
        }
    }

    remaining.insert(remaining.end(), movedBlock.begin(), movedBlock.end());
    objects_ = std::move(remaining);
    markSceneDirty();
    return true;
}

glm::mat4 EditorSceneDocument::worldTransformMatrix(std::uint64_t id) const {
    const EditorSceneObject* object = findObject(id);
    if (object == nullptr) {
        return glm::mat4(1.0f);
    }
    return localParentMatrix(*object) * localTransformMatrix(*object);
}

glm::mat3 EditorSceneDocument::worldRotationMatrix(std::uint64_t id) const {
    return extractRotationMatrix(worldTransformMatrix(id));
}

bool EditorSceneDocument::applyWorldTransform(std::uint64_t id, const glm::mat4& worldMatrix) {
    EditorSceneObject* object = findObject(id);
    if (object == nullptr) {
        return false;
    }

    const glm::mat4 localMatrix = glm::inverse(localParentMatrix(*object)) * worldMatrix;
    glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh: {
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        if (!decomposeTransformMatrix(localMatrix, position, rotation, scale)) {
            return false;
        }
        mesh.position = position;
        mesh.rotation = rotation;
        mesh.scale = glm::max(scale, glm::vec3(0.01f));
        break;
    }
    case EditorSceneObjectKind::Light: {
        auto& light = std::get<LevelLightPlacement>(object->payload);
        if (light.type != LightType::Directional) {
            light.position = glm::vec3(localMatrix[3]);
        }
        break;
    }
    case EditorSceneObjectKind::BoxCollider: {
        auto& box = std::get<LevelBoxColliderPlacement>(object->payload);
        if (!decomposeTransformMatrix(localMatrix, position, rotation, scale)) {
            return false;
        }
        box.position = position;
        box.rotation = rotation;
        box.halfExtents = glm::max(scale * 0.5f, glm::vec3(0.01f));
        break;
    }
    case EditorSceneObjectKind::CylinderCollider: {
        auto& cylinder = std::get<LevelCylinderColliderPlacement>(object->payload);
        if (!decomposeTransformMatrix(localMatrix, position, rotation, scale)) {
            return false;
        }
        cylinder.position = position;
        cylinder.rotation = rotation;
        cylinder.radius = std::max(0.05f, (std::abs(scale.x) + std::abs(scale.z)) * 0.25f);
        cylinder.halfHeight = std::max(0.05f, std::abs(scale.y) * 0.5f);
        break;
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        auto& spawn = std::get<LevelPlayerSpawn>(object->payload);
        spawn.position = glm::vec3(localMatrix[3]);
        break;
    }
    case EditorSceneObjectKind::Archetype: {
        auto& archetype = std::get<LevelArchetypePlacement>(object->payload);
        if (!decomposeTransformMatrix(localMatrix, position, rotation, scale)) {
            return false;
        }
        archetype.position = position;
        archetype.yawDegrees = rotation.y;
        break;
    }
    }

    markSceneDirty();
    return true;
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
    std::vector<std::uint64_t> expandedIds = ids;
    for (std::size_t index = 0; index < expandedIds.size(); ++index) {
        for (const auto childId : childObjectIds(expandedIds[index])) {
            if (std::find(expandedIds.begin(), expandedIds.end(), childId) == expandedIds.end()) {
                expandedIds.push_back(childId);
            }
        }
    }
    objects_.erase(
        std::remove_if(objects_.begin(), objects_.end(), [&](const EditorSceneObject& object) {
            return std::find(expandedIds.begin(), expandedIds.end(), object.id) != expandedIds.end();
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
    EditorSceneObject object{objectId, kind, payload};
    if (std::string* nodeId = nodeIdPtr(object)) {
        if (nodeId->empty() || findObjectIdByNodeId(*nodeId) != 0) {
            nodeId->clear();
        }
    }
    objects_.push_back(std::move(object));
    ensureObjectNodeId(objects_.back());
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

std::uint64_t EditorSceneDocument::findObjectIdByNodeId(const std::string& nodeId) const {
    if (nodeId.empty()) {
        return 0;
    }
    for (const auto& object : objects_) {
        const std::string* objectNodeId = nodeIdPtr(object);
        if (objectNodeId != nullptr && *objectNodeId == nodeId) {
            return object.id;
        }
    }
    return 0;
}

std::string* EditorSceneDocument::nodeIdPtr(EditorSceneObject& object) {
    return std::visit([](auto& placement) -> std::string* { return &placement.nodeId; }, object.payload);
}

const std::string* EditorSceneDocument::nodeIdPtr(const EditorSceneObject& object) const {
    return std::visit([](const auto& placement) -> const std::string* { return &placement.nodeId; }, object.payload);
}

std::string* EditorSceneDocument::parentNodeIdPtr(EditorSceneObject& object) {
    return std::visit([](auto& placement) -> std::string* { return &placement.parentNodeId; }, object.payload);
}

const std::string* EditorSceneDocument::parentNodeIdPtr(const EditorSceneObject& object) const {
    return std::visit([](const auto& placement) -> const std::string* { return &placement.parentNodeId; }, object.payload);
}

std::string EditorSceneDocument::ensureObjectNodeId(EditorSceneObject& object) {
    std::string* nodeId = nodeIdPtr(object);
    if (nodeId == nullptr) {
        return {};
    }
    if (nodeId->empty()) {
        *nodeId = "node_" + std::to_string(object.id);
    }
    return *nodeId;
}

glm::mat4 EditorSceneDocument::localTransformMatrix(const EditorSceneObject& object) const {
    switch (object.kind) {
    case EditorSceneObjectKind::Mesh: {
        const auto& mesh = std::get<LevelMeshPlacement>(object.payload);
        return makeTransformMatrix(mesh.position, mesh.rotation, mesh.scale);
    }
    case EditorSceneObjectKind::Light: {
        const auto& light = std::get<LevelLightPlacement>(object.payload);
        if (light.type == LightType::Directional) {
            return glm::mat4(1.0f);
        }
        return makeTransformMatrix(light.position, glm::vec3(0.0f), glm::vec3(1.0f));
    }
    case EditorSceneObjectKind::BoxCollider: {
        const auto& box = std::get<LevelBoxColliderPlacement>(object.payload);
        return makeTransformMatrix(box.position, box.rotation, box.halfExtents * 2.0f);
    }
    case EditorSceneObjectKind::CylinderCollider: {
        const auto& cylinder = std::get<LevelCylinderColliderPlacement>(object.payload);
        return makeTransformMatrix(cylinder.position,
                                   cylinder.rotation,
                                   glm::vec3(cylinder.radius * 2.0f, cylinder.halfHeight * 2.0f, cylinder.radius * 2.0f));
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        const auto& spawn = std::get<LevelPlayerSpawn>(object.payload);
        return makeTransformMatrix(spawn.position, glm::vec3(0.0f), glm::vec3(1.0f));
    }
    case EditorSceneObjectKind::Archetype: {
        const auto& archetype = std::get<LevelArchetypePlacement>(object.payload);
        return makeTransformMatrix(archetype.position, glm::vec3(0.0f, archetype.yawDegrees, 0.0f), glm::vec3(1.0f));
    }
    }
    return glm::mat4(1.0f);
}

glm::mat4 EditorSceneDocument::localParentMatrix(const EditorSceneObject& object) const {
    const std::string* parentNodeId = parentNodeIdPtr(object);
    if (parentNodeId == nullptr || parentNodeId->empty()) {
        return glm::mat4(1.0f);
    }
    const std::uint64_t parentId = findObjectIdByNodeId(*parentNodeId);
    if (parentId == 0) {
        return glm::mat4(1.0f);
    }
    return worldTransformMatrix(parentId);
}

glm::mat3 EditorSceneDocument::localParentRotationMatrix(const EditorSceneObject& object) const {
    return extractRotationMatrix(localParentMatrix(object));
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
