#pragma once

#include <string>

struct LevelDoorPlacement;
class EditorSceneDocument;
struct EditorSceneDocumentState;
class EditorCommandStack;
struct EditorPendingCommand;

void drawDoorGroupInspector(LevelDoorPlacement& doorGroup,
                             EditorSceneDocument& document,
                             EditorCommandStack& commandStack,
                             EditorPendingCommand& pendingCommand,
                             const EditorSceneDocumentState& beforeState);
