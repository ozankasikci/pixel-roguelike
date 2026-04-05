#include "editor/debug/DebugHarness.h"
#include "editor/core/EditorRuntimePreviewSession.h"

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <glad/gl.h>
#include <spdlog/spdlog.h>

DebugHarness::DebugHarness(EditorSceneDocument& doc,
                           std::vector<std::uint64_t>& selectedIds,
                           EditorUiState& ui,
                           EditorCommandStack& cmdStack,
                           GLFWwindow* window,
                           EditorCamera& camera,
                           EditorCameraAnimation& cameraAnim,
                           const EditorViewportState& viewport,
                           const EditorPreviewWorld& previewWorld,
                           EditorRuntimePreviewSession* runtimePreviewSession)
    : doc_(doc)
    , selectedIds_(selectedIds)
    , ui_(ui)
    , cmdStack_(cmdStack)
    , window_(window)
    , camera_(camera)
    , cameraAnim_(cameraAnim)
    , viewport_(viewport)
    , previewWorld_(previewWorld)
    , runtimePreviewSession_(runtimePreviewSession) {
}

void DebugHarness::init() {
    inspector_ = std::make_unique<EditorInspector>(doc_, selectedIds_, ui_, cmdStack_, camera_, viewport_, &previewWorld_, runtimePreviewSession_);
    commander_ = std::make_unique<EditorCommander>(doc_, selectedIds_, ui_, cmdStack_, window_, camera_, cameraAnim_, viewport_, &previewWorld_, runtimePreviewSession_, &pendingScreenshotPath_);

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
    registry_.registerCommand("inspect.play_preview", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->playPreviewState();
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
    registry_.registerCommand("inspect.runtime_camera", [this](const nlohmann::json& args) {
        (void)args;
        return inspector_->runtimeCamera();
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

    // Pixel sampling — read RGB values from the rendered framebuffer at screen coordinates.
    // Useful for automated visual regression tests (e.g., detecting bright-line shadow artifacts).
    registry_.registerCommand("inspect.pixel_sample", [this](const nlohmann::json& args) {
        int ww = 0, wh = 0;
        if (window_) {
            glfwGetFramebufferSize(window_, &ww, &wh);
        }
        if (ww == 0 || wh == 0) {
            return nlohmann::json{{"ok", false}, {"error", "Cannot determine framebuffer size"}};
        }

        // Accept {x, y}, {points: [{x, y}, ...]}, or {line: {x0, y0, x1, y1, samples}}
        nlohmann::json points = nlohmann::json::array();
        if (args.contains("line")) {
            auto& line = args["line"];
            float x0 = line.value("x0", 0.0f), y0 = line.value("y0", 0.0f);
            float x1 = line.value("x1", 0.0f), y1 = line.value("y1", 0.0f);
            int samples = line.value("samples", 20);
            if (samples < 2) samples = 2;
            points.clear();
            for (int i = 0; i < samples; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(samples - 1);
                points.push_back({{"x", x0 + (x1 - x0) * t}, {"y", y0 + (y1 - y0) * t}});
            }
        } else if (args.contains("points") && args["points"].is_array()) {
            points = args["points"];
        } else if (args.contains("x") && args.contains("y")) {
            points.push_back({{"x", args["x"]}, {"y", args["y"]}});
        } else {
            return nlohmann::json{{"ok", false}, {"error", "Provide {x, y}, {points: [...]}, or {line: {x0,y0,x1,y1,samples}}"}};
        }

        // GLFW logical coords → framebuffer coords (handles Retina/HiDPI)
        int logW = 0, logH = 0;
        if (window_) {
            glfwGetWindowSize(window_, &logW, &logH);
        }
        float scaleX = (logW > 0) ? static_cast<float>(ww) / logW : 1.0f;
        float scaleY = (logH > 0) ? static_cast<float>(wh) / logH : 1.0f;

        nlohmann::json results = nlohmann::json::array();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0); // default framebuffer
        for (const auto& pt : points) {
            float sx = pt.value("x", 0.0f);
            float sy = pt.value("y", 0.0f);
            int px = static_cast<int>(sx * scaleX);
            int py = wh - 1 - static_cast<int>(sy * scaleY); // flip Y for OpenGL

            if (px < 0 || px >= ww || py < 0 || py >= wh) {
                results.push_back({{"x", sx}, {"y", sy}, {"error", "out of bounds"}});
                continue;
            }

            unsigned char rgba[4] = {0, 0, 0, 255};
            glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

            results.push_back({
                {"x", sx}, {"y", sy},
                {"r", rgba[0]}, {"g", rgba[1]}, {"b", rgba[2]}, {"a", rgba[3]},
                {"brightness", (0.299f * rgba[0] + 0.587f * rgba[1] + 0.114f * rgba[2]) / 255.0f}
            });
        }

        return nlohmann::json{
            {"ok", true},
            {"framebuffer_size", {{"w", ww}, {"h", wh}}},
            {"data", results}
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
    registry_.registerCommand("command.toggle_play_preview", [this](const nlohmann::json& args) {
        return commander_->togglePlayPreview(args);
    });
    registry_.registerCommand("command.set_preview_mode", [this](const nlohmann::json& args) {
        return commander_->setPreviewMode(args);
    });
    registry_.registerCommand("command.set_runtime_camera", [this](const nlohmann::json& args) {
        return commander_->setRuntimeCamera(args);
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
    registry_.registerCommand("command.capture_screenshot", [this](const nlohmann::json& args) {
        return commander_->captureScreenshot(args);
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

std::string DebugHarness::consumePendingScreenshotPath() {
    std::string path = std::move(pendingScreenshotPath_);
    pendingScreenshotPath_.clear();
    return path;
}
