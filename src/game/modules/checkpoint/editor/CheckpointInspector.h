#pragma once

class EditorSceneDocument;
class EditorCommandStack;
struct EditorPendingCommand;
struct EditorSceneDocumentState;
struct LevelCheckpointPlacement;

void drawCheckpointInspector(LevelCheckpointPlacement& checkpoint,
                              EditorSceneDocument& document,
                              EditorCommandStack& commandStack,
                              EditorPendingCommand& pendingCommand,
                              const EditorSceneDocumentState& beforeState);
