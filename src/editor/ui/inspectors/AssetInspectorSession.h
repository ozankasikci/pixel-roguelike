#pragma once

#include "editor/render/EditorAssetPreviewRenderer.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/EnvironmentDefinition.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"

#include <optional>
#include <string>

// Shared state for asset inspector sessions. Tracks which asset is loaded
// and holds draft values for editable asset types (material, environment, prefab).
struct AssetInspectorSession {
    std::string loadedPath;
    std::optional<MaterialDefinition> materialDraft;
    std::optional<EnvironmentDefinition> environmentDraft;
    std::optional<GameplayArchetypeDefinition> prefabDraft;
    MaterialTextureLibrary materialLibrary;
    bool materialLibraryReady = false;
    bool materialLibraryDirty = true;
    bool materialDirty = false;
    bool environmentDirty = false;
    bool prefabDirty = false;
    EditorAssetPreviewRenderer previewRenderer;
};

struct EditorInspectedAsset;

// Session accessors (implemented in AssetInspectorSession.cpp)
AssetInspectorSession& assetInspectorSession();
void syncAssetInspectorSession(const EditorInspectedAsset& asset, AssetInspectorSession& session);
