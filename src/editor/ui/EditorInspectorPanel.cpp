#include "editor/ui/EditorPanels.h"

#include "editor/ui/inspectors/ArchetypeInspector.h"
#include "editor/ui/inspectors/AssetInspectorSession.h"
#include "editor/ui/inspectors/ColliderInspector.h"
#include "editor/ui/inspectors/EnvironmentInspector.h"
#include "editor/ui/inspectors/GroupInspector.h"
#include "editor/ui/inspectors/LightInspector.h"
#include "editor/ui/inspectors/MaterialInspector.h"
#include "editor/ui/inspectors/MeshInspector.h"
#include "editor/ui/inspectors/PlayerSpawnInspector.h"
#include "editor/ui/inspectors/PrefabInspector.h"
#include "editor/ui/inspectors/ReflectionProbeInspector.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"

#include <glm/common.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace {

// -----------------------------------------------------------------------
// Session management
// -----------------------------------------------------------------------

AssetInspectorSession& assetInspectorSession() {
    static AssetInspectorSession session;
    return session;
}

void syncAssetInspectorSession(const EditorInspectedAsset& asset, AssetInspectorSession& session) {
    if (session.loadedPath == asset.absolutePath) {
        return;
    }

    session.loadedPath = asset.absolutePath;
    session.materialDraft.reset();
    session.environmentDraft.reset();
    session.prefabDraft.reset();
    session.materialDirty = false;
    session.environmentDirty = false;
    session.prefabDirty = false;
    session.previewRenderer.invalidate();

    try {
        switch (asset.kind) {
        case EditorAssetBrowserKind::Material:
            session.materialDraft = loadMaterialDefinitionAsset(asset.absolutePath);
            break;
        case EditorAssetBrowserKind::Environment:
            session.environmentDraft = loadEnvironmentDefinitionAsset(asset.absolutePath);
            break;
        case EditorAssetBrowserKind::Prefab:
            session.prefabDraft = loadGameplayArchetypeAsset(asset.absolutePath);
            break;
        default:
            break;
        }
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to inspect asset '{}': {}", asset.absolutePath, ex.what());
    }
}

// -----------------------------------------------------------------------
// Shared asset inspector helpers
// -----------------------------------------------------------------------

const char* assetKindLabel(EditorAssetBrowserKind kind) {
    switch (kind) {
    case EditorAssetBrowserKind::Directory:  return "Folder";
    case EditorAssetBrowserKind::Scene:      return "Scene";
    case EditorAssetBrowserKind::Mesh:       return "Model";
    case EditorAssetBrowserKind::Material:   return "Material";
    case EditorAssetBrowserKind::Environment: return "Environment";
    case EditorAssetBrowserKind::Prefab:     return "Prefab";
    case EditorAssetBrowserKind::Sky:        return "Sky";
    case EditorAssetBrowserKind::Shader:     return "Shader";
    case EditorAssetBrowserKind::Other:      return "File";
    }
    return "File";
}

void renderFileHeader(const EditorInspectedAsset& asset) {
    ImGui::TextUnformatted(assetKindLabel(asset.kind));
    ImGui::TextWrapped("%s", asset.relativePath.c_str());
    if (!asset.declaredId.empty()) {
        ImGui::TextDisabled("Id: %s", asset.declaredId.c_str());
    }
    ImGui::Separator();
}

void renderFileMetadata(const std::string& absolutePath) {
    namespace fs = std::filesystem;
    const fs::path path(absolutePath);
    ImGui::TextWrapped("Path: %s", absolutePath.c_str());
    ImGui::Text("Extension: %s", path.extension().string().c_str());
    if (fs::exists(path) && fs::is_regular_file(path)) {
        ImGui::Text("Size: %.1f KB", static_cast<double>(fs::file_size(path)) / 1024.0);
    }
}

std::string shaderSnippet(const std::string& absolutePath) {
    std::ifstream file(absolutePath);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream snippet;
    std::string line;
    int lineCount = 0;
    while (std::getline(file, line) && lineCount < 20) {
        snippet << line << '\n';
        ++lineCount;
    }
    return snippet.str();
}

