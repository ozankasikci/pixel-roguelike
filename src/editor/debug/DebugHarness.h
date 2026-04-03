#pragma once

#include "editor/debug/CommandRegistry.h"
#include "editor/debug/DebugServer.h"
#include "editor/debug/EditorCommander.h"
#include "editor/debug/EditorInspector.h"
#include "editor/debug/SessionPlayer.h"
#include "editor/debug/SessionRecorder.h"
#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorPreviewWorld.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/viewport/EditorViewportController.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;
class EditorRuntimePreviewSession;

class DebugHarness {
public:
    DebugHarness(EditorSceneDocument& doc,
                 std::vector<std::uint64_t>& selectedIds,
                 EditorUiState& ui,
                 EditorCommandStack& cmdStack,
                 GLFWwindow* window,
                 EditorCamera& camera,
                 EditorCameraAnimation& cameraAnim,
                 const EditorViewportState& viewport,
                 const EditorPreviewWorld& previewWorld,
                 EditorRuntimePreviewSession* runtimePreviewSession = nullptr);
    ~DebugHarness() = default;

    DebugHarness(const DebugHarness&) = delete;
    DebugHarness& operator=(const DebugHarness&) = delete;

    void init();
    void poll();
    void shutdown();

    bool hasPendingScreenshot() const { return !pendingScreenshotPath_.empty(); }
    std::string consumePendingScreenshotPath();

private:
    EditorSceneDocument& doc_;
    std::vector<std::uint64_t>& selectedIds_;
    EditorUiState& ui_;
    EditorCommandStack& cmdStack_;
    GLFWwindow* window_;
    EditorCamera& camera_;
    EditorCameraAnimation& cameraAnim_;
    const EditorViewportState& viewport_;
    const EditorPreviewWorld& previewWorld_;
    EditorRuntimePreviewSession* runtimePreviewSession_ = nullptr;

    CommandRegistry registry_;
    DebugServer server_;
    std::unique_ptr<EditorInspector> inspector_;
    std::unique_ptr<EditorCommander> commander_;
    SessionRecorder recorder_;
    SessionPlayer player_;
    std::string pendingScreenshotPath_;
};
