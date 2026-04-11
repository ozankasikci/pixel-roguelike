#include "editor/ui/LevelEditorUi.h"

#include "game/content/ContentRegistry.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <imgui.h>

namespace {

struct InspectorLayoutMetrics {
    float labelWidth = 108.0f;
};

thread_local std::vector<InspectorLayoutMetrics> gInspectorLayoutStack;

void copyPayloadString(char (&dst)[64], const std::string& src) {
    std::snprintf(dst, sizeof(dst), "%s", src.c_str());
}

InspectorLayoutMetrics currentInspectorLayout() {
    if (!gInspectorLayoutStack.empty()) {
        return gInspectorLayoutStack.back();
    }
    return {};
}

float inspectorFieldWidth(EditorInspectorFieldKind kind, bool wideMode) {
    (void)wideMode;
    switch (kind) {
    case EditorInspectorFieldKind::Toggle:
        return 0.0f;
    case EditorInspectorFieldKind::Scalar:
    case EditorInspectorFieldKind::Enum:
    case EditorInspectorFieldKind::Text:
    case EditorInspectorFieldKind::Vector:
    case EditorInspectorFieldKind::Color:
        return FLT_MAX;
    }
    return FLT_MAX;
}

} // namespace

bool beginCompactEditorPanelWindow(const char* name,
                                   bool* open,
                                   ImGuiWindowFlags flags) {
    const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(std::max(3.0f, framePadding.x - 1.0f),
                               std::max(2.0f, framePadding.y - 1.0f)));
    const bool visible = ImGui::Begin(name, open, flags);
    ImGui::PopStyleVar();
    return visible;
}

bool beginInspectorPropertyTable(const char* id,
                                 float labelColumnFraction,
                                 float minLabelWidth,
                                 float maxLabelWidth) {
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float labelWidth = std::clamp(availableWidth * labelColumnFraction,
                                        minLabelWidth,
                                        maxLabelWidth);
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX)) {
        return false;
    }
    gInspectorLayoutStack.push_back(InspectorLayoutMetrics{labelWidth});
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, labelWidth);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    return true;
}

void endInspectorPropertyTable() {
    if (!gInspectorLayoutStack.empty()) {
        gInspectorLayoutStack.pop_back();
    }
    ImGui::EndTable();
}

void applyInspectorFieldWidth(EditorInspectorFieldKind kind) {
    if (kind == EditorInspectorFieldKind::Toggle) {
        return;
    }
    const InspectorLayoutMetrics metrics = currentInspectorLayout();
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float clampedWidth = std::min(availableWidth, inspectorFieldWidth(kind, false));
    ImGui::SetNextItemWidth(clampedWidth > 0.0f ? clampedWidth : availableWidth);
}

void beginInspectorPropertyLabel(const char* label, EditorInspectorFieldKind kind) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    applyInspectorFieldWidth(kind);
    ImGui::PushID(label);
}

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

    bool changed = false;
    for (EditorSceneObject* object : meshObjects) {
        if (object == nullptr || object->kind != EditorSceneObjectKind::Mesh) {
            continue;
        }
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        if (mesh.materialId == materialId) {
            continue;
        }
        mesh.materialId = materialId;
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
    const std::string firstMaterial = firstMesh.materialId.empty() ? "stone_default" : firstMesh.materialId;

    for (std::size_t index = 1; index < meshObjects.size(); ++index) {
        const auto& mesh = std::get<LevelMeshPlacement>(meshObjects[index]->payload);
        const std::string materialId = mesh.materialId.empty() ? "stone_default" : mesh.materialId;
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
    applyInspectorFieldWidth(EditorInspectorFieldKind::Vector);
    return ImGui::DragFloat3(label, &value.x, speed, -1000.0f, 1000.0f, "%.2f");
}

bool editColor(const char* label, glm::vec3& value) {
    applyInspectorFieldWidth(EditorInspectorFieldKind::Color);
    return ImGui::ColorEdit3(label, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
}

bool editString(const char* label, std::string& value, const char* hint) {
    applyInspectorFieldWidth(EditorInspectorFieldKind::Text);
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
    case EditorPlacementKind::Checkpoint:
        break;
    case EditorPlacementKind::Archetype:
        state.archetypeId = primaryId;
        break;
    case EditorPlacementKind::Collider:
        // primaryId encodes collider shape: "box", "cylinder", "sphere", "capsule"
        state.meshId = primaryId.empty() ? "box" : primaryId;
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

void emitMaterialDragSource(const std::string& materialId) {
    if (!ImGui::BeginDragDropSource()) {
        return;
    }
    EditorMaterialDragPayload payload;
    copyPayloadString(payload.materialId, materialId);
    ImGui::SetDragDropPayload("EDITOR_MATERIAL", &payload, sizeof(payload));
    ImGui::TextUnformatted(materialId.c_str());
    ImGui::EndDragDropSource();
}
