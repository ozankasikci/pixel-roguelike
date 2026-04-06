#pragma once

#include <string>

struct LevelDoorGroupPlacement;
class EditorSceneDocument;
struct EditorSceneDocumentState;
class EditorCommandStack;
struct EditorPendingCommand;

void drawDoorGroupInspector(LevelDoorGroupPlacement& doorGroup,
                             EditorSceneDocument& document,
                             EditorCommandStack& commandStack,
                             EditorPendingCommand& pendingCommand,
                             const EditorSceneDocumentState& beforeState);
