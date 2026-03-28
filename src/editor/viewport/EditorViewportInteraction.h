#pragma once

#include "editor/scene/EditorSelectionSystem.h"
#include "editor/ui/LevelEditorUi.h"

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <vector>

class ContentRegistry;
class EditorPreviewWorld;
class MaterialTextureLibrary;
struct RenderObject;

struct EditorSelectionPickerState {
    std::vector<EditorHitResult> hits;
    ImVec2 anchorScreen{0.0f, 0.0f};
    std::size_t currentIndex = 0;
    double visibleUntilSeconds = 0.0;

    bool active(double nowSeconds) const {
        return !hits.empty() && nowSeconds <= visibleUntilSeconds;
    }

    void clear() {
        hits.clear();
        anchorScreen = ImVec2(0.0f, 0.0f);
        currentIndex = 0;
        visibleUntilSeconds = 0.0;
    }
};

bool pointInViewport(const EditorViewportState& viewport, const ImVec2& mousePos);
bool isSelected(const std::vector<std::uint64_t>& selectedIds, std::uint64_t id);
EditorPlacementState makePlacementState(const EditorDragPayload& payload);
std::vector<EditorSelectionHandle> buildViewportSelectionHandles(const std::vector<EditorSelectionHandle>& handles,
                                                                 const EditorUiState& ui);
void toggleSelection(std::vector<std::uint64_t>& selectedIds, std::uint64_t id, bool additive);
void pruneSelection(const EditorSceneDocument& document, std::vector<std::uint64_t>& selectedIds);
void refreshSelectionPicker(EditorSelectionPickerState& picker,
                            const std::vector<EditorHitResult>& hits,
                            const ImVec2& mousePos,
                            double nowSeconds,
                            bool advanceCycle);
void applySelectionHit(std::vector<std::uint64_t>& selectedIds,
                       const EditorSelectionPickerState& picker,
                       bool additive);
void renderSelectionPicker(EditorSelectionPickerState& picker,
                           EditorSceneDocument& document,
                           std::vector<std::uint64_t>& selectedIds,
                           double nowSeconds);
bool applyGizmoToSelectedObject(EditorSceneDocument& document,
                                const std::vector<std::uint64_t>& selectedIds,
                                const EditorViewportState& viewport,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                const EditorUiState& ui,
                                const EditorPreviewWorld& previewWorld);
std::optional<glm::vec3> computePlacementPoint(const std::vector<EditorSelectionHandle>& handles,
                                               const EditorRay& ray,
                                               const EditorCamera& camera,
                                               bool snappingEnabled,
                                               float moveSnap);
void commitPlacement(EditorSceneDocument& document,
                     const EditorPlacementState& state,
                     const glm::vec3& position,
                     const ContentRegistry& content,
                     const EditorCamera& camera);
void appendPlacementGhost(std::vector<RenderObject>& objects,
                          const EditorPlacementState& state,
                          const glm::vec3& position,
                          const EditorPreviewWorld& previewWorld,
                          const MaterialTextureLibrary& materials,
                          const ContentRegistry& content);
