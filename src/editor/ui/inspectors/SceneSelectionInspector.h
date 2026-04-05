#pragma once

#include "editor/ui/LevelEditorUi.h"

#include <cstdint>
#include <string>
#include <vector>

class ContentRegistry;

// Renders the inspector for the current scene object selection (single or multi).
// Delegates to per-type inspectors (MeshInspector, LightInspector, etc.) and
// provides the parent picker for supported object kinds.
void renderSceneSelectionInspector(EditorSceneDocument& document,
                                   const std::vector<std::uint64_t>& selectedIds,
                                   const ContentRegistry& content,
                                   const std::vector<std::string>& meshIds,
                                   const std::vector<std::string>& materialIds,
                                   const std::vector<std::string>& archetypeIds,
                                   EditorPendingCommand& pendingCommand,
                                   EditorCommandStack& commandStack);
