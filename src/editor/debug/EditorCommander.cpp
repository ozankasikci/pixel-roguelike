#include "editor/debug/EditorCommander.h"

#include "editor/viewport/EditorViewportController.h"

EditorCommander::EditorCommander(EditorSceneDocument& doc,
                                 std::vector<std::uint64_t>& selectedIds,
                                 EditorUiState& ui,
                                 EditorCommandStack& cmdStack)
    : doc_(doc), selectedIds_(selectedIds), ui_(ui), cmdStack_(cmdStack) {
}

nlohmann::json EditorCommander::selectEntity(const nlohmann::json& args) {
    std::string name = args.value("name", "");
    if (name.empty()) {
        return {{"ok", false}, {"error", "Missing required arg: name"}};
    }

    for (const EditorSceneObject& obj : doc_.objects()) {
        if (editorSceneObjectLabel(obj) == name) {
            selectedIds_ = {obj.id};
            return {{"ok", true}};
        }
    }

    return {{"ok", false}, {"error", "Entity not found: " + name}};
}

nlohmann::json EditorCommander::deselectAll(const nlohmann::json& /*args*/) {
    selectedIds_.clear();
    return {{"ok", true}};
}

nlohmann::json EditorCommander::setGizmo(const nlohmann::json& args) {
    std::string mode = args.value("mode", "");
    if (mode == "Translate") {
        ui_.tool = EditorTransformTool::Translate;
    } else if (mode == "Rotate") {
        ui_.tool = EditorTransformTool::Rotate;
    } else if (mode == "Scale") {
        ui_.tool = EditorTransformTool::Scale;
    } else {
        return {{"ok", false}, {"error", "Unknown gizmo mode: " + mode + " (expected Translate, Rotate, or Scale)"}};
    }
    return {{"ok", true}};
}

nlohmann::json EditorCommander::undo(const nlohmann::json& /*args*/) {
    if (!cmdStack_.canUndo()) {
        return {{"ok", false}, {"error", "Nothing to undo"}};
    }
    cmdStack_.undo(doc_);
    return {{"ok", true}};
}

nlohmann::json EditorCommander::redo(const nlohmann::json& /*args*/) {
    if (!cmdStack_.canRedo()) {
        return {{"ok", false}, {"error", "Nothing to redo"}};
    }
    cmdStack_.redo(doc_);
    return {{"ok", true}};
}

nlohmann::json EditorCommander::togglePanel(const nlohmann::json& args) {
    std::string panel = args.value("panel", "");
    if (panel == "outliner") {
        ui_.showOutliner = !ui_.showOutliner;
    } else if (panel == "inspector") {
        ui_.showInspector = !ui_.showInspector;
    } else if (panel == "asset_browser") {
        ui_.showAssetBrowser = !ui_.showAssetBrowser;
    } else if (panel == "environment") {
        ui_.showEnvironment = !ui_.showEnvironment;
    } else if (panel == "viewport") {
        ui_.showViewport = !ui_.showViewport;
    } else if (panel == "build_output") {
        ui_.showBuildOutput = !ui_.showBuildOutput;
    } else {
        return {{"ok", false}, {"error", "Unknown panel: " + panel}};
    }
    return {{"ok", true}};
}

nlohmann::json EditorCommander::keyPress(const nlohmann::json& /*args*/) {
    return {{"ok", false}, {"error", "Not yet implemented"}};
}

nlohmann::json EditorCommander::mouseClick(const nlohmann::json& /*args*/) {
    return {{"ok", false}, {"error", "Not yet implemented"}};
}

nlohmann::json EditorCommander::drag(const nlohmann::json& /*args*/) {
    return {{"ok", false}, {"error", "Not yet implemented"}};
}
