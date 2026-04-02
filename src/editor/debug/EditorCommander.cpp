#include "editor/debug/EditorCommander.h"

#include "editor/viewport/EditorViewportController.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

EditorCommander::EditorCommander(EditorSceneDocument& doc,
                                 std::vector<std::uint64_t>& selectedIds,
                                 EditorUiState& ui,
                                 EditorCommandStack& cmdStack,
                                 GLFWwindow* window)
    : doc_(doc), selectedIds_(selectedIds), ui_(ui), cmdStack_(cmdStack), window_(window) {
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

// Map a key name string to the corresponding ImGuiKey value.
// Returns ImGuiKey_None if the key name is not recognized.
static ImGuiKey mapKeyNameToImGuiKey(const std::string& keyName) {
    if (keyName == "A") return ImGuiKey_A;
    if (keyName == "B") return ImGuiKey_B;
    if (keyName == "C") return ImGuiKey_C;
    if (keyName == "D") return ImGuiKey_D;
    if (keyName == "E") return ImGuiKey_E;
    if (keyName == "F") return ImGuiKey_F;
    if (keyName == "G") return ImGuiKey_G;
    if (keyName == "H") return ImGuiKey_H;
    if (keyName == "I") return ImGuiKey_I;
    if (keyName == "J") return ImGuiKey_J;
    if (keyName == "K") return ImGuiKey_K;
    if (keyName == "L") return ImGuiKey_L;
    if (keyName == "M") return ImGuiKey_M;
    if (keyName == "N") return ImGuiKey_N;
    if (keyName == "O") return ImGuiKey_O;
    if (keyName == "P") return ImGuiKey_P;
    if (keyName == "Q") return ImGuiKey_Q;
    if (keyName == "R") return ImGuiKey_R;
    if (keyName == "S") return ImGuiKey_S;
    if (keyName == "T") return ImGuiKey_T;
    if (keyName == "U") return ImGuiKey_U;
    if (keyName == "V") return ImGuiKey_V;
    if (keyName == "W") return ImGuiKey_W;
    if (keyName == "X") return ImGuiKey_X;
    if (keyName == "Y") return ImGuiKey_Y;
    if (keyName == "Z") return ImGuiKey_Z;
    if (keyName == "0") return ImGuiKey_0;
    if (keyName == "1") return ImGuiKey_1;
    if (keyName == "2") return ImGuiKey_2;
    if (keyName == "3") return ImGuiKey_3;
    if (keyName == "4") return ImGuiKey_4;
    if (keyName == "5") return ImGuiKey_5;
    if (keyName == "6") return ImGuiKey_6;
    if (keyName == "7") return ImGuiKey_7;
    if (keyName == "8") return ImGuiKey_8;
    if (keyName == "9") return ImGuiKey_9;
    if (keyName == "Delete")    return ImGuiKey_Delete;
    if (keyName == "Backspace") return ImGuiKey_Backspace;
    if (keyName == "Escape")    return ImGuiKey_Escape;
    if (keyName == "Enter")     return ImGuiKey_Enter;
    if (keyName == "Space")     return ImGuiKey_Space;
    if (keyName == "Tab")       return ImGuiKey_Tab;
    if (keyName == "Left")      return ImGuiKey_LeftArrow;
    if (keyName == "Right")     return ImGuiKey_RightArrow;
    if (keyName == "Up")        return ImGuiKey_UpArrow;
    if (keyName == "Down")      return ImGuiKey_DownArrow;
    if (keyName == "F1")  return ImGuiKey_F1;
    if (keyName == "F2")  return ImGuiKey_F2;
    if (keyName == "F3")  return ImGuiKey_F3;
    if (keyName == "F4")  return ImGuiKey_F4;
    if (keyName == "F5")  return ImGuiKey_F5;
    if (keyName == "F6")  return ImGuiKey_F6;
    if (keyName == "F7")  return ImGuiKey_F7;
    if (keyName == "F8")  return ImGuiKey_F8;
    if (keyName == "F9")  return ImGuiKey_F9;
    if (keyName == "F10") return ImGuiKey_F10;
    if (keyName == "F11") return ImGuiKey_F11;
    if (keyName == "F12") return ImGuiKey_F12;
    return ImGuiKey_None;
}

nlohmann::json EditorCommander::keyPress(const nlohmann::json& args) {
    std::string keyName = args.value("key", "");
    if (keyName.empty()) {
        return {{"ok", false}, {"error", "Missing required arg: key"}};
    }

    ImGuiKey key = mapKeyNameToImGuiKey(keyName);
    if (key == ImGuiKey_None) {
        return {{"ok", false}, {"error", "Unknown key name: " + keyName}};
    }

    bool ctrlMod  = args.value("ctrl", false);
    bool superMod = args.value("super", false);
    bool shiftMod = args.value("shift", false);
    bool altMod   = args.value("alt", false);

    ImGuiIO& io = ImGui::GetIO();

    // Set modifier state before the key event
    if (ctrlMod)  io.AddKeyEvent(ImGuiMod_Ctrl,  true);
    if (superMod) io.AddKeyEvent(ImGuiMod_Super, true);
    if (shiftMod) io.AddKeyEvent(ImGuiMod_Shift, true);
    if (altMod)   io.AddKeyEvent(ImGuiMod_Alt,   true);

    // Inject key press and release
    io.AddKeyEvent(key, true);
    io.AddKeyEvent(key, false);

    // Clear modifiers
    if (ctrlMod)  io.AddKeyEvent(ImGuiMod_Ctrl,  false);
    if (superMod) io.AddKeyEvent(ImGuiMod_Super, false);
    if (shiftMod) io.AddKeyEvent(ImGuiMod_Shift, false);
    if (altMod)   io.AddKeyEvent(ImGuiMod_Alt,   false);

    if (window_) {
        glfwPostEmptyEvent();
    }

    nlohmann::json modifiers = nlohmann::json::array();
    if (ctrlMod)  modifiers.push_back("ctrl");
    if (superMod) modifiers.push_back("super");
    if (shiftMod) modifiers.push_back("shift");
    if (altMod)   modifiers.push_back("alt");

    return {{"ok", true}, {"key", keyName}, {"modifiers", modifiers}};
}

nlohmann::json EditorCommander::mouseClick(const nlohmann::json& args) {
    if (!args.contains("x") || !args.contains("y")) {
        return {{"ok", false}, {"error", "Missing required args: x, y"}};
    }

    float x = args["x"].get<float>();
    float y = args["y"].get<float>();
    int button = args.value("button", 0);

    if (button < 0 || button > 4) {
        return {{"ok", false}, {"error", "Button must be 0-4"}};
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(button, true);
    io.AddMouseButtonEvent(button, false);

    if (window_) {
        glfwPostEmptyEvent();
    }

    return {{"ok", true}, {"x", x}, {"y", y}, {"button", button}};
}

nlohmann::json EditorCommander::drag(const nlohmann::json& args) {
    if (!args.contains("start_x") || !args.contains("start_y") ||
        !args.contains("end_x")   || !args.contains("end_y")) {
        return {{"ok", false}, {"error", "Missing required args: start_x, start_y, end_x, end_y"}};
    }

    float sx = args["start_x"].get<float>();
    float sy = args["start_y"].get<float>();
    float ex = args["end_x"].get<float>();
    float ey = args["end_y"].get<float>();
    int button = args.value("button", 0);
    int steps  = args.value("steps", 10);
    bool hold  = args.value("hold", false);

    if (button < 0 || button > 4) {
        return {{"ok", false}, {"error", "Button must be 0-4"}};
    }
    if (steps < 1) steps = 1;

    ImGuiIO& io = ImGui::GetIO();

    // Move to start position then press button
    io.AddMousePosEvent(sx, sy);
    io.AddMouseButtonEvent(button, true);

    // Interpolate positions across steps
    for (int i = 1; i <= steps; ++i) {
        float t  = static_cast<float>(i) / static_cast<float>(steps);
        float cx = sx + (ex - sx) * t;
        float cy = sy + (ey - sy) * t;
        io.AddMousePosEvent(cx, cy);
    }

    // Release button unless hold mode requested
    if (!hold) {
        io.AddMouseButtonEvent(button, false);
    }

    if (window_) {
        glfwPostEmptyEvent();
    }

    // steps positions + 1 press + (hold ? 0 : 1 release)
    int queued = steps + 2 + (hold ? 0 : 1);

    return {
        {"ok", true},
        {"queued_events", queued},
        {"hold", hold},
        {"note", "Events queued for processing across subsequent frames. Poll inspect.imguizmo_state to observe state changes."}
    };
}

nlohmann::json EditorCommander::mouseRelease(const nlohmann::json& args) {
    int button = args.value("button", 0);

    if (button < 0 || button > 4) {
        return {{"ok", false}, {"error", "Button must be 0-4"}};
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, false);

    if (window_) {
        glfwPostEmptyEvent();
    }

    return {{"ok", true}, {"button", button}};
}
