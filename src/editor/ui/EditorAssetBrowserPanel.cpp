#include "editor/ui/EditorPanels.h"

#include "editor/assets/EditorAssetBrowser.h"
#include "engine/core/PathUtils.h"
#include "engine/core/ProjectConfig.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"
#include "game/rendering/MaterialDefinition.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace {

void revealInFinder(const std::string& absolutePath) {
#ifdef __APPLE__
    std::string cmd = "open -R \"" + absolutePath + "\" &";
#elif defined(_WIN32)
    std::string cmd = "explorer /select,\"" + absolutePath + "\"";
#else
    std::string cmd = "xdg-open \"" + std::filesystem::path(absolutePath).parent_path().string() + "\" &";
#endif
    std::system(cmd.c_str());
}

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

struct SceneRenameState {
    std::string renamingPath;
    char nameBuffer[128] = {};
    bool focusRequested = false;
};

SceneRenameState& sceneRenameState() {
    static SceneRenameState state;
    return state;
}

struct MaterialCrudState {
    // New Material popup
    bool newPopupOpen = false;
    char newIdBuffer[128] = {};
    char newParentBuffer[128] = {};
    // Rename popup
    bool renamePopupOpen = false;
    std::string renamingId;
    std::string renamingAbsolutePath;
    char renameBuffer[128] = {};
    // Delete popup
    bool deletePopupOpen = false;
    std::string deletingId;
    std::string deletingAbsolutePath;
};

