#pragma once

#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"

#include <cstdint>
#include <vector>

#include <nlohmann/json.hpp>

struct GLFWwindow;

class EditorCommander {
public:
    EditorCommander(EditorSceneDocument& doc,
                    std::vector<std::uint64_t>& selectedIds,
                    EditorUiState& ui,
                    EditorCommandStack& cmdStack,
                    GLFWwindow* window);

    EditorCommander(const EditorCommander&) = delete;
    EditorCommander& operator=(const EditorCommander&) = delete;

    nlohmann::json selectEntity(const nlohmann::json& args);
    nlohmann::json deselectAll(const nlohmann::json& args);
    nlohmann::json setGizmo(const nlohmann::json& args);
    nlohmann::json undo(const nlohmann::json& args);
    nlohmann::json redo(const nlohmann::json& args);
    nlohmann::json togglePanel(const nlohmann::json& args);

    nlohmann::json keyPress(const nlohmann::json& args);
    nlohmann::json mouseClick(const nlohmann::json& args);
    nlohmann::json drag(const nlohmann::json& args);
    nlohmann::json mouseRelease(const nlohmann::json& args);

private:
    EditorSceneDocument& doc_;
    std::vector<std::uint64_t>& selectedIds_;
    EditorUiState& ui_;
    EditorCommandStack& cmdStack_;
    GLFWwindow* window_;
};
