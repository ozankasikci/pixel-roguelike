#include "editor/ui/EditorPanels.h"

#include "editor/assets/EditorAssetBrowser.h"
#include "engine/core/PathUtils.h"
#include "game/content/ContentRegistry.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <imgui.h>

namespace {

struct AssetBrowserVisibleRow {
    const EditorAssetBrowserNode* node = nullptr;
    std::string path;
    std::string parentPath;
    bool directory = false;
    int depth = 0;
};

struct AssetBrowserSession {
    std::vector<EditorAssetBrowserNode> cachedNodes;
    bool cacheValid = false;
    std::string pendingScrollPath;
};

AssetBrowserSession& assetBrowserSession() {
    static AssetBrowserSession session;
    return session;
}

const std::vector<EditorAssetBrowserNode>& cachedAssetNodes(bool forceRefresh = false) {
    AssetBrowserSession& session = assetBrowserSession();
    if (!session.cacheValid || forceRefresh) {
        session.cachedNodes = buildProjectAssetBrowserTree();
        session.cacheValid = true;
    }
    return session.cachedNodes;
}

void requestAssetScroll(const std::string& path) {
    assetBrowserSession().pendingScrollPath = path;
}

bool consumeAssetScrollRequest(const std::string& path) {
    AssetBrowserSession& session = assetBrowserSession();
    if (session.pendingScrollPath != path) {
        return false;
    }
    session.pendingScrollPath.clear();
    return true;
}

void setSelectedAsset(EditorUiState& ui, const EditorAssetBrowserNode& node) {
    if (ui.selectedAssetPath == node.relativePath
        && ui.inspectedAsset.absolutePath == node.absolutePath
        && ui.inspectedAsset.kind == node.kind
        && ui.inspectedAsset.directory == node.directory) {
        ui.inspectorContext = EditorInspectorContext::AssetSelection;
        return;
    }

    ui.selectedAssetPath = node.relativePath;
    requestAssetScroll(ui.selectedAssetPath);
    ui.inspectorContext = EditorInspectorContext::AssetSelection;
    ui.inspectedAsset.relativePath = node.relativePath;
    ui.inspectedAsset.absolutePath = node.absolutePath;
    ui.inspectedAsset.kind = node.kind;
    ui.inspectedAsset.directory = node.directory;
    ui.inspectedAsset.declaredId = node.declaredId;
    ui.inspectedAsset.meshId = node.meshId;
}

void buildVisibleRows(const std::vector<EditorAssetBrowserNode>& nodes,
                      const std::unordered_set<std::string>& expandedPaths,
                      std::vector<AssetBrowserVisibleRow>& rows,
                      const std::string& parentPath,
                      int depth) {
    for (const auto& node : nodes) {
        rows.push_back(AssetBrowserVisibleRow{
            .node = &node,
            .path = node.relativePath,
            .parentPath = parentPath,
            .directory = node.directory,
            .depth = depth,
        });
        if (node.directory && expandedPaths.contains(node.relativePath)) {
            buildVisibleRows(node.children, expandedPaths, rows, node.relativePath, depth + 1);
        }
    }
}

int findVisibleRowIndex(const std::vector<AssetBrowserVisibleRow>& rows, const std::string& path) {
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (rows[index].path == path) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

const AssetBrowserVisibleRow* findVisibleRow(const std::vector<AssetBrowserVisibleRow>& rows, const std::string& path) {
    const int index = findVisibleRowIndex(rows, path);
    if (index < 0) {
        return nullptr;
    }
    return &rows[static_cast<std::size_t>(index)];
}

void ensureValidAssetSelection(EditorUiState& ui, const std::vector<AssetBrowserVisibleRow>& rows) {
    if (rows.empty()) {
        ui.selectedAssetPath = "assets";
        return;
    }
    if (findVisibleRowIndex(rows, ui.selectedAssetPath) >= 0) {
        return;
    }
    ui.selectedAssetPath = rows.front().path;
    requestAssetScroll(ui.selectedAssetPath);
}

bool shouldHandleAssetBrowserKeys() {
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive()
        && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
}

void selectAdjacentAsset(EditorUiState& ui,
                         const std::vector<AssetBrowserVisibleRow>& rows,
                         int delta) {
    if (rows.empty()) {
        return;
    }
    int index = findVisibleRowIndex(rows, ui.selectedAssetPath);
    if (index < 0) {
        ui.selectedAssetPath = rows.front().path;
        requestAssetScroll(ui.selectedAssetPath);
        return;
    }
    index = std::clamp(index + delta, 0, static_cast<int>(rows.size()) - 1);
    ui.selectedAssetPath = rows[static_cast<std::size_t>(index)].path;
    if (const AssetBrowserVisibleRow* selected = findVisibleRow(rows, ui.selectedAssetPath)) {
        if (selected->node != nullptr) {
            setSelectedAsset(ui, *selected->node);
        }
    }
}

void handleHorizontalAssetNavigation(EditorUiState& ui,
                                     const std::vector<AssetBrowserVisibleRow>& rows,
                                     ImGuiKey key) {
    const AssetBrowserVisibleRow* selected = findVisibleRow(rows, ui.selectedAssetPath);
    if (selected == nullptr) {
        return;
    }

    if (key == ImGuiKey_RightArrow) {
        if (selected->directory) {
            if (!ui.expandedAssetPaths.contains(selected->path)) {
                ui.expandedAssetPaths.insert(selected->path);
                return;
            }

            const int index = findVisibleRowIndex(rows, selected->path);
            const int nextIndex = index + 1;
            if (nextIndex >= 0 && nextIndex < static_cast<int>(rows.size())
                && rows[static_cast<std::size_t>(nextIndex)].parentPath == selected->path) {
                ui.selectedAssetPath = rows[static_cast<std::size_t>(nextIndex)].path;
                if (rows[static_cast<std::size_t>(nextIndex)].node != nullptr) {
                    setSelectedAsset(ui, *rows[static_cast<std::size_t>(nextIndex)].node);
                }
            }
        }
        return;
    }

    if (key == ImGuiKey_LeftArrow) {
        if (selected->directory && ui.expandedAssetPaths.contains(selected->path)) {
            ui.expandedAssetPaths.erase(selected->path);
            return;
        }
        if (!selected->parentPath.empty()) {
            ui.selectedAssetPath = selected->parentPath;
            if (const AssetBrowserVisibleRow* parent = findVisibleRow(rows, ui.selectedAssetPath)) {
                if (parent->node != nullptr) {
                    setSelectedAsset(ui, *parent->node);
                }
            }
        }
    }
}

void renderAssetBrowserCreateContextMenu(EditorPlacementState& placementState) {
    if (ImGui::MenuItem("Place Point Light")) {
        beginPlacement(placementState, EditorPlacementKind::PointLight);
    }
    if (ImGui::MenuItem("Place Spot Light")) {
        beginPlacement(placementState, EditorPlacementKind::SpotLight);
    }
    if (ImGui::MenuItem("Add Directional Light")) {
        beginPlacement(placementState, EditorPlacementKind::DirectionalLight);
    }
    if (ImGui::MenuItem("Place Box Collider")) {
        beginPlacement(placementState, EditorPlacementKind::BoxCollider);
    }
    if (ImGui::MenuItem("Place Cylinder Collider")) {
        beginPlacement(placementState, EditorPlacementKind::CylinderCollider);
    }
    if (ImGui::MenuItem("Place Player Spawn")) {
        beginPlacement(placementState, EditorPlacementKind::PlayerSpawn);
    }
}

} // namespace

AssetBrowserActionResult renderAssetBrowser(EditorUiState& ui,
                                            EditorPlacementState& placementState,
                                            EditorSceneDocument& document,
                                            const std::vector<std::uint64_t>& selectedIds,
                                            const ContentRegistry& content,
                                            const std::vector<std::string>& meshIds,
                                            const std::vector<std::string>& materialIds,
                                            const std::vector<std::string>& archetypeIds,
                                            const std::vector<std::filesystem::path>& externalDropPaths,
                                            bool* open,
                                            EditorCommandStack& commandStack) {
    AssetBrowserActionResult result;
    if (open != nullptr && !*open) {
        return result;
    }
    if (!ImGui::Begin("Asset Browser", open)) {
        ImGui::End();
        return result;
    }

    const auto meshObjects = selectedMeshObjects(document, selectedIds);
    bool refreshAssetTree = false;
    if (ImGui::Button("Refresh")) {
        refreshAssetTree = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("F5 to rescan assets");
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F5)) {
        refreshAssetTree = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Drop .glb/.gltf/.fbx here to import");
    ImGui::Separator();

    if (!externalDropPaths.empty()) {
        const bool selectedIsDirectory = ui.inspectorContext == EditorInspectorContext::AssetSelection
            ? ui.inspectedAsset.directory
            : (ui.selectedAssetPath == "assets");
        const auto importedAssets = importEditorExternalAssets(externalDropPaths,
                                                               resolveProjectPath("assets"),
                                                               ui.selectedAssetPath,
                                                               selectedIsDirectory);
        result.consumedExternalDrops = true;
        if (!importedAssets.empty()) {
            refreshAssetTree = true;
            result.assetCatalogChanged = true;
            const EditorImportedAsset& imported = importedAssets.back();
            ui.selectedAssetPath = imported.relativePath;
            requestAssetScroll(ui.selectedAssetPath);
            ui.inspectorContext = EditorInspectorContext::AssetSelection;
            ui.inspectedAsset.relativePath = imported.relativePath;
            ui.inspectedAsset.absolutePath = imported.absolutePath;
            ui.inspectedAsset.kind = imported.kind;
            ui.inspectedAsset.directory = imported.directory;
            ui.inspectedAsset.declaredId = imported.declaredId;
            ui.inspectedAsset.meshId = imported.meshId;
        }
    }

    const auto& assetNodes = cachedAssetNodes(refreshAssetTree);
    std::vector<AssetBrowserVisibleRow> visibleRows;
    visibleRows.reserve(128);
    buildVisibleRows(assetNodes, ui.expandedAssetPaths, visibleRows, "assets", 1);
    ensureValidAssetSelection(ui, visibleRows);
    if (ui.inspectorContext == EditorInspectorContext::AssetSelection) {
        if (const AssetBrowserVisibleRow* selected = findVisibleRow(visibleRows, ui.selectedAssetPath)) {
            if (selected->node != nullptr) {
                setSelectedAsset(ui, *selected->node);
            }
        }
    }

    auto applyMaterialFromAsset = [&](const std::string& materialId) {
        ui.selectedMaterialId = materialId;
        const EditorSceneDocumentState beforeState = document.captureState();
        if (applyMaterialToMeshes(meshObjects, materialId, content, document)) {
            commandStack.pushDocumentStateCommand(
                meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                beforeState,
                document.captureState(),
                document);
            result.previewDirty = true;
        }
    };

    std::function<void(const EditorAssetBrowserNode&)> renderNode = [&](const EditorAssetBrowserNode& node) {
        ImGui::PushID(node.relativePath.c_str());

        if (node.directory) {
            ImGui::SetNextItemOpen(ui.expandedAssetPaths.contains(node.relativePath), ImGuiCond_Always);
            const bool openNode = ImGui::TreeNodeEx(node.name.c_str(),
                                                    ImGuiTreeNodeFlags_SpanAvailWidth
                                                        | ImGuiTreeNodeFlags_OpenOnArrow
                                                        | (ui.selectedAssetPath == node.relativePath ? ImGuiTreeNodeFlags_Selected : 0));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                setSelectedAsset(ui, node);
            }
            if (ImGui::IsItemToggledOpen()) {
                if (openNode) {
                    ui.expandedAssetPaths.insert(node.relativePath);
                } else {
                    ui.expandedAssetPaths.erase(node.relativePath);
                }
            }
            if (consumeAssetScrollRequest(node.relativePath)) {
                ImGui::SetScrollHereY(0.5f);
            }
            if (ImGui::BeginPopupContextItem("AssetFolderContext")) {
                setSelectedAsset(ui, node);
                renderAssetBrowserCreateContextMenu(placementState);
                ImGui::EndPopup();
            }
            if (openNode) {
                for (const auto& child : node.children) {
                    renderNode(child);
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            return;
        }

        const bool selected = (ui.selectedAssetPath == node.relativePath);
        if (ImGui::Selectable(node.name.c_str(), selected)) {
            setSelectedAsset(ui, node);
        }
        if (consumeAssetScrollRequest(node.relativePath)) {
            ImGui::SetScrollHereY(0.5f);
        }

        const std::string& declaredId = node.declaredId;
        const std::string& meshId = node.meshId;
        const bool meshPlaceable = node.kind == EditorAssetBrowserKind::Mesh && containsString(meshIds, meshId);
        const bool materialKnown = !declaredId.empty() && containsString(materialIds, declaredId);
        const bool archetypeKnown = !declaredId.empty() && containsString(archetypeIds, declaredId);

        if (node.kind == EditorAssetBrowserKind::Scene && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            result.openScenePath = node.relativePath;
        } else if (node.kind == EditorAssetBrowserKind::Environment
                   && ImGui::IsItemHovered()
                   && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
                   && !declaredId.empty()) {
            const EditorSceneDocumentState beforeState = document.captureState();
            document.setEnvironmentId(declaredId, content);
            commandStack.pushDocumentStateCommand(
                "Change Environment",
                beforeState,
                document.captureState(),
                document);
            result.previewDirty = true;
        }

        if (meshPlaceable) {
            emitPlacementDragSource(EditorPlacementKind::Mesh, meshId, ui.selectedMaterialId);
        } else if (node.kind == EditorAssetBrowserKind::Prefab && archetypeKnown) {
            emitPlacementDragSource(EditorPlacementKind::Archetype, declaredId);
        }

        if (ImGui::BeginPopupContextItem("AssetFileContext")) {
            setSelectedAsset(ui, node);
            switch (node.kind) {
            case EditorAssetBrowserKind::Scene:
                if (ImGui::MenuItem("Open Scene")) {
                    result.openScenePath = node.relativePath;
                }
                break;
            case EditorAssetBrowserKind::Environment:
                if (!declaredId.empty() && ImGui::MenuItem("Load Environment")) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    document.setEnvironmentId(declaredId, content);
                    commandStack.pushDocumentStateCommand(
                        "Change Environment",
                        beforeState,
                        document.captureState(),
                        document);
                    result.previewDirty = true;
                }
                break;
            case EditorAssetBrowserKind::Mesh:
                if (meshPlaceable && ImGui::MenuItem("Place Mesh")) {
                    ui.selectedMeshId = meshId;
                    beginPlacement(placementState, EditorPlacementKind::Mesh, meshId, ui.selectedMaterialId);
                }
                if (meshPlaceable && ImGui::MenuItem("Set As Active Mesh")) {
                    ui.selectedMeshId = meshId;
                }
                break;
            case EditorAssetBrowserKind::Material:
                if (materialKnown && ImGui::MenuItem("Set As Active Material")) {
                    ui.selectedMaterialId = declaredId;
                }
                if (materialKnown) {
                    ImGui::BeginDisabled(meshObjects.empty());
                    if (ImGui::MenuItem(meshObjects.size() == 1 ? "Apply To Selected Mesh" : "Apply To Selected Meshes")) {
                        applyMaterialFromAsset(declaredId);
                    }
                    ImGui::EndDisabled();
                }
                break;
            case EditorAssetBrowserKind::Prefab:
                if (archetypeKnown && ImGui::MenuItem("Place Archetype")) {
                    ui.selectedArchetypeId = declaredId;
                    beginPlacement(placementState, EditorPlacementKind::Archetype, declaredId);
                }
                break;
            default:
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    };

    if (shouldHandleAssetBrowserKeys()) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            selectAdjacentAsset(ui, visibleRows, -1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            selectAdjacentAsset(ui, visibleRows, 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            handleHorizontalAssetNavigation(ui, visibleRows, ImGuiKey_RightArrow);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            handleHorizontalAssetNavigation(ui, visibleRows, ImGuiKey_LeftArrow);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            if (const AssetBrowserVisibleRow* selected = findVisibleRow(visibleRows, ui.selectedAssetPath)) {
                if (selected->directory) {
                    if (ui.expandedAssetPaths.contains(selected->path)) {
                        ui.expandedAssetPaths.erase(selected->path);
                    } else {
                        ui.expandedAssetPaths.insert(selected->path);
                    }
                } else if (selected->node != nullptr) {
                    setSelectedAsset(ui, *selected->node);
                    if (selected->node->kind == EditorAssetBrowserKind::Scene) {
                        result.openScenePath = selected->node->relativePath;
                    } else if (selected->node->kind == EditorAssetBrowserKind::Environment && !selected->node->declaredId.empty()) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        document.setEnvironmentId(selected->node->declaredId, content);
                        commandStack.pushDocumentStateCommand(
                            "Change Environment",
                            beforeState,
                            document.captureState(),
                            document);
                        result.previewDirty = true;
                    }
                }
            }
        }
    }

    if (ImGui::TreeNodeEx("assets", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        for (const auto& node : assetNodes) {
            renderNode(node);
        }
        ImGui::TreePop();
    }

    if (ImGui::BeginPopupContextWindow("AssetBrowserContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        renderAssetBrowserCreateContextMenu(placementState);
        ImGui::EndPopup();
    }

    if (placementState.active()) {
        ImGui::Separator();
        ImGui::Text("Placement: active");
        if (ImGui::Button("Cancel Placement")) {
            placementState.clear();
        }
    }

    result.assetCatalogChanged = result.assetCatalogChanged || refreshAssetTree;
    ImGui::End();
    return result;
}
