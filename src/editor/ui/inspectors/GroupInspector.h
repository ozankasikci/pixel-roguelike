#pragma once

#include "editor/ui/LevelEditorUi.h"

struct LevelGroupNode;

void drawGroupInspector(LevelGroupNode& group,
                         EditorSceneDocument& document,
                         EditorCommandStack& commandStack,
                         EditorPendingCommand& pendingCommand,
                         const EditorSceneDocumentState& beforeState);
