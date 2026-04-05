#pragma once

#include "editor/ui/LevelEditorUi.h"

struct LevelColliderPlacement;

void drawColliderInspector(LevelColliderPlacement& collider,
                           EditorSceneDocument& document,
                           EditorCommandStack& commandStack,
                           EditorPendingCommand& pendingCommand,
                           const EditorSceneDocumentState& beforeState);
