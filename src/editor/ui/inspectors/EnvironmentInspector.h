#pragma once

#include "editor/ui/LevelEditorUi.h"
#include "editor/ui/inspectors/AssetInspectorSession.h"

class ContentRegistry;

void drawEnvironmentAssetInspector(EditorUiState& ui,
                                   const EditorInspectedAsset& asset,
                                   AssetInspectorSession& session,
                                   EditorSceneDocument& document,
                                   ContentRegistry& content,
                                   InspectorActionResult& result);
