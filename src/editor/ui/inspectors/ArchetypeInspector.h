#pragma once

#include "editor/ui/LevelEditorUi.h"

#include <string>
#include <vector>

struct LevelArchetypePlacement;

void drawArchetypeInspector(LevelArchetypePlacement& archetype,
                             EditorSceneDocument& document,
                             const std::vector<std::string>& archetypeIds,
                             EditorCommandStack& commandStack,
                             EditorPendingCommand& pendingCommand,
                             const EditorSceneDocumentState& beforeState);
