#include "editor/ui/inspectors/AssetInspectorHelpers.h"

#include "game/level/LevelDef.h"

#include <glm/vec3.hpp>
#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <sstream>

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
