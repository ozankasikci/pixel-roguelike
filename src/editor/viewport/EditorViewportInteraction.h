#pragma once

#include "editor/scene/EditorSelectionSystem.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/scene/EditorSceneDocument.h"

#include <glm/glm.hpp>
#include <imgui.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

class ContentRegistry;
class EditorPreviewWorld;
class MaterialTextureLibrary;
class EditorCommandStack;
struct RenderObject;

// Axis enum for trigger resize handle drag (per D-02)
enum class TriggerHandleAxis {
    None,
    PosX, NegX,
    PosY, NegY,
    PosZ, NegZ,
};

// State for an active trigger handle drag
struct TriggerHandleDragState {
    TriggerHandleAxis activeAxis = TriggerHandleAxis::None;
    std::uint64_t objectId = 0;
    EditorSceneDocumentState beforeState;

    bool active() const { return activeAxis != TriggerHandleAxis::None; }
    void clear() {
        activeAxis = TriggerHandleAxis::None;
        objectId = 0;
        beforeState = EditorSceneDocumentState{};
    }
};

bool tryBeginTriggerHandleDrag(TriggerHandleDragState& dragState,
                               const EditorSceneDocument& document,
                               const std::vector<std::uint64_t>& selectedIds,
                               const glm::mat4& viewProj,
                               const glm::vec2& viewportSize,
                               const glm::vec2& mousePos);

void updateTriggerHandleDrag(TriggerHandleDragState& dragState,
                             EditorSceneDocument& document,
                             const glm::vec3& cameraPos,
                             const glm::vec3& rayDir);

void endTriggerHandleDrag(TriggerHandleDragState& dragState,
                          EditorSceneDocument& document,
                          EditorCommandStack& commandStack);

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
struct MultiGizmoState {
    bool active = false;
    glm::vec3 cachedCentroid{0.0f};
    std::unordered_map<std::uint64_t, glm::mat4> cachedTransforms;

    struct LightSnapshot {
        glm::vec3 position{0.0f};
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
    };
    std::unordered_map<std::uint64_t, LightSnapshot> cachedLightData;

    void clear() {
        active = false;
        cachedTransforms.clear();
        cachedLightData.clear();
    }
};

bool applyGizmoToSelectedObject(EditorSceneDocument& document,
                                const std::vector<std::uint64_t>& selectedIds,
                                const EditorViewportState& viewport,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                const EditorUiState& ui,
                                const EditorPreviewWorld& previewWorld,
                                MultiGizmoState& multiGizmoState);
std::optional<glm::vec3> computePlacementPoint(const std::vector<EditorSelectionHandle>& handles,
                                               const EditorRay& ray,
                                               const EditorCamera& camera,
                                               bool snappingEnabled,
                                               float moveSnap);
std::optional<std::uint64_t> commitPlacement(EditorSceneDocument& document,
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
