#include "editor/LevelEditorUi.h"

#include "game/content/ContentRegistry.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>

namespace {

void copyPayloadString(char (&dst)[64], const std::string& src) {
    std::snprintf(dst, sizeof(dst), "%s", src.c_str());
}

} // namespace

void beginPendingCommand(EditorPendingCommand& pending,
                         const EditorSceneDocumentState& beforeState,
                         std::string label) {
    if (pending.active) {
        return;
    }
    pending.active = true;
    pending.label = std::move(label);
    pending.beforeState = beforeState;
}

bool finalizePendingCommand(EditorPendingCommand& pending,
                            EditorCommandStack& commandStack,
                            EditorSceneDocument& document) {
    if (!pending.active) {
        return false;
    }
    const bool pushed = commandStack.pushDocumentStateCommand(
        pending.label, pending.beforeState, document.captureState(), document);
    pending.clear();
    return pushed;
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

std::vector<EditorSceneObject*> selectedMeshObjects(EditorSceneDocument& document,
                                                    const std::vector<std::uint64_t>& selectedIds) {
    std::vector<EditorSceneObject*> meshes;
    meshes.reserve(selectedIds.size());
    for (const std::uint64_t id : selectedIds) {
        EditorSceneObject* object = document.findObject(id);
        if (object != nullptr && object->kind == EditorSceneObjectKind::Mesh) {
            meshes.push_back(object);
        }
    }
    return meshes;
}

bool applyMaterialToMeshes(const std::vector<EditorSceneObject*>& meshObjects,
                           const std::string& materialId,
                           const ContentRegistry& content,
                           EditorSceneDocument& document) {
    if (meshObjects.empty()) {
        return false;
    }

    const MaterialKind materialKind = resolvePlacementMaterialKind(materialId, content);
    bool changed = false;
    for (EditorSceneObject* object : meshObjects) {
        if (object == nullptr || object->kind != EditorSceneObjectKind::Mesh) {
            continue;
        }
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        if (mesh.materialId == materialId && mesh.material == materialKind) {
            continue;
        }
        mesh.materialId = materialId;
        mesh.material = materialKind;
        changed = true;
    }

    if (changed) {
        document.markSceneDirty();
    }
    return changed;
}

std::string materialSelectionLabel(const std::vector<EditorSceneObject*>& meshObjects) {
    if (meshObjects.empty()) {
        return "No Mesh Selected";
    }

    const auto& firstMesh = std::get<LevelMeshPlacement>(meshObjects.front()->payload);
    const std::string firstMaterial = firstMesh.materialId.empty()
        ? std::string(defaultMaterialIdForKind(firstMesh.material.value_or(MaterialKind::Stone)))
        : firstMesh.materialId;

    for (std::size_t index = 1; index < meshObjects.size(); ++index) {
        const auto& mesh = std::get<LevelMeshPlacement>(meshObjects[index]->payload);
        const std::string materialId = mesh.materialId.empty()
            ? std::string(defaultMaterialIdForKind(mesh.material.value_or(MaterialKind::Stone)))
            : mesh.materialId;
        if (materialId != firstMaterial) {
            return "<mixed>";
        }
    }

    return firstMaterial;
}

void trackLastItemCommand(const EditorSceneDocumentState& beforeState,
                          std::string label,
                          EditorPendingCommand& pending,
                          EditorCommandStack& commandStack,
                          EditorSceneDocument& document) {
    const bool itemActive = ImGui::IsItemActive();
    const bool itemActivated = ImGui::IsItemActivated();
    const bool itemEdited = ImGui::IsItemEdited();
    const bool itemDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();

    if (itemActivated && !pending.active) {
        beginPendingCommand(pending, beforeState, label);
    }

    if (itemDeactivatedAfterEdit) {
        if (!pending.active) {
            beginPendingCommand(pending, beforeState, label);
        }
        finalizePendingCommand(pending, commandStack, document);
        return;
    }

    if (itemEdited && !itemActive) {
        commandStack.pushDocumentStateCommand(
            std::move(label), beforeState, document.captureState(), document);
        return;
    }

    if (pending.active && pending.label == label && !itemActive && !itemEdited) {
        pending.clear();
    }
}

bool editVec3(const char* label, glm::vec3& value, float speed) {
    return ImGui::DragFloat3(label, &value.x, speed, -1000.0f, 1000.0f, "%.2f");
}

bool editColor(const char* label, glm::vec3& value) {
    return ImGui::ColorEdit3(label, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
}

bool editString(const char* label, std::string& value, const char* hint) {
    char buffer[512]{};
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    bool changed = false;
    if (hint != nullptr && hint[0] != '\0') {
        changed = ImGui::InputTextWithHint(label, hint, buffer, sizeof(buffer));
    } else {
        changed = ImGui::InputText(label, buffer, sizeof(buffer));
    }
    if (changed) {
        value = buffer;
    }
    return changed;
}

MaterialKind resolvePlacementMaterialKind(const std::string& materialId, const ContentRegistry& content) {
    if (const auto* material = content.findMaterial(materialId)) {
        return resolveMaterialDefinition(material->id, content.materials()).shadingModel;
    }
    return MaterialKind::Stone;
}

void beginPlacement(EditorPlacementState& state,
                    EditorPlacementKind kind,
                    const std::string& primaryId,
                    const std::string& secondaryId) {
    state.clear();
    state.kind = kind;
    switch (kind) {
    case EditorPlacementKind::Mesh:
        state.meshId = primaryId;
        state.materialId = secondaryId;
        break;
    case EditorPlacementKind::Archetype:
        state.archetypeId = primaryId;
        break;
    default:
        break;
    }
}

void emitPlacementDragSource(EditorPlacementKind kind,
                             const std::string& primaryId,
                             const std::string& secondaryId) {
    if (!ImGui::BeginDragDropSource()) {
        return;
    }
    EditorDragPayload payload;
    payload.kind = kind;
    copyPayloadString(payload.primaryId, primaryId);
    copyPayloadString(payload.secondaryId, secondaryId);
    ImGui::SetDragDropPayload("EDITOR_PLACE", &payload, sizeof(payload));
    ImGui::TextUnformatted("Place in viewport");
    ImGui::EndDragDropSource();
}
