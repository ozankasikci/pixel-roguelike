#include "editor/debug/EditorCommander.h"

#include "editor/core/EditorRuntimePreviewSession.h"
#include "editor/viewport/EditorViewportController.h"
#include "game/components/CameraComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <optional>

EditorCommander::EditorCommander(EditorSceneDocument& doc,
                                 std::vector<std::uint64_t>& selectedIds,
                                 EditorUiState& ui,
                                 EditorCommandStack& cmdStack,
                                 GLFWwindow* window,
                                 EditorCamera& camera,
                                 EditorCameraAnimation& cameraAnim,
                                 const EditorViewportState& viewport,
                                 const EditorPreviewWorld* previewWorld,
                                 EditorRuntimePreviewSession* runtimePreviewSession,
                                 std::string* screenshotRequestPath)
    : doc_(doc)
    , selectedIds_(selectedIds)
    , ui_(ui)
    , cmdStack_(cmdStack)
    , window_(window)
    , camera_(camera)
    , cameraAnim_(cameraAnim)
    , viewport_(viewport)
    , previewWorld_(previewWorld)
    , runtimePreviewSession_(runtimePreviewSession)
    , screenshotRequestPath_(screenshotRequestPath) {
}

static const char* previewModeName(EditorPreviewMode mode) {
    switch (mode) {
    case EditorPreviewMode::Final:
        return "final";
    case EditorPreviewMode::LightingOnly:
        return "lighting";
    case EditorPreviewMode::SkyOnly:
        return "sky";
    case EditorPreviewMode::SunDirect:
        return "sun_direct";
    case EditorPreviewMode::SunShadowVisibility:
        return "sun_shadow";
    case EditorPreviewMode::CsmUvBounds:
        return "csm_uv";
    case EditorPreviewMode::CascadeIndex:
        return "cascade";
    }
    return "final";
}

static std::optional<EditorPreviewMode> parsePreviewModeName(const std::string& value) {
    if (value == "final") return EditorPreviewMode::Final;
    if (value == "lighting" || value == "lighting_only" || value == "scene_color") return EditorPreviewMode::LightingOnly;
    if (value == "sky") return EditorPreviewMode::SkyOnly;
    if (value == "sun_direct" || value == "sun-direct") return EditorPreviewMode::SunDirect;
    if (value == "sun_shadow" || value == "sun-shadow" || value == "sun_shadow_visibility") return EditorPreviewMode::SunShadowVisibility;
    if (value == "csm_uv" || value == "csm-uv" || value == "csm_uv_bounds") return EditorPreviewMode::CsmUvBounds;
    if (value == "cascade" || value == "cascade_index" || value == "cascade-index") return EditorPreviewMode::CascadeIndex;
    return std::nullopt;
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

nlohmann::json EditorCommander::togglePlayPreview(const nlohmann::json& /*args*/) {
    ui_.playPreviewToggleRequested = true;
    if (window_) {
        glfwPostEmptyEvent();
    }
    return {
        {"ok", true},
        {"requested", true},
        {"currently_active", ui_.playPreview}
    };
}

nlohmann::json EditorCommander::setPreviewMode(const nlohmann::json& args) {
    std::string mode = args.value("mode", "");
    if (mode.empty()) {
        return {{"ok", false}, {"error", "Missing required arg: mode"}};
    }

    std::optional<EditorPreviewMode> parsedMode = parsePreviewModeName(mode);
    if (!parsedMode.has_value()) {
        return {
            {"ok", false},
            {"error", "Unknown preview mode: " + mode + " (expected final, lighting, sky, sun_direct, sun_shadow, csm_uv, or cascade)"}
        };
    }

    ui_.previewMode = *parsedMode;
    if (window_) {
        glfwPostEmptyEvent();
    }
    return {
        {"ok", true},
        {"mode", previewModeName(ui_.previewMode)}
    };
}

nlohmann::json EditorCommander::setRuntimeCamera(const nlohmann::json& args) {
    if (runtimePreviewSession_ == nullptr) {
        return {{"ok", false}, {"error", "Runtime preview session is not available"}};
    }

    auto view = runtimePreviewSession_->registry().view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : view.each()) {
        const glm::vec3 requestedPosition(
            args.value("x", transform.position.x),
            args.value("y", transform.position.y),
            args.value("z", transform.position.z));
        const float requestedYaw = args.value("yaw", camera.yaw);
        const float requestedPitch = args.value("pitch", camera.pitch);
        const bool hasFov = args.contains("fov");
        const std::optional<float> requestedFov = hasFov
            ? std::optional<float>(args["fov"].get<float>())
            : std::nullopt;

        if (!args.contains("x") && !args.contains("y") && !args.contains("z")
            && !args.contains("yaw") && !args.contains("pitch") && !args.contains("fov")) {
            return {{"ok", false}, {"error", "No runtime camera fields provided"}};
        }

        runtimePreviewSession_->setPrimaryCameraView(requestedPosition,
                                                     requestedYaw,
                                                     requestedPitch,
                                                     requestedFov);

        const auto& updatedTransform = runtimePreviewSession_->registry().get<TransformComponent>(entity);
        const auto& updatedCamera = runtimePreviewSession_->registry().get<CameraComponent>(entity);

        if (window_) {
            glfwPostEmptyEvent();
        }

        return {{"ok", true}, {"data", {
            {"entity", static_cast<std::uint32_t>(entity)},
            {"position", {{"x", updatedTransform.position.x}, {"y", updatedTransform.position.y}, {"z", updatedTransform.position.z}}},
            {"yaw", updatedCamera.yaw},
            {"pitch", updatedCamera.pitch},
            {"fov", updatedCamera.fov}
        }}};
    }

    return {{"ok", false}, {"error", "Primary runtime camera not found"}};
}