// -----------------------------------------------------------------------
// Simple asset inspectors (< 30 lines each — kept inline)
// -----------------------------------------------------------------------

void renderSceneAssetInspector(const EditorInspectedAsset& asset) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    try {
        const LevelDef level = loadLevelDef(asset.absolutePath);
        ImGui::Separator();
        ImGui::Text("Environment: %s", level.environmentId.c_str());
        ImGui::Text("Meshes: %zu", level.meshes.size());
        ImGui::Text("Lights: %zu", level.lights.size());
        ImGui::Text("Colliders: %zu", level.colliders.size());
        ImGui::Text("Archetypes: %zu", level.archetypes.size());
        ImGui::Text("Player Spawn: %s", level.hasPlayerSpawn ? "Yes" : "No");
    } catch (const std::exception& ex) {
        ImGui::TextWrapped("Failed to load scene: %s", ex.what());
    }
}

void renderMeshAssetInspector(const EditorInspectedAsset& asset, AssetInspectorSession& session) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (!asset.meshId.empty()) {
        ImGui::Text("Mesh Id: %s", asset.meshId.c_str());
    }
    RenderMaterialData material;
    material.id = "mesh_preview";
    material.specularLevel = 0.20f;
    material.detailStone = true;
    material.baseColor = glm::vec3(0.92f, 0.90f, 0.86f);
    if (session.previewRenderer.drawMeshPreview(asset.absolutePath, material, glm::vec3(0.08f, 0.09f, 0.10f), "mesh_asset_preview")) {
        const EditorPreviewMeshStats stats = session.previewRenderer.currentMeshStats();
        if (stats.valid) {
            ImGui::Text("Vertices: %zu", stats.vertexCount);
            ImGui::Text("Indices: %zu", stats.indexCount);
            ImGui::Text("Bounds Min: %.2f %.2f %.2f", stats.min.x, stats.min.y, stats.min.z);
            ImGui::Text("Bounds Max: %.2f %.2f %.2f", stats.max.x, stats.max.y, stats.max.z);
        }
    } else {
        ImGui::TextUnformatted("Failed to load mesh preview.");
    }
}

void renderSkyAssetInspector(const EditorInspectedAsset& asset, AssetInspectorSession& session) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (session.previewRenderer.drawImagePreview(asset.absolutePath, "sky_asset_preview")) {
        const glm::ivec2 imageSize = session.previewRenderer.currentImageSize();
        ImGui::Text("Dimensions: %d x %d", imageSize.x, imageSize.y);
    } else {
        ImGui::TextUnformatted("No image preview available.");
    }
}

void renderShaderAssetInspector(const EditorInspectedAsset& asset) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    const std::string snippet = shaderSnippet(asset.absolutePath);
    if (!snippet.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted("Preview");
        ImGui::BeginChild("ShaderSnippet", ImVec2(0.0f, 220.0f), true);
        ImGui::TextUnformatted(snippet.c_str());
        ImGui::EndChild();
    }
}

void renderOtherAssetInspector(const EditorInspectedAsset& asset) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (asset.directory) {
        ImGui::TextUnformatted("Folder selected.");
    } else {
        ImGui::TextUnformatted("No specialized inspector yet.");
    }
}

// -----------------------------------------------------------------------
// Scene selection inspector — thin dispatcher to per-type inspector files
// -----------------------------------------------------------------------

