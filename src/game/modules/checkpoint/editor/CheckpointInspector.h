#pragma once

#include "editor/scene/EditorSceneDocument.h"
#include "editor/core/EditorCommand.h"

struct LevelCheckpointPlacement;
struct EditorPendingCommand;

void drawCheckpointInspector(LevelCheckpointPlacement& checkpoint,
                             EditorSceneDocument& document,
                             EditorCommandStack& commandStack,
                             EditorPendingCommand& pendingCommand,
                             const EditorSceneDocumentState& beforeState);
