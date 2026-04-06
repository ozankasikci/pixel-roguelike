#include "editor/debug/EditorInspector.h"

#include "editor/core/EditorRuntimePreviewSession.h"
#include "editor/viewport/EditorViewportController.h"
#include "game/components/CameraComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/DoorComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "engine/input/InputSystem.h"
#include <GLFW/glfw3.h>

#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

EditorInspector::EditorInspector(const EditorSceneDocument& doc,
                                 const std::vector<std::uint64_t>& selectedIds,
                                 const EditorUiState& ui,
                                 const EditorCommandStack& cmdStack,
                                 const EditorCamera& camera,
                                 const EditorViewportState& viewport,
                                 const EditorPreviewWorld* previewWorld,
                                 const EditorRuntimePreviewSession* runtimePreviewSession)
    : doc_(doc)
    , selectedIds_(selectedIds)
    , ui_(ui)
    , cmdStack_(cmdStack)
    , camera_(camera)
    , viewport_(viewport)
    , previewWorld_(previewWorld)
    , runtimePreviewSession_(runtimePreviewSession) {
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

nlohmann::json EditorInspector::playPreviewState() const {
    return {
        {"ok", true},
        {"data", {
            {"active", ui_.playPreview},
            {"toggle_requested", ui_.playPreviewToggleRequested},
            {"captured", runtimePreviewSession_ != nullptr ? runtimePreviewSession_->captured() : false}
        }}
    };
}

nlohmann::json EditorInspector::imguiCapture() const {
    const ImGuiIO& io = ImGui::GetIO();
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    ImGuiWindow* hoveredWindow = ctx ? ctx->HoveredWindow : nullptr;
    ImGuiWindow* activeIdWindow = ctx ? ctx->ActiveIdWindow : nullptr;

    return {
        {"ok", true},
        {"data", {
            {"want_capture_mouse",    io.WantCaptureMouse},
            {"want_capture_keyboard", io.WantCaptureKeyboard},
            {"want_text_input",       io.WantTextInput},
            {"hovered_window",        hoveredWindow ? hoveredWindow->Name : ""},
            {"active_id",             ImGui::GetActiveID()},
            {"active_id_window",      activeIdWindow ? activeIdWindow->Name : ""}
        }}
    };
}

nlohmann::json EditorInspector::imguizmoState() const {
    return {
        {"ok", true},
        {"data", {
            {"is_over",                   ImGuizmo::IsOver()},
            {"is_using",                  ImGuizmo::IsUsing()},
            {"is_using_any",              ImGuizmo::IsUsingAny()},
            {"is_using_view_manipulate",  ImGuizmo::IsUsingViewManipulate()},
            {"is_view_manipulate_hovered", ImGuizmo::IsViewManipulateHovered()}
        }}
    };
}

nlohmann::json EditorInspector::gizmoDetailed() const {
    std::string toolName;
    switch (ui_.tool) {
        case EditorTransformTool::Translate: toolName = "Translate"; break;
        case EditorTransformTool::Rotate:    toolName = "Rotate";    break;
        case EditorTransformTool::Scale:     toolName = "Scale";     break;
    }

    const char* undoLbl = cmdStack_.canUndo() ? cmdStack_.undoLabel() : nullptr;
    const char* redoLbl = cmdStack_.canRedo() ? cmdStack_.redoLabel() : nullptr;

    return {
        {"ok", true},
        {"data", {
            {"tool",                  toolName},
            {"imguizmo_is_over",      ImGuizmo::IsOver()},
            {"imguizmo_is_using",     ImGuizmo::IsUsing()},
            {"imguizmo_is_using_any", ImGuizmo::IsUsingAny()},
            {"editor_gizmo_is_hot",   editorGizmoIsHot()},
            {"mouse_left_down",       ImGui::IsMouseDown(ImGuiMouseButton_Left)},
            {"mouse_pos_x",           ImGui::GetMousePos().x},
            {"mouse_pos_y",           ImGui::GetMousePos().y},
            {"selected_count",        static_cast<int>(selectedIds_.size())},
            {"can_undo",              cmdStack_.canUndo()},
            {"can_redo",              cmdStack_.canRedo()},
            {"undo_label",            undoLbl ? undoLbl : ""},
            {"redo_label",            redoLbl ? redoLbl : ""}
        }}
    };
}

nlohmann::json EditorInspector::entities() const {
    auto arr = nlohmann::json::array();
    for (const auto& obj : doc_.objects()) {
        glm::vec3 pos = editorSceneObjectAnchor(obj);
        arr.push_back({
            {"id", obj.id},
            {"label", editorSceneObjectLabel(obj)},
            {"kind", editorSceneObjectKindName(obj.kind)},
            {"world_position", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}}
        });
    }
    return {{"ok", true}, {"data", arr}};
}

