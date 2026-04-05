#include "editor/ui/EditorPanels.h"

#include "editor/ui/inspectors/AssetInspectorHelpers.h"
#include "editor/ui/inspectors/EnvironmentInspector.h"
#include "editor/ui/inspectors/MaterialInspector.h"
#include "editor/ui/inspectors/PrefabInspector.h"
#include "editor/ui/inspectors/SceneSelectionInspector.h"

#include <imgui.h>

#include <string>

// -----------------------------------------------------------------------
// renderInspector — public entry point
// -----------------------------------------------------------------------

InspectorActionResult renderInspector(EditorUiState& ui,
                                      EditorSceneDocument& document,
                                      const std::vector<std::uint64_t>& selectedIds,
                                      ContentRegistry& content,
                                      const std::vector<std::string>& meshIds,
                                      const std::vector<std::string>& materialIds,
                                      const std::vector<std::string>& archetypeIds,
                                      bool* open,
                                      EditorPendingCommand& pendingCommand,
                                      EditorCommandStack& commandStack) {
    InspectorActionResult result;
    if (open != nullptr && !*open) {
        return result;
    }
    if (!beginCompactEditorPanelWindow("Inspector", open)) {
        ImGui::End();
        return result;
    }

    AssetInspectorSession& session = assetInspectorSession();

    if (ui.inspectorContext == EditorInspectorContext::AssetSelection && !ui.inspectedAsset.relativePath.empty()) {
        syncAssetInspectorSession(ui.inspectedAsset, session);
        switch (ui.inspectedAsset.kind) {
        case EditorAssetBrowserKind::Scene:
            renderSceneAssetInspector(ui.inspectedAsset);
            break;
        case EditorAssetBrowserKind::Mesh:
            renderMeshAssetInspector(ui.inspectedAsset, session);
            break;
        case EditorAssetBrowserKind::Material:
            renderFileHeader(ui.inspectedAsset);
            renderFileMetadata(ui.inspectedAsset.absolutePath);
            drawMaterialAssetInspector(ui, ui.inspectedAsset, session, content, result);
            break;
        case EditorAssetBrowserKind::Environment:
            renderFileHeader(ui.inspectedAsset);
            renderFileMetadata(ui.inspectedAsset.absolutePath);
            drawEnvironmentAssetInspector(ui, ui.inspectedAsset, session, document, content, result);
            break;
        case EditorAssetBrowserKind::Prefab:
            renderFileHeader(ui.inspectedAsset);
            renderFileMetadata(ui.inspectedAsset.absolutePath);
            drawPrefabAssetInspector(ui, ui.inspectedAsset, session, content, result);
            break;
        case EditorAssetBrowserKind::Sky:
            renderSkyAssetInspector(ui.inspectedAsset, session);
            break;
        case EditorAssetBrowserKind::Shader:
            renderShaderAssetInspector(ui.inspectedAsset);
            break;
        case EditorAssetBrowserKind::Directory:
        case EditorAssetBrowserKind::Other:
            renderOtherAssetInspector(ui.inspectedAsset);
            break;
        }
    } else {
        renderSceneSelectionInspector(document, selectedIds, content,
                                      meshIds, materialIds, archetypeIds,
                                      pendingCommand, commandStack);
    }

    ImGui::End();
    return result;
}
