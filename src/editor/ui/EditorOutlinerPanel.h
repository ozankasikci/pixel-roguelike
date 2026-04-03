#pragma once

#include <cstdint>
#include <vector>

class EditorSceneDocument;
class EditorCommandStack;
struct EditorUiState;

enum class OutlinerNavDirection {
    Up,
    Down,
    Left,
    Right,
};

struct OutlinerVisibleRow {
    std::uint64_t objectId = 0;
    std::uint64_t parentObjectId = 0;
    bool hasChildren = false;
    bool expanded = false;
};

std::vector<OutlinerVisibleRow> buildOutlinerVisibleRows(const EditorSceneDocument& document,
                                                         const EditorUiState& ui);
bool applyOutlinerKeyboardNavigation(const EditorSceneDocument& document,
                                     EditorUiState& ui,
                                     std::vector<std::uint64_t>& selectedIds,
                                     OutlinerNavDirection direction,
                                     bool extendRange = false);

std::vector<std::uint64_t> renderOutliner(EditorSceneDocument& document,
                                          EditorUiState& ui,
                                          std::vector<std::uint64_t>& selectedIds,
                                          bool* open,
                                          EditorCommandStack& commandStack);
