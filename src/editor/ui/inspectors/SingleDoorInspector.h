#pragma once

#include "editor/ui/LevelEditorUi.h"

#include <string>
#include <vector>

struct LevelSingleDoorPlacement;

void drawSingleDoorInspector(LevelSingleDoorPlacement& door,
                              EditorSceneDocument& document,
                              const std::vector<std::string>& meshIds,
                              const std::vector<std::string>& materialIds,
                              EditorCommandStack& commandStack,
                              EditorPendingCommand& pendingCommand,
                              const EditorSceneDocumentState& beforeState);
