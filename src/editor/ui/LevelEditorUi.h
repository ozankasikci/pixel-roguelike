#pragma once

#include "editor/assets/EditorAssetBrowser.h"
#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/viewport/EditorViewportController.h"
#include "game/rendering/MaterialDefinition.h"

#include <cstdint>
#include <filesystem>
#include <glm/vec3.hpp>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>

class ContentRegistry;

enum class EditorPreviewMode {
    Final,
    LightingOnly,
    SkyOnly,
    SunDirect,
    SunShadowVisibility,
    CsmUvBounds,
    CascadeIndex,
};

enum class EditorPreviewQuality {
    Fast,
    Balanced,
    High,
};

struct EditorPreviewQualitySettings {
    float renderScale = 1.0f;
    int shadowResolutionIndex = 1;
};

inline const char* editorPreviewQualityLabel(EditorPreviewQuality quality) {
    switch (quality) {
    case EditorPreviewQuality::Fast:
        return "Fast";
    case EditorPreviewQuality::Balanced:
        return "Balanced";
    case EditorPreviewQuality::High:
        return "High";
    }
    return "Balanced";
}

inline EditorPreviewQualitySettings editorPreviewQualitySettings(EditorPreviewQuality quality) {
    switch (quality) {
    case EditorPreviewQuality::Fast:
        return EditorPreviewQualitySettings{0.75f, 0};
    case EditorPreviewQuality::Balanced:
        return EditorPreviewQualitySettings{0.90f, 1};
    case EditorPreviewQuality::High:
        return EditorPreviewQualitySettings{1.0f, 2};
    }
    return EditorPreviewQualitySettings{0.90f, 1};
}

enum class EditorPlacementKind {
    None,
    Mesh,
    PointLight,
    SpotLight,
    DirectionalLight,
    Collider,
    PlayerSpawn,
    Checkpoint,
    Archetype,
};

enum class EditorInspectorContext {
    None,
    SceneSelection,
    AssetSelection,
};

struct EditorInspectedAsset {
    std::string relativePath = "assets";
    std::string absolutePath;
    EditorAssetBrowserKind kind = EditorAssetBrowserKind::Directory;
    bool directory = true;
    std::string declaredId;
    std::string meshId;
};

struct EditorUiState {
    EditorTransformTool tool = EditorTransformTool::Translate;
    EditorPreviewMode previewMode = EditorPreviewMode::Final;
    EditorPreviewQuality previewQuality = EditorPreviewQuality::High;
    bool playPreview = false;
    bool playPreviewToggleRequested = false;
    bool showOutliner = true;
    bool showInspector = true;
    bool showAssetBrowser = true;
    bool showEnvironment = true;
    bool showViewport = true;
    bool showBuildOutput = false;
    bool showPerformance = false;
    bool showCameraDebug = false;
    bool viewportFullscreen = false;
    bool showColliders = true;
    bool showLightHelpers = false;
    bool showSpawnMarker = false;
    bool showTriggers = true;
#ifdef NDEBUG
    bool showViewportStats = false;
#else
    bool showViewportStats = true;
#endif
    bool snappingEnabled = false;
    float moveSnap = 0.5f;
    float rotateSnap = 15.0f;
    float scaleSnap = 0.1f;
    float previewRenderScale = 1.0f;
    int shadowResolutionIndex = 2;
    std::string selectedMeshId = "cube";
    std::string selectedMaterialId = "stone_default";
    std::string selectedArchetypeId = "checkpoint_shrine";
    std::string pendingScenePath;
    std::string activeLayoutPreset = "default";
    std::string selectedAssetPath = "assets";
    std::unordered_set<std::string> expandedAssetPaths{"assets"};
    EditorInspectorContext inspectorContext = EditorInspectorContext::None;
    EditorInspectedAsset inspectedAsset{};
    bool frameSelectionRequested = false;
    bool scrollToSelection = false;
    std::uint64_t outlinerAnchorId = 0;
    std::unordered_set<std::uint64_t> expandedOutlinerIds;
    char layoutNameBuffer[64] = "default";
    char outlinerFilter[128] = {};
};

inline void applyEditorPreviewQuality(EditorUiState& ui, EditorPreviewQuality quality) {
    const EditorPreviewQualitySettings settings = editorPreviewQualitySettings(quality);
    ui.previewQuality = quality;
    ui.previewRenderScale = settings.renderScale;
    ui.shadowResolutionIndex = settings.shadowResolutionIndex;
}

