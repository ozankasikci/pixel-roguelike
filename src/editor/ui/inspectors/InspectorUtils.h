#pragma once

#include "editor/ui/LevelEditorUi.h"

#include <string>
#include <vector>

struct BehaviorDeclaration;

// Behavior authoring sections — render on_activate/on_enter/on_exit/on_timer behavior lists.
// Modifies behaviors in-place; pushes undo commands via commandStack.
void drawBehaviorSections(std::vector<BehaviorDeclaration>& behaviors,
                          EditorSceneDocument& document,
                          EditorCommandStack& commandStack);
