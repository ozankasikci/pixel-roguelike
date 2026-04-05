#include "editor/ui/inspectors/AssetInspectorSession.h"

#include "editor/ui/LevelEditorUi.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/EnvironmentDefinition.h"
#include "game/content/ContentRegistry.h"

#include <spdlog/spdlog.h>

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
