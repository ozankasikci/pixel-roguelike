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
    static constexpr const char* kKinds[] = {"checkpoint"};
    int kindIndex = 0;
    renderInspectorPropertyRow("Type", [&]() { return ImGui::Combo("##value", &kindIndex, kKinds, 1); },
                               EditorInspectorFieldKind::Enum);

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
