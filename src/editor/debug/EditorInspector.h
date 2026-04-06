#pragma once

#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorPreviewWorld.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/viewport/EditorViewportController.h"

#include <cstdint>
#include <vector>

#include <nlohmann/json.hpp>

class EditorRuntimePreviewSession;

class EditorInspector {
public:
    EditorInspector(const EditorSceneDocument& doc,
                    const std::vector<std::uint64_t>& selectedIds,
                    const EditorUiState& ui,
                    const EditorCommandStack& cmdStack,
                    const EditorCamera& camera,
                    const EditorViewportState& viewport,
                    const EditorPreviewWorld* previewWorld = nullptr,
                    const EditorRuntimePreviewSession* runtimePreviewSession = nullptr);

    EditorInspector(const EditorInspector&) = delete;
    EditorInspector& operator=(const EditorInspector&) = delete;

    nlohmann::json selection() const;
    nlohmann::json gizmoState() const;
    nlohmann::json inputState() const;
    nlohmann::json frameStats() const;
    nlohmann::json undoStack() const;
    nlohmann::json panels() const;
    nlohmann::json playPreviewState() const;

    // Diagnostic inspect commands
    nlohmann::json imguiCapture() const;
    nlohmann::json imguizmoState() const;
    nlohmann::json gizmoDetailed() const;

    // Entity and camera listing
    nlohmann::json entities() const;
    nlohmann::json worldToScreen(const nlohmann::json& args) const;
    nlohmann::json camera() const;
    nlohmann::json runtimeCamera() const;
    nlohmann::json runtimeInteraction() const;

private:
    const EditorSceneDocument& doc_;
    const std::vector<std::uint64_t>& selectedIds_;
    const EditorUiState& ui_;
    const EditorCommandStack& cmdStack_;
    const EditorCamera& camera_;
    const EditorViewportState& viewport_;
    const EditorPreviewWorld* previewWorld_ = nullptr;
    const EditorRuntimePreviewSession* runtimePreviewSession_ = nullptr;
};
