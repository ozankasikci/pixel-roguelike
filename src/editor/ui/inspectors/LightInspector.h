#pragma once

#include "editor/ui/LevelEditorUi.h"

struct LevelLightPlacement;

void drawLightInspector(LevelLightPlacement& light,
                        EditorSceneDocument& document,
                        EditorCommandStack& commandStack,
                        EditorPendingCommand& pendingCommand,
                        const EditorSceneDocumentState& beforeState);