struct EditorPlacementState {
    EditorPlacementKind kind = EditorPlacementKind::None;
    std::string meshId;
    std::string materialId;
    std::string archetypeId;

    bool active() const { return kind != EditorPlacementKind::None; }
    void clear() {
        kind = EditorPlacementKind::None;
        meshId.clear();
        materialId.clear();
        archetypeId.clear();
    }
};

struct EditorDragPayload {
    EditorPlacementKind kind = EditorPlacementKind::None;
    char primaryId[64]{};
    char secondaryId[64]{};
};

struct EditorPendingCommand {
    bool active = false;
    std::string label;
    EditorSceneDocumentState beforeState;

    void clear() {
        active = false;
        label.clear();
        beforeState = EditorSceneDocumentState{};
    }
};

struct AssetBrowserActionResult {
    std::optional<std::string> openScenePath;
    bool previewDirty = false;
    bool assetCatalogChanged = false;
    bool consumedExternalDrops = false;
    bool newSceneRequested = false;
    std::optional<std::string> deleteScenePath;
    std::optional<std::string> renameScenePath;
    // Material CRUD — caller must apply to ContentRegistry
    std::optional<std::string> newMaterialId;    // material was created; call content.addMaterial()
    std::optional<std::string> deletedMaterialId; // material was deleted; call content.removeMaterial()
    // Rename: caller removes old id and adds new definition
    std::optional<std::string> renamedMaterialOldId;
    std::optional<std::string> renamedMaterialNewId;
};

struct InspectorActionResult {
    bool previewDirty = false;
    bool materialDirty = false;
    bool contentReloaded = false;
};

enum class EditorInspectorFieldKind {
    Toggle,
    Scalar,
    Enum,
    Text,
    Vector,
    Color,
};

bool beginInspectorPropertyTable(const char* id,
                                 float labelColumnFraction = 0.28f,
                                 float minLabelWidth = 88.0f,
                                 float maxLabelWidth = 180.0f);
bool beginCompactEditorPanelWindow(const char* name,
                                   bool* open = nullptr,
                                   ImGuiWindowFlags flags = 0);
void endInspectorPropertyTable();
void beginInspectorPropertyLabel(const char* label,
                                 EditorInspectorFieldKind kind = EditorInspectorFieldKind::Scalar);
void applyInspectorFieldWidth(EditorInspectorFieldKind kind);

template <typename Func>
auto renderInspectorPropertyRow(const char* label,
                                Func&& func,
                                EditorInspectorFieldKind kind = EditorInspectorFieldKind::Scalar) -> decltype(func()) {
    beginInspectorPropertyLabel(label, kind);
    auto result = func();
    ImGui::PopID();
    return result;
}

void beginPlacement(EditorPlacementState& state,
                    EditorPlacementKind kind,
                    const std::string& primaryId = {},
                    const std::string& secondaryId = {});
void emitPlacementDragSource(EditorPlacementKind kind,
                             const std::string& primaryId = {},
                             const std::string& secondaryId = {});
bool containsString(const std::vector<std::string>& values, const std::string& value);
std::vector<EditorSceneObject*> selectedMeshObjects(EditorSceneDocument& document,
                                                    const std::vector<std::uint64_t>& selectedIds);
bool applyMaterialToMeshes(const std::vector<EditorSceneObject*>& meshObjects,
                           const std::string& materialId,
                           const ContentRegistry& content,
                           EditorSceneDocument& document);
std::string materialSelectionLabel(const std::vector<EditorSceneObject*>& meshObjects);
void beginPendingCommand(EditorPendingCommand& pending,
                         const EditorSceneDocumentState& beforeState,
                         std::string label);
bool finalizePendingCommand(EditorPendingCommand& pending,
                            EditorCommandStack& commandStack,
                            EditorSceneDocument& document);
void trackLastItemCommand(const EditorSceneDocumentState& beforeState,
                          std::string label,
                          EditorPendingCommand& pending,
                          EditorCommandStack& commandStack,
                          EditorSceneDocument& document);
bool editVec3(const char* label, glm::vec3& value, float speed = 0.01f);
bool editColor(const char* label, glm::vec3& value);
bool editString(const char* label, std::string& value, const char* hint = nullptr);
