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

class ContentRegistry;

enum class EditorPreviewMode {
    Final,
    LightingOnly,
    SkyOnly,
};

enum class EditorPlacementKind {
    None,
    Mesh,
    PointLight,
    SpotLight,
    DirectionalLight,
    BoxCollider,
    CylinderCollider,
    PlayerSpawn,
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
    bool playPreview = false;
    bool showOutliner = true;
    bool showInspector = true;
    bool showAssetBrowser = true;
    bool showEnvironment = true;
    bool showViewport = true;
    bool showColliders = false;
    bool showLightHelpers = false;
    bool showSpawnMarker = false;
    bool snappingEnabled = true;
    float moveSnap = 0.5f;
    float rotateSnap = 15.0f;
    float scaleSnap = 0.1f;
    int shadowResolutionIndex = 1;
    std::string selectedMeshId = "cube";
    std::string selectedMaterialId = "stone_default";
    std::string selectedArchetypeId = "checkpoint_shrine";
    std::string pendingScenePath = "assets/scenes/silos_cloister.scene";
    std::string activeLayoutPreset = "default";
    std::string selectedAssetPath = "assets";
    std::unordered_set<std::string> expandedAssetPaths{"assets"};
    EditorInspectorContext inspectorContext = EditorInspectorContext::None;
    EditorInspectedAsset inspectedAsset{};
    bool frameSelectionRequested = false;
    char layoutNameBuffer[64] = "default";
};

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
};

struct InspectorActionResult {
    bool previewDirty = false;
    bool materialDirty = false;
    bool contentReloaded = false;
};

MaterialKind resolvePlacementMaterialKind(const std::string& materialId, const ContentRegistry& content);
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
bool editVec3(const char* label, glm::vec3& value, float speed = 0.1f);
bool editColor(const char* label, glm::vec3& value);
bool editString(const char* label, std::string& value, const char* hint = nullptr);
