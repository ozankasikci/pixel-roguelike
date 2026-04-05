#pragma once

#include "editor/ui/LevelEditorUi.h"

struct LevelReflectionProbePlacement;

void drawReflectionProbeInspector(LevelReflectionProbePlacement& probe,
                                   EditorSceneDocument& document,
                                   EditorCommandStack& commandStack,
                                   EditorPendingCommand& pendingCommand,
                                   const EditorSceneDocumentState& beforeState);