nlohmann::json EditorCommander::captureScreenshot(const nlohmann::json& args) {
    if (!screenshotRequestPath_) {
        return {{"ok", false}, {"error", "Screenshot capture is not configured for this commander"}};
    }

    std::string path = args.value("path", "");
    if (path.empty()) {
        path = "/tmp/editor_screenshot.png";
    }

    *screenshotRequestPath_ = path;
    if (window_) {
        glfwPostEmptyEvent();
    }

    return {
        {"ok", true},
        {"queued", true},
        {"path", path}
    };
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

int EditorCommander::pendingEventCount() const {
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) {
        return 0;
    }
    return ctx->InputEventsQueue.Size;
}

nlohmann::json EditorCommander::focusEntity(const nlohmann::json& args) {
    std::string name = args.value("name", "");
    if (name.empty()) {
        return {{"ok", false}, {"error", "Missing required arg: name"}};
    }

    const EditorSceneObject* targetObj = nullptr;
    for (const auto& obj : doc_.objects()) {
        if (editorSceneObjectLabel(obj) == name) {
            targetObj = &obj;
            break;
        }
    }
    if (!targetObj) {
        return {{"ok", false}, {"error", "Entity not found: " + name}};
    }

    // Compute bounds for the focus animation — same logic as main.cpp focusPressed
    const EditorObjectBounds* bounds = previewWorld_ ? previewWorld_->findObjectBounds(targetObj->id) : nullptr;
    if (bounds && bounds->valid) {
        beginFocusAnimation(camera_, cameraAnim_, bounds->min, bounds->max);
    } else {
        glm::vec3 anchor = editorSceneObjectAnchor(*targetObj);
        glm::vec3 halfUnit(0.5f);
        beginFocusAnimation(camera_, cameraAnim_, anchor - halfUnit, anchor + halfUnit);
    }

    return {{"ok", true}, {"note", "Camera focus animation started. Wait ~0.3s (18 frames at 60fps) for completion."}};
}

nlohmann::json EditorCommander::gizmoDrag(const nlohmann::json& args) {
    ImVec2 gizmoCenter = ImGuizmo::GetScreenCenter();

    // Validate the gizmo center is within reasonable window bounds
    int ww = 0, wh = 0;
    if (window_) {
        glfwGetWindowSize(window_, &ww, &wh);
    }
    if (gizmoCenter.x < 0 || gizmoCenter.y < 0 || gizmoCenter.x > ww || gizmoCenter.y > wh) {
        return {
            {"ok", false},
            {"error", "Gizmo screen center is out of window bounds"},
            {"gizmo_x", gizmoCenter.x},
            {"gizmo_y", gizmoCenter.y},
            {"window_w", ww},
            {"window_h", wh},
            {"hint", "Focus camera on entity first, then wait for gizmo render"}
        };
    }

    float dx = 0.0f, dy = 0.0f;
    if (args.contains("direction")) {
        if (args["direction"].is_string()) {
            std::string dir = args["direction"].get<std::string>();
            if (dir == "right")     { dx = 1.0f; dy = 0.0f; }
            else if (dir == "left") { dx = -1.0f; dy = 0.0f; }
            else if (dir == "up")   { dx = 0.0f; dy = -1.0f; }
            else if (dir == "down") { dx = 0.0f; dy = 1.0f; }
            else {
                return {{"ok", false}, {"error", "Unknown direction: " + dir}};
            }
        } else if (args["direction"].is_object()) {
            dx = args["direction"].value("dx", 0.0f);
            dy = args["direction"].value("dy", 0.0f);
        }
    } else {
        dx = 1.0f; dy = 0.0f; // default: drag right
    }

    float distance = args.value("distance", 50.0f);
    int steps = args.value("steps", 10);
    if (steps < 1) {
        steps = 1;
    }

    // Normalize direction
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.001f) {
        dx /= len;
        dy /= len;
    }

    float startX = gizmoCenter.x;
    float startY = gizmoCenter.y;
    float endX = startX + dx * distance;
    float endY = startY + dy * distance;

    // Build drag args and delegate to the existing drag() method
    nlohmann::json dragArgs = {
        {"start_x", startX},
        {"start_y", startY},
        {"end_x", endX},
        {"end_y", endY},
        {"button", 0},
        {"steps", steps}
    };

    nlohmann::json result = drag(dragArgs);
    result["gizmo_center"] = {{"x", startX}, {"y", startY}};
    result["drag_end"]     = {{"x", endX},   {"y", endY}};
    return result;
}

nlohmann::json EditorCommander::waitEvents(const nlohmann::json& /*args*/) {
    int pending = pendingEventCount();
    bool cameraAnimating = cameraAnim_.active;
    return {
        {"ok", true},
        {"data", {
            {"pending_events",    pending},
            {"camera_animating",  cameraAnimating},
            {"idle",              pending == 0 && !cameraAnimating}
        }}
    };
}
