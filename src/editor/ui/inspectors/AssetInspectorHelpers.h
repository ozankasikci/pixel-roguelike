#pragma once

#include "editor/ui/inspectors/AssetInspectorSession.h"
#include "editor/ui/LevelEditorUi.h"

#include <string>

// Shared helper: asset kind label string
const char* assetKindLabel(EditorAssetBrowserKind kind);

// Shared helper: render file header (kind label, relative path, declared ID)
void renderFileHeader(const EditorInspectedAsset& asset);

// Shared helper: render file metadata (absolute path, extension, size)
void renderFileMetadata(const std::string& absolutePath);

// Shared helper: read first 20 lines of a shader file
std::string shaderSnippet(const std::string& absolutePath);

// Simple asset inspectors (< 30 lines each)
void renderSceneAssetInspector(const EditorInspectedAsset& asset);
void renderMeshAssetInspector(const EditorInspectedAsset& asset, AssetInspectorSession& session);
void renderSkyAssetInspector(const EditorInspectedAsset& asset, AssetInspectorSession& session);
void renderShaderAssetInspector(const EditorInspectedAsset& asset);
void renderOtherAssetInspector(const EditorInspectedAsset& asset);
