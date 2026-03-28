#include "editor/EditorCommand.h"

#include "game/level/LevelDef.h"
#include "game/rendering/EnvironmentDefinition.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

LevelDef makeLevelDefFromState(const EditorSceneDocumentState& state) {
    LevelDef level;
    level.environmentId = state.environment.id;
    level.environmentProfile = state.legacyEnvironmentProfile;
    if (level.environmentId.empty()) {
        level.environmentId = environmentProfileName(level.environmentProfile);
    }

    for (const auto& object : state.objects) {
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

} // namespace

EditorDocumentStateCommand::EditorDocumentStateCommand(std::string label,
                                                       EditorSceneDocumentState beforeState,
                                                       EditorSceneDocumentState afterState)
    : label_(std::move(label))
    , beforeState_(std::move(beforeState))
    , afterState_(std::move(afterState)) {}

void EditorDocumentStateCommand::undo(EditorSceneDocument& document) const {
    document.restoreState(beforeState_);
}

void EditorDocumentStateCommand::redo(EditorSceneDocument& document) const {
    document.restoreState(afterState_);
}

void EditorCommandStack::reset(EditorSceneDocument& document) {
    commands_.clear();
    cursor_ = 0;
    savedSignature_ = makeSignature(document.captureState());
    syncDirtyFlags(document);
}

void EditorCommandStack::markSaved(EditorSceneDocument& document) {
    savedSignature_ = makeSignature(document.captureState());
    syncDirtyFlags(document);
}

bool EditorCommandStack::canUndo() const {
    return cursor_ > 0;
}

bool EditorCommandStack::canRedo() const {
    return cursor_ < commands_.size();
}

const char* EditorCommandStack::undoLabel() const {
    return canUndo() ? commands_[cursor_ - 1]->label().c_str() : "";
}

const char* EditorCommandStack::redoLabel() const {
    return canRedo() ? commands_[cursor_]->label().c_str() : "";
}

bool EditorCommandStack::undo(EditorSceneDocument& document) {
    if (!canUndo()) {
        return false;
    }

    --cursor_;
    commands_[cursor_]->undo(document);
    syncDirtyFlags(document);
    return true;
}

bool EditorCommandStack::redo(EditorSceneDocument& document) {
    if (!canRedo()) {
        return false;
    }

    commands_[cursor_]->redo(document);
    ++cursor_;
    syncDirtyFlags(document);
    return true;
}

bool EditorCommandStack::pushDocumentStateCommand(std::string label,
                                                  const EditorSceneDocumentState& beforeState,
                                                  const EditorSceneDocumentState& afterState,
                                                  EditorSceneDocument& document) {
    const SavedSignature beforeSignature = makeSignature(beforeState);
    const SavedSignature afterSignature = makeSignature(afterState);
    if (beforeSignature.scene == afterSignature.scene
        && beforeSignature.environment == afterSignature.environment) {
        syncDirtyFlags(document);
        return false;
    }

    if (cursor_ < commands_.size()) {
        commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
    }

    commands_.push_back(std::make_unique<EditorDocumentStateCommand>(
        std::move(label), beforeState, afterState));
    cursor_ = commands_.size();
    trimToLimit();
    syncDirtyFlags(document);
    return true;
}

EditorCommandStack::SavedSignature EditorCommandStack::makeSignature(const EditorSceneDocumentState& state) {
    const LevelDef level = makeLevelDefFromState(state);
    SavedSignature signature;
    signature.scene = state.scenePath + "\n" + serializeLevelDef(level);
    signature.environment = serializeEnvironmentDefinitionAsset(state.environment);
    return signature;
}

void EditorCommandStack::syncDirtyFlags(EditorSceneDocument& document) const {
    const SavedSignature currentSignature = makeSignature(document.captureState());
    document.setDirtyFlags(currentSignature.scene != savedSignature_.scene,
                           currentSignature.environment != savedSignature_.environment);
}

void EditorCommandStack::trimToLimit() {
    if (commands_.size() <= kMaxCommands) {
        return;
    }

    const std::size_t removeCount = commands_.size() - kMaxCommands;
    commands_.erase(commands_.begin(), commands_.begin() + static_cast<std::ptrdiff_t>(removeCount));
    cursor_ = std::min(cursor_, commands_.size());
}