nlohmann::json EditorInspector::worldToScreen(const nlohmann::json& args) const {
    float wx = args.value("x", 0.0f);
    float wy = args.value("y", 0.0f);
    float wz = args.value("z", 0.0f);

    float aspect = viewport_.size.x > 0 ? viewport_.size.x / viewport_.size.y : 1.0f;
    glm::mat4 view = editorCameraView(camera_);
    glm::mat4 proj = editorCameraProjection(camera_, aspect);
    glm::vec4 clip = proj * view * glm::vec4(wx, wy, wz, 1.0f);

    bool visible = clip.w > 0.001f;
    if (visible) {
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        // NDC to viewport pixel coordinates (GLFW cursor space)
        float sx = viewport_.origin.x + (ndc.x * 0.5f + 0.5f) * viewport_.size.x;
        float sy = viewport_.origin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_.size.y;
        visible = sx >= viewport_.origin.x && sx <= viewport_.origin.x + viewport_.size.x
               && sy >= viewport_.origin.y && sy <= viewport_.origin.y + viewport_.size.y;
        return {{"ok", true}, {"data", {{"x", sx}, {"y", sy}, {"visible", visible}}}};
    }
    return {{"ok", true}, {"data", {{"x", 0}, {"y", 0}, {"visible", false}}}};
}

nlohmann::json EditorInspector::camera() const {
    return {{"ok", true}, {"data", {
        {"position", {{"x", camera_.position.x}, {"y", camera_.position.y}, {"z", camera_.position.z}}},
        {"yaw",              camera_.yawDegrees},
        {"pitch",            camera_.pitchDegrees},
        {"fov",              camera_.fovDegrees},
        {"orbit_distance",   camera_.orbitDistance},
        {"orbit_pivot",      {{"x", camera_.orbitPivot.x}, {"y", camera_.orbitPivot.y}, {"z", camera_.orbitPivot.z}}},
        {"orbit_pivot_valid", camera_.orbitPivotValid}
    }}};
}

nlohmann::json EditorInspector::runtimeCamera() const {
    if (runtimePreviewSession_ == nullptr) {
        return {{"ok", false}, {"error", "Runtime preview session is not available"}};
    }

    auto view = runtimePreviewSession_->registry().view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : view.each()) {
        return {{"ok", true}, {"data", {
            {"entity", static_cast<std::uint32_t>(entity)},
            {"position", {{"x", transform.position.x}, {"y", transform.position.y}, {"z", transform.position.z}}},
            {"yaw", camera.yaw},
            {"pitch", camera.pitch},
            {"fov", camera.fov},
            {"near_plane", camera.nearPlane},
            {"far_plane", camera.farPlane},
            {"forward", {{"x", camera.forward.x}, {"y", camera.forward.y}, {"z", camera.forward.z}}}
        }}};
    }

    return {{"ok", false}, {"error", "Primary runtime camera not found"}};
}

nlohmann::json EditorInspector::runtimeInteraction() const {
    if (runtimePreviewSession_ == nullptr) {
        return {{"ok", false}, {"error", "No runtime preview session"}};
    }
    if (!ui_.playPreview) {
        return {{"ok", false}, {"error", "Play preview not active"}};
    }

    const auto& reg = runtimePreviewSession_->registry();
    const auto& input = runtimePreviewSession_->input();

    nlohmann::json result;
    result["ok"] = true;
    result["data"] = nlohmann::json::object();
    result["data"]["captured"] = runtimePreviewSession_->captured();
    result["data"]["cursor_locked"] = input.isCursorLocked();
    result["data"]["e_key_pressed"] = input.isKeyPressed(GLFW_KEY_E);

    // Find player
    auto actorView = reg.view<TransformComponent, CameraComponent, ControllableTag, PrimaryCameraTag>();
    bool foundActor = false;
    glm::vec3 actorPos{0.0f};
    for (auto [entity, transform, camera] : actorView.each()) {
        actorPos = transform.position;
        float yawRad = glm::radians(camera.yaw);
        float pitchRad = glm::radians(camera.pitch);
        glm::vec3 fwd{std::cos(pitchRad) * std::sin(yawRad), std::sin(pitchRad), std::cos(pitchRad) * std::cos(yawRad)};
        result["data"]["player"] = {
            {"position", {{"x", transform.position.x}, {"y", transform.position.y}, {"z", transform.position.z}}},
            {"forward", {{"x", fwd.x}, {"y", fwd.y}, {"z", fwd.z}}},
            {"yaw", camera.yaw}, {"pitch", camera.pitch}
        };
        foundActor = true;
        break;
    }
    result["data"]["player_found"] = foundActor;

    // List interactables with distance to player
    nlohmann::json interactables = nlohmann::json::array();
    auto interView = reg.view<TransformComponent, InteractableComponent>();
    for (auto [entity, transform, interactable] : interView.each()) {
        float dist = foundActor ? glm::length(transform.position - actorPos) : -1.0f;
        bool hasDoor = reg.any_of<DoorComponent>(entity);
        interactables.push_back({
            {"entity", static_cast<std::uint32_t>(entity)},
            {"position", {{"x", transform.position.x}, {"y", transform.position.y}, {"z", transform.position.z}}},
            {"prompt", interactable.promptText},
            {"enabled", interactable.enabled},
            {"distance", interactable.interactDistance},
            {"dot_threshold", interactable.interactDotThreshold},
            {"player_distance", dist},
            {"has_door_component", hasDoor}
        });
    }
    result["data"]["interactables"] = interactables;
    result["data"]["interactable_count"] = interactables.size();

    return result;
}
