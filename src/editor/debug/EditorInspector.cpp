#include "editor/debug/EditorInspector.h"

#include "editor/viewport/EditorViewportController.h"

#include <imgui.h>

EditorInspector::EditorInspector(const EditorSceneDocument& doc,
                                 const std::vector<std::uint64_t>& selectedIds,
                                 const EditorUiState& ui,
                                 const EditorCommandStack& cmdStack)
    : doc_(doc), selectedIds_(selectedIds), ui_(ui), cmdStack_(cmdStack) {
}

nlohmann::json EditorInspector::selection() const {
    nlohmann::json selectedNames = nlohmann::json::array();
    for (std::uint64_t id : selectedIds_) {
        const EditorSceneObject* obj = doc_.findObject(id);
        if (obj) {
            selectedNames.push_back(editorSceneObjectLabel(*obj));
        }
    }

    std::string gizmoMode;
    switch (ui_.tool) {
        case EditorTransformTool::Translate: gizmoMode = "Translate"; break;
        case EditorTransformTool::Rotate:    gizmoMode = "Rotate";    break;
        case EditorTransformTool::Scale:     gizmoMode = "Scale";     break;
    }

    return {
        {"ok", true},
        {"data", {
            {"selected", selectedNames},
            {"gizmo", gizmoMode},
            {"gizmo_active", editorGizmoIsHot()}
        }}
    };
}

nlohmann::json EditorInspector::gizmoState() const {
    std::string toolName;
    switch (ui_.tool) {
        case EditorTransformTool::Translate: toolName = "Translate"; break;
        case EditorTransformTool::Rotate:    toolName = "Rotate";    break;
        case EditorTransformTool::Scale:     toolName = "Scale";     break;
    }

    return {
        {"ok", true},
        {"data", {
            {"mode", toolName},
            {"active", editorGizmoIsHot()}
        }}
    };
}

nlohmann::json EditorInspector::inputState() const {
    ImVec2 mousePos = ImGui::GetMousePos();
    const ImGuiIO& io = ImGui::GetIO();

    return {
        {"ok", true},
        {"data", {
            {"mouse_x", mousePos.x},
            {"mouse_y", mousePos.y},
            {"mouse_button_0", ImGui::IsMouseDown(ImGuiMouseButton_Left)},
            {"mouse_button_1", ImGui::IsMouseDown(ImGuiMouseButton_Right)},
            {"mouse_button_2", ImGui::IsMouseDown(ImGuiMouseButton_Middle)},
            {"key_ctrl",  io.KeyCtrl},
            {"key_shift", io.KeyShift},
            {"key_alt",   io.KeyAlt}
        }}
    };
}

nlohmann::json EditorInspector::frameStats() const {
    const ImGuiIO& io = ImGui::GetIO();
    return {
        {"ok", true},
        {"data", {
            {"fps",        io.Framerate},
            {"delta_time", io.DeltaTime}
        }}
    };
}

nlohmann::json EditorInspector::undoStack() const {
    const char* undoLbl = cmdStack_.canUndo() ? cmdStack_.undoLabel() : nullptr;
    const char* redoLbl = cmdStack_.canRedo() ? cmdStack_.redoLabel() : nullptr;

    return {
        {"ok", true},
        {"data", {
            {"can_undo",   cmdStack_.canUndo()},
            {"can_redo",   cmdStack_.canRedo()},
            {"undo_label", undoLbl ? undoLbl : ""},
            {"redo_label", redoLbl ? redoLbl : ""}
        }}
    };
}

nlohmann::json EditorInspector::panels() const {
    return {
        {"ok", true},
        {"data", {
            {"outliner",      ui_.showOutliner},
            {"inspector",     ui_.showInspector},
            {"asset_browser", ui_.showAssetBrowser},
            {"environment",   ui_.showEnvironment},
            {"viewport",      ui_.showViewport},
            {"build_output",  ui_.showBuildOutput}
        }}
    };
}
