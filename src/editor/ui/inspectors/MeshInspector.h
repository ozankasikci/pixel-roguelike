#pragma once

#include "editor/ui/LevelEditorUi.h"

#include <string>
#include <vector>

class ContentRegistry;
struct LevelMeshPlacement;

void drawMeshInspector(LevelMeshPlacement& mesh,
                       EditorSceneDocument& document,
                       const std::vector<std::string>& meshIds,
                       const std::vector<std::string>& materialIds,
                       const ContentRegistry& content,
                       EditorCommandStack& commandStack,
                       EditorPendingCommand& pendingCommand,
                       const EditorSceneDocumentState& beforeState);
