#include "editor/debug/DebugHarness.h"

#include <spdlog/spdlog.h>

DebugHarness::DebugHarness(EditorSceneDocument& doc,
                           std::vector<std::uint64_t>& selectedIds,
                           EditorUiState& ui,
                           EditorCommandStack& cmdStack)
    : doc_(doc), selectedIds_(selectedIds), ui_(ui), cmdStack_(cmdStack) {
}

void DebugHarness::init() {
    inspector_ = std::make_unique<EditorInspector>(doc_, selectedIds_, ui_, cmdStack_);
    commander_ = std::make_unique<EditorCommander>(doc_, selectedIds_, ui_, cmdStack_);

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

    // --- command.* commands (mutating) ---
    // These are wired through a helper so we can record them in the session recorder
    auto wrapCommand = [this](auto fn) {
        return [this, fn](const nlohmann::json& args) -> nlohmann::json {
            nlohmann::json result = fn(args);
            // Record commands that succeed
            if (recorder_.isRecording()) {
                if (result.value("ok", false)) {
                    // Caller sets cmd name via outer lambda — we can't easily get it here
                    // so just record the raw result; recordCommand() is called from DebugHarness::poll()
                }
            }
            return result;
        };
    };

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
    // Deferred: these require GLFW event injection
    registry_.registerCommand("command.key_press", [this](const nlohmann::json& args) {
        return commander_->keyPress(args);
    });
    registry_.registerCommand("command.mouse_click", [this](const nlohmann::json& args) {
        return commander_->mouseClick(args);
    });
    registry_.registerCommand("command.drag", [this](const nlohmann::json& args) {
        return commander_->drag(args);
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

    (void)wrapCommand;  // suppress unused-variable warning
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