void renderSceneSelectionInspector(EditorSceneDocument& document,
                                   const std::vector<std::uint64_t>& selectedIds,
                                   const ContentRegistry& content,
                                   const std::vector<std::string>& meshIds,
                                   const std::vector<std::string>& materialIds,
                                   const std::vector<std::string>& archetypeIds,
                                   EditorPendingCommand& pendingCommand,
                                   EditorCommandStack& commandStack) {
    if (selectedIds.empty()) {
        ImGui::TextUnformatted("No scene selection");
        return;
    }

    if (selectedIds.size() > 1) {
        ImGui::Text("%zu objects selected", selectedIds.size());
        const auto meshObjects = selectedMeshObjects(document, selectedIds);
        if (!meshObjects.empty()) {
            ImGui::Separator();
            ImGui::Text("Mesh Materials");
            const std::string currentMaterialLabel = materialSelectionLabel(meshObjects);
            if (ImGui::BeginCombo("Material Id", currentMaterialLabel.c_str())) {
                for (const auto& materialId : materialIds) {
                    const bool selected = materialId == currentMaterialLabel;
                    if (ImGui::Selectable(materialId.c_str(), selected)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (applyMaterialToMeshes(meshObjects, materialId, content, document)) {
                            commandStack.pushDocumentStateCommand(
                                meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                                beforeState,
                                document.captureState(),
                                document);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Applies to %zu selected mesh%s", meshObjects.size(), meshObjects.size() == 1 ? "" : "es");
        }
        return;
    }

    EditorSceneObject* object = document.findObject(selectedIds.front());
    if (object == nullptr) {
        ImGui::TextUnformatted("Selection missing");
        return;
    }

    ImGui::TextUnformatted(editorSceneObjectKindName(object->kind));
    ImGui::TextDisabled("Id: %llu", static_cast<unsigned long long>(object->id));
    ImGui::Separator();

    if (!beginInspectorPropertyTable("SceneSelectionProperties")) {
        return;
    }

    // Parent picker — rendered into the open property table
    const auto renderParentPicker = [&]() {
        const std::uint64_t currentParentId = document.parentObjectId(object->id);
        if (!(document.supportsParenting(object->id) || currentParentId != 0)) {
            return;
        }

        std::string parentLabel = "<None>";
        if (currentParentId != 0) {
            if (const EditorSceneObject* parentObject = document.findObject(currentParentId)) {
                parentLabel = editorSceneObjectLabel(*parentObject);
            }
        }

        renderInspectorPropertyRow("Parent", [&]() {
            if (ImGui::BeginCombo("##value", parentLabel.c_str())) {
                const bool noParentSelected = (currentParentId == 0);
                if (ImGui::Selectable("<None>", noParentSelected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (document.clearParent(object->id)) {
                        commandStack.pushDocumentStateCommand("Clear Parent", beforeState, document.captureState(), document);
                    }
                }
                if (noParentSelected) {
                    ImGui::SetItemDefaultFocus();
                }
                for (const auto& candidate : document.objects()) {
                    if (candidate.id == object->id) {
                        continue;
                    }
                    if (!document.canSetParent(object->id, candidate.id) && candidate.id != currentParentId) {
                        continue;
                    }
                    const bool selected = (candidate.id == currentParentId);
                    if (ImGui::Selectable(editorSceneObjectLabel(candidate).c_str(), selected)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (document.setParent(object->id, candidate.id)) {
                            commandStack.pushDocumentStateCommand("Set Parent", beforeState, document.captureState(), document);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            return false;
        });
    };

    renderParentPicker();

    // Each per-type inspector receives the open property table and closes it.
    const EditorSceneDocumentState beforeState = document.captureState();

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh:
        drawMeshInspector(std::get<LevelMeshPlacement>(object->payload), document,
                          meshIds, materialIds, content, commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Light:
        drawLightInspector(std::get<LevelLightPlacement>(object->payload), document,
                           commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Collider:
        drawColliderInspector(std::get<LevelColliderPlacement>(object->payload), document,
                              commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::ReflectionProbe:
        drawReflectionProbeInspector(std::get<LevelReflectionProbePlacement>(object->payload), document,
                                     commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::PlayerSpawn:
        drawPlayerSpawnInspector(std::get<LevelPlayerSpawn>(object->payload), document,
                                 commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Archetype:
        drawArchetypeInspector(std::get<LevelArchetypePlacement>(object->payload), document,
                               archetypeIds, commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Group:
        drawGroupInspector(std::get<LevelGroupNode>(object->payload), document,
                           commandStack, pendingCommand, beforeState);
        return;
    }
}

} // namespace

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
