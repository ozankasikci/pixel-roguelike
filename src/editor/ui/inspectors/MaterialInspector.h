#pragma once

#include "editor/ui/LevelEditorUi.h"
#include "editor/ui/inspectors/AssetInspectorSession.h"

class ContentRegistry;

void drawMaterialAssetInspector(EditorUiState& ui,
                                const EditorInspectedAsset& asset,
                                AssetInspectorSession& session,
                                ContentRegistry& content,
                                InspectorActionResult& result);
