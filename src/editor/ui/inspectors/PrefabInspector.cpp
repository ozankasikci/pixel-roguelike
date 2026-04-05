#include "editor/ui/inspectors/PrefabInspector.h"

#include "editor/ui/LevelEditorUi.h"
#include "game/content/ContentRegistry.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace {

bool renderPrefabDraftFields(GameplayArchetypeDefinition& prefab) {
    bool dirty = false;
    if (!beginInspectorPropertyTable("PrefabDraftFields")) {
        return false;
    }
    dirty |= renderInspectorPropertyRow("Id", [&]() { return editString("##value", prefab.id); });
    static constexpr const char* kKinds[] = {"checkpoint", "double_door"};
    int kindIndex = prefab.kind == GameplayArchetypeKind::Checkpoint ? 0 : 1;
    if (renderInspectorPropertyRow("Type", [&]() { return ImGui::Combo("##value", &kindIndex, kKinds, 2); },
                                  EditorInspectorFieldKind::Enum)) {
        prefab.kind = kindIndex == 0 ? GameplayArchetypeKind::Checkpoint : GameplayArchetypeKind::DoubleDoor;
        dirty = true;
    }

    switch (prefab.kind) {
    case GameplayArchetypeKind::Checkpoint:
        dirty |= renderInspectorPropertyRow("Position", [&]() { return editVec3("##value", prefab.checkpoint.position); });
        dirty |= renderInspectorPropertyRow("Respawn Position", [&]() { return editVec3("##value", prefab.checkpoint.respawnPosition); });
        dirty |= renderInspectorPropertyRow("Interact Distance", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.interactDistance, 0.05f, 0.1f, 20.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Interact Dot", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.interactDotThreshold, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Light Position", [&]() { return editVec3("##value", prefab.checkpoint.lightPosition); });
        dirty |= renderInspectorPropertyRow("Light Color", [&]() { return editColor("##value", prefab.checkpoint.lightColor); });
        dirty |= renderInspectorPropertyRow("Light Radius", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.lightRadius, 0.05f, 0.1f, 30.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Light Intensity", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.lightIntensity, 0.01f, 0.0f, 10.0f, "%.2f"); });
        break;
    case GameplayArchetypeKind::DoubleDoor:
        dirty |= renderInspectorPropertyRow("Left Leaf Mesh", [&]() { return editString("##value", prefab.doubleDoor.leftLeafMeshName); });
        dirty |= renderInspectorPropertyRow("Right Leaf Mesh", [&]() { return editString("##value", prefab.doubleDoor.rightLeafMeshName); });
        dirty |= renderInspectorPropertyRow("Root Position", [&]() { return editVec3("##value", prefab.doubleDoor.rootPosition); });
        dirty |= renderInspectorPropertyRow("Left Hinge", [&]() { return editVec3("##value", prefab.doubleDoor.leftHingePosition); });
        dirty |= renderInspectorPropertyRow("Right Hinge", [&]() { return editVec3("##value", prefab.doubleDoor.rightHingePosition); });
        dirty |= renderInspectorPropertyRow("Leaf Scale", [&]() { return editVec3("##value", prefab.doubleDoor.leafScale, 0.02f); });
        dirty |= renderInspectorPropertyRow("Closed Yaw", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.closedYaw, 0.5f, -360.0f, 360.0f, "%.1f"); });
        dirty |= renderInspectorPropertyRow("Open Angle", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.openAngle, 0.5f, 1.0f, 180.0f, "%.1f"); });
        dirty |= renderInspectorPropertyRow("Interact Distance", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.interactDistance, 0.05f, 0.1f, 20.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Interact Dot", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.interactDotThreshold, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Open Duration", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.openDuration, 0.01f, 0.1f, 10.0f, "%.2f"); });
        break;
    }
    endInspectorPropertyTable();
    return dirty;
}

} // namespace

void drawPrefabAssetInspector(EditorUiState& ui,
                               const EditorInspectedAsset& asset,
                               AssetInspectorSession& session,
                               ContentRegistry& content,
                               InspectorActionResult& result) {
    if (!session.prefabDraft.has_value()) {
        ImGui::TextUnformatted("Failed to load prefab asset.");
        return;
    }

    GameplayArchetypeDefinition& prefab = *session.prefabDraft;
    if (ImGui::Button("Save Prefab")) {
        try {
            saveGameplayArchetypeAsset(asset.absolutePath, prefab);
            content.loadDefaults();
            session.prefabDirty = false;
            ui.inspectedAsset.declaredId = prefab.id;
            result.contentReloaded = true;
            result.previewDirty = true;
        } catch (const std::exception& ex) {
            spdlog::error("Prefab save failed: {}", ex.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Prefab")) {
        try {
            prefab = loadGameplayArchetypeAsset(asset.absolutePath);
            session.prefabDirty = false;
        } catch (const std::exception& ex) {
            spdlog::error("Prefab reload failed: {}", ex.what());
        }
    }

    const bool dirty = renderPrefabDraftFields(prefab);
    if (dirty) {
        session.prefabDirty = true;
    }
    if (session.prefabDirty) {
        ImGui::TextDisabled("Unsaved changes");
    }
    ImGui::TextDisabled("Prefab preview is metadata-only in this first pass.");
}