MaterialCrudState& materialCrudState() {
    static MaterialCrudState state;
    return state;
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
    if (ImGui::BeginMenu("Place Collider")) {
        if (ImGui::MenuItem("Box")) {
            beginPlacement(placementState, EditorPlacementKind::Collider, "box");
        }
        if (ImGui::MenuItem("Cylinder")) {
            beginPlacement(placementState, EditorPlacementKind::Collider, "cylinder");
        }
        if (ImGui::MenuItem("Sphere")) {
            beginPlacement(placementState, EditorPlacementKind::Collider, "sphere");
        }
        if (ImGui::MenuItem("Capsule")) {
            beginPlacement(placementState, EditorPlacementKind::Collider, "capsule");
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Place Player Spawn")) {
        beginPlacement(placementState, EditorPlacementKind::PlayerSpawn);
    }
    if (ImGui::MenuItem("Place Checkpoint")) {
        beginPlacement(placementState, EditorPlacementKind::Checkpoint);
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
    if (!beginCompactEditorPanelWindow("Asset Browser", open)) {
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
            if (node.name == "scenes") {
                ImGui::SameLine();
                if (ImGui::SmallButton("+##NewScene")) {
                    result.newSceneRequested = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("New Scene");
                }
            }
            if (node.name == "materials") {
                ImGui::SameLine();
                if (ImGui::SmallButton("+##NewMaterial")) {
                    MaterialCrudState& crud = materialCrudState();
                    crud.newPopupOpen = true;
                    crud.newIdBuffer[0] = '\0';
                    crud.newParentBuffer[0] = '\0';
                    ImGui::OpenPopup("NewMaterialPopup");
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("New Material");
                }
            }
            if (ImGui::BeginPopupContextItem("AssetFolderContext")) {
                setSelectedAsset(ui, node);
                if (ImGui::MenuItem("Reveal in Finder")) {
                    revealInFinder(node.absolutePath);
                }
                ImGui::Separator();
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
        SceneRenameState& renameState = sceneRenameState();
        const bool isRenaming = (node.kind == EditorAssetBrowserKind::Scene
                                 && renameState.renamingPath == node.relativePath);
        const bool isActiveScene = (node.kind == EditorAssetBrowserKind::Scene
                                    && node.relativePath == ui.pendingScenePath);

        if (isRenaming) {
            if (renameState.focusRequested) {
                ImGui::SetKeyboardFocusHere();
                renameState.focusRequested = false;
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##rename", renameState.nameBuffer, sizeof(renameState.nameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                const std::string newName(renameState.nameBuffer);
                if (!newName.empty() && newName != std::filesystem::path(node.name).stem().string()) {
                    namespace fs = std::filesystem;
                    const fs::path oldPath(node.absolutePath);
                    const std::string newFilename = newName.ends_with(".scene") ? newName : newName + ".scene";
                    const fs::path newPath = oldPath.parent_path() / newFilename;
                    std::error_code ec;
                    fs::rename(oldPath, newPath, ec);
                    if (!ec) {
                        const std::string cfgPath = resolveProjectPath("assets/project.cfg");
                        const std::string lastScene = readProjectCfgLastScene(cfgPath);
                        if (lastScene == oldPath.filename().string()) {
                            writeProjectCfgLastScene(cfgPath, newFilename);
                        }
                        if (ui.pendingScenePath == node.relativePath) {
                            const std::string newRelative = fs::relative(newPath, fs::current_path()).generic_string();
                            ui.pendingScenePath = newRelative;
                            document.setScenePath(newRelative);
                        }
                        refreshAssetTree = true;
                    }
                }
                renameState.renamingPath.clear();
            }
            if (!ImGui::IsItemActive() && !renameState.focusRequested
                && renameState.renamingPath == node.relativePath
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                renameState.renamingPath.clear();
            }
        } else {
            if (isActiveScene) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
            }
            if (ImGui::Selectable(node.name.c_str(), selected)) {
                setSelectedAsset(ui, node);
            }
            if (isActiveScene) {
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::TextDisabled("[active]");
            }
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
                if (ImGui::MenuItem("Rename")) {
                    SceneRenameState& rs = sceneRenameState();
                    rs.renamingPath = node.relativePath;
                    const std::string stem = std::filesystem::path(node.name).stem().string();
                    std::strncpy(rs.nameBuffer, stem.c_str(), sizeof(rs.nameBuffer) - 1);
                    rs.nameBuffer[sizeof(rs.nameBuffer) - 1] = '\0';
                    rs.focusRequested = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete...")) {
                    result.deleteScenePath = node.relativePath;
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
                ImGui::Separator();
                if (ImGui::MenuItem("New Material...")) {
                    MaterialCrudState& crud = materialCrudState();
                    crud.newPopupOpen = true;
                    crud.newIdBuffer[0] = '\0';
                    // Pre-fill parent with current material
                    if (!declaredId.empty()) {
                        std::strncpy(crud.newParentBuffer, declaredId.c_str(), sizeof(crud.newParentBuffer) - 1);
                        crud.newParentBuffer[sizeof(crud.newParentBuffer) - 1] = '\0';
                    } else {
                        crud.newParentBuffer[0] = '\0';
                    }
                    ImGui::OpenPopup("NewMaterialPopup");
                }
                if (!declaredId.empty() && ImGui::MenuItem("Rename...")) {
                    MaterialCrudState& crud = materialCrudState();
                    crud.renamePopupOpen = true;
                    crud.renamingId = declaredId;
                    crud.renamingAbsolutePath = node.absolutePath;
                    std::strncpy(crud.renameBuffer, declaredId.c_str(), sizeof(crud.renameBuffer) - 1);
                    crud.renameBuffer[sizeof(crud.renameBuffer) - 1] = '\0';
                    ImGui::OpenPopup("RenameMaterialPopup");
                }
                ImGui::Separator();
                if (!declaredId.empty() && ImGui::MenuItem("Delete...")) {
                    MaterialCrudState& crud = materialCrudState();
                    crud.deletePopupOpen = true;
                    crud.deletingId = declaredId;
                    crud.deletingAbsolutePath = node.absolutePath;
                    ImGui::OpenPopup("DeleteMaterialPopup");
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
            ImGui::Separator();
            if (ImGui::MenuItem("Reveal in Finder")) {
                revealInFinder(node.absolutePath);
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

    // ---------- Material CRUD popups ----------

    // New Material popup
    if (ImGui::BeginPopupModal("NewMaterialPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("New Material");
        ImGui::Separator();
        MaterialCrudState& crud = materialCrudState();
        ImGui::InputText("Id##NewMatId", crud.newIdBuffer, sizeof(crud.newIdBuffer));
        ImGui::InputText("Parent (optional)##NewMatParent", crud.newParentBuffer, sizeof(crud.newParentBuffer));
        ImGui::Separator();
        if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            const std::string newId(crud.newIdBuffer);
            if (!newId.empty()) {
                namespace fs = std::filesystem;
                const std::string matDir = resolveProjectPath("assets/materials");
                const std::string matPath = matDir + "/" + newId + ".material";
                MaterialDefinition def;
                def.id = newId;
                const std::string parentStr(crud.newParentBuffer);
                if (!parentStr.empty()) {
                    def.parent = parentStr;
                }
                def.roughnessBias = 0.82f;
                def.specularLevel = 0.20f;
                try {
                    fs::create_directories(matDir);
                    saveMaterialDefinitionAsset(matPath, def);
                    result.newMaterialId = newId;
                    result.assetCatalogChanged = true;
                    refreshAssetTree = true;
                    spdlog::info("Created material '{}' at {}", newId, matPath);
                } catch (const std::exception& ex) {
                    spdlog::error("Failed to create material '{}': {}", newId, ex.what());
                }
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Rename Material popup
    if (ImGui::BeginPopupModal("RenameMaterialPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        MaterialCrudState& crud = materialCrudState();
        ImGui::Text("Rename material '%s'", crud.renamingId.c_str());
        ImGui::Separator();
        ImGui::InputText("New Id##RenameMatId", crud.renameBuffer, sizeof(crud.renameBuffer));
        ImGui::Separator();
        if (ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            const std::string newId(crud.renameBuffer);
            if (!newId.empty() && newId != crud.renamingId) {
                namespace fs = std::filesystem;
                const fs::path oldPath(crud.renamingAbsolutePath);
                const fs::path newPath = oldPath.parent_path() / (newId + ".material");
                try {
                    // Load, update id, save under new name, remove old file
                    MaterialDefinition def = loadMaterialDefinitionAsset(crud.renamingAbsolutePath);
                    def.id = newId;
                    saveMaterialDefinitionAsset(newPath.string(), def);
                    std::error_code ec;
                    fs::remove(oldPath, ec);
                    result.renamedMaterialOldId = crud.renamingId;
                    result.renamedMaterialNewId = newId;
                    result.assetCatalogChanged = true;
                    refreshAssetTree = true;
                    spdlog::info("Renamed material '{}' to '{}'", crud.renamingId, newId);
                } catch (const std::exception& ex) {
                    spdlog::error("Failed to rename material '{}': {}", crud.renamingId, ex.what());
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Delete Material popup
    if (ImGui::BeginPopupModal("DeleteMaterialPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        MaterialCrudState& crud = materialCrudState();
        ImGui::Text("Delete material '%s'? This cannot be undone.", crud.deletingId.c_str());
        ImGui::Separator();
        if (ImGui::Button("Delete")) {
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::remove(fs::path(crud.deletingAbsolutePath), ec);
            if (!ec) {
                result.deletedMaterialId = crud.deletingId;
                result.assetCatalogChanged = true;
                refreshAssetTree = true;
                spdlog::info("Deleted material '{}'", crud.deletingId);
            } else {
                spdlog::error("Failed to delete material '{}': {}", crud.deletingId, ec.message());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    result.assetCatalogChanged = result.assetCatalogChanged || refreshAssetTree;
    ImGui::End();
    return result;
}
