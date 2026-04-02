#include "editor/debug/DebugHarness.h"

#include <ImGuizmo.h>
#include <spdlog/spdlog.h>

DebugHarness::DebugHarness(EditorSceneDocument& doc,
                           std::vector<std::uint64_t>& selectedIds,
                           EditorUiState& ui,
                           EditorCommandStack& cmdStack,
                           GLFWwindow* window,
                           EditorCamera& camera,
                           EditorCameraAnimation& cameraAnim,
                           const EditorViewportState& viewport,
                           const EditorPreviewWorld& previewWorld)
    : doc_(doc)
    , selectedIds_(selectedIds)
    , ui_(ui)
    , cmdStack_(cmdStack)
    , window_(window)
    , camera_(camera)
    , cameraAnim_(cameraAnim)
    , viewport_(viewport)
    , previewWorld_(previewWorld) {
}

void DebugHarness::init() {
    inspector_ = std::make_unique<EditorInspector>(doc_, selectedIds_, ui_, cmdStack_, camera_, viewport_, previewWorld_);
    commander_ = std::make_unique<EditorCommander>(doc_, selectedIds_, ui_, cmdStack_, window_, camera_, cameraAnim_, viewport_, previewWorld_);

    // --- inspect.* commands (read-only) ---
    registry_.registerCommand("inspect.selection", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->selection();
    });
    registry_.registerCommand("inspect.gizmo_state", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->gizmoState();
    });
    registry_.registerCommand("inspect.input_state", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->inputState();
    });
    registry_.registerCommand("inspect.frame_stats", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->frameStats();
    });
    registry_.registerCommand("inspect.undo_stack", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->undoStack();
    });
    registry_.registerCommand("inspect.panels", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->panels();
    });

    // Diagnostic inspect commands
    registry_.registerCommand("inspect.imgui_capture", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->imguiCapture();
    });
    registry_.registerCommand("inspect.imguizmo_state", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->imguizmoState();
    });
    registry_.registerCommand("inspect.gizmo_detailed", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->gizmoDetailed();
    });

    // Entity listing and camera / projection helpers
    registry_.registerCommand("inspect.entities", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->entities();
    });
    registry_.registerCommand("inspect.world_to_screen", [this](const nlohmann::json& args) {
        return inspector_->worldToScreen(args);
    });
    registry_.registerCommand("inspect.camera", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->camera();
    });
    registry_.registerCommand("inspect.gizmo_screen_pos", [this](const nlohmann::json& /*args*/) {
        ImVec2 center = ImGuizmo::GetScreenCenter();
        return nlohmann::json{
            {"ok", true},
            {"data", {
                {"x", center.x},
                {"y", center.y},
                {"note", "Coordinates are in GLFW cursor space (logical pixels, window-relative). "
                         "Only valid after ImGuizmo::Manipulate() has been called this frame."}
            }}
        };
    });

    // --- command.* commands (mutating) ---
    registry_.registerCommand("command.select_entity", [this](const nlohmann::json& args) {
        return commander_->selectEntity(args);
    });
    registry_.registerCommand("command.deselect_all", [this](const nlohmann::json& args) {
        return commander_->deselectAll(args);
    });
    registry_.registerCommand("command.set_gizmo", [this](const nlohmann::json& args) {
        return commander_->setGizmo(args);
    });
    registry_.registerCommand("command.undo", [this](const nlohmann::json& args) {
        return commander_->undo(args);
    });
    registry_.registerCommand("command.redo", [this](const nlohmann::json& args) {
        return commander_->redo(args);
    });
    registry_.registerCommand("command.toggle_panel", [this](const nlohmann::json& args) {
        return commander_->togglePanel(args);
    });
    registry_.registerCommand("command.key_press", [this](const nlohmann::json& args) {
        return commander_->keyPress(args);
    });
    registry_.registerCommand("command.mouse_click", [this](const nlohmann::json& args) {
        return commander_->mouseClick(args);
    });
    registry_.registerCommand("command.drag", [this](const nlohmann::json& args) {
        return commander_->drag(args);
    });
    registry_.registerCommand("command.mouse_release", [this](const nlohmann::json& args) {
        return commander_->mouseRelease(args);
    });
    registry_.registerCommand("command.focus_entity", [this](const nlohmann::json& args) {
        return commander_->focusEntity(args);
    });
    registry_.registerCommand("command.gizmo_drag", [this](const nlohmann::json& args) {
        return commander_->gizmoDrag(args);
    });
    registry_.registerCommand("command.wait_events", [this](const nlohmann::json& args) {
        return commander_->waitEvents(args);
    });

    // --- record.* commands ---
    registry_.registerCommand("record.start", [this](const nlohmann::json& args) {
        std::string file = args.value("file", "");
        return recorder_.start(file, doc_.scenePath());
    });
    registry_.registerCommand("record.stop", [this](const nlohmann::json& /*args*/) {
        return recorder_.stop();
    });
    registry_.registerCommand("record.status", [this](const nlohmann::json& /*args*/) {
        return recorder_.status();
    });

    // --- replay.* commands ---
    registry_.registerCommand("replay.load", [this](const nlohmann::json& args) {
        std::string file = args.value("file", "");
        return player_.load(file);
    });
    registry_.registerCommand("replay.play", [this](const nlohmann::json& /*args*/) {
        return player_.play();
    });
    registry_.registerCommand("replay.pause", [this](const nlohmann::json& /*args*/) {
        return player_.pause();
    });
    registry_.registerCommand("replay.step", [this](const nlohmann::json& /*args*/) {
        return player_.step(registry_);
    });

    server_.init();
    spdlog::info("DebugHarness: ready on {}", server_.socketPath());
}

void DebugHarness::poll() {
    server_.poll(registry_);
    player_.tick(registry_);
}

void DebugHarness::shutdown() {
    if (recorder_.isRecording()) {
        recorder_.stop();
    }
    server_.shutdown();
}
