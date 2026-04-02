#pragma once

#include "editor/debug/CommandRegistry.h"
#include "editor/debug/DebugServer.h"
#include "editor/debug/EditorCommander.h"
#include "editor/debug/EditorInspector.h"
#include "editor/debug/SessionPlayer.h"
#include "editor/debug/SessionRecorder.h"
#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"

#include <cstdint>
#include <memory>
#include <vector>

class DebugHarness {
public:
    DebugHarness(EditorSceneDocument& doc,
                 std::vector<std::uint64_t>& selectedIds,
                 EditorUiState& ui,
                 EditorCommandStack& cmdStack);
    ~DebugHarness() = default;

    DebugHarness(const DebugHarness&) = delete;
    DebugHarness& operator=(const DebugHarness&) = delete;

    void init();
    void poll();
    void shutdown();

private:
    EditorSceneDocument& doc_;
    std::vector<std::uint64_t>& selectedIds_;
    EditorUiState& ui_;
    EditorCommandStack& cmdStack_;

    CommandRegistry registry_;
    DebugServer server_;
    std::unique_ptr<EditorInspector> inspector_;
    std::unique_ptr<EditorCommander> commander_;
    SessionRecorder recorder_;
    SessionPlayer player_;
};
