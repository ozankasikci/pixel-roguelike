#pragma once

#include <cstdint>
#include <vector>

class EditorSceneDocument;
class EditorCommandStack;
struct EditorUiState;

std::vector<std::uint64_t> renderOutliner(EditorSceneDocument& document,
                                          EditorUiState& ui,
                                          std::vector<std::uint64_t>& selectedIds,
                                          bool* open,
                                          EditorCommandStack& commandStack);
