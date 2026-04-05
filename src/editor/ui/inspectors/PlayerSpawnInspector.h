#pragma once

#include "editor/ui/LevelEditorUi.h"

struct LevelPlayerSpawn;

void drawPlayerSpawnInspector(LevelPlayerSpawn& spawn,
                               EditorSceneDocument& document,
                               EditorCommandStack& commandStack,
                               EditorPendingCommand& pendingCommand,
                               const EditorSceneDocumentState& beforeState);
