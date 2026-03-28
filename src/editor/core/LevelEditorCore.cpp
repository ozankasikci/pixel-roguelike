#include "editor/core/LevelEditorCore.h"

#include "engine/core/PathUtils.h"
#include "editor/EditorCommand.h"
#include "editor/EditorViewportController.h"
#include "game/components/CameraComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/EnvironmentDefinition.h"

#include <filesystem>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>

EditorLayoutVisibility captureLayoutVisibility(const EditorUiState& ui) {
    return EditorLayoutVisibility{
        ui.showOutliner,
        ui.showInspector,
        ui.showAssetBrowser,
        ui.showEnvironment,
        ui.showViewport,
    };
}

void applyLayoutVisibility(EditorUiState& ui, const EditorLayoutVisibility& visibility) {
    ui.showOutliner = visibility.showOutliner;
    ui.showInspector = visibility.showInspector;
    ui.showAssetBrowser = visibility.showAssetBrowser;
    ui.showEnvironment = visibility.showEnvironment;
    ui.showViewport = visibility.showViewport;
}

bool saveLayoutPresetFromUi(const EditorUiState& ui, const std::string& layoutName) {
    if (layoutName.empty()) {
        return false;
    }

    const char* iniText = ImGui::SaveIniSettingsToMemory();
    EditorLayoutPreset preset;
    preset.name = layoutName;
    preset.visibility = captureLayoutVisibility(ui);
    preset.imguiIni = iniText != nullptr ? std::string(iniText) : std::string();
    saveEditorLayoutPreset(editorLayoutPresetPath(layoutName), preset);
    return true;
}

bool loadLayoutPresetIntoUi(EditorUiState& ui, const std::string& layoutName) {
    if (layoutName.empty()) {
        return false;
    }

    const EditorLayoutPreset preset = loadEditorLayoutPreset(editorLayoutPresetPath(layoutName));
    applyLayoutVisibility(ui, preset.visibility);
    ImGui::LoadIniSettingsFromMemory(preset.imguiIni.c_str(), preset.imguiIni.size());
    const std::string storedName = std::filesystem::path(editorLayoutPresetPath(layoutName)).stem().string();
    ui.activeLayoutPreset = storedName;
    std::snprintf(ui.layoutNameBuffer, sizeof(ui.layoutNameBuffer), "%s", preset.name.c_str());
    return true;
}

void buildDefaultEditorDockLayout(ImGuiID dockspaceId,
                                  const ImVec2& dockspaceSize,
                                  const char* viewportWindowName,
                                  const char* outlinerWindowName,
                                  const char* inspectorWindowName,
                                  const char* assetBrowserWindowName,
                                  const char* environmentWindowName) {
    if (dockspaceSize.x <= 0.0f || dockspaceSize.y <= 0.0f) {
        return;
    }

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, dockspaceSize);

    ImGuiID centerId = dockspaceId;
    ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.22f, nullptr, &centerId);
    ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.26f, nullptr, &centerId);
    ImGuiID bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.24f, nullptr, &centerId);

    ImGui::DockBuilderDockWindow(viewportWindowName, centerId);
    ImGui::DockBuilderDockWindow(outlinerWindowName, leftId);
    ImGui::DockBuilderDockWindow(inspectorWindowName, rightId);
    ImGui::DockBuilderDockWindow(environmentWindowName, rightId);
    ImGui::DockBuilderDockWindow(assetBrowserWindowName, bottomId);
    ImGui::DockBuilderFinish(dockspaceId);
}

void resetEditorCameraToRuntimeDefaults(EditorCamera& camera) {
    const CameraComponent runtimeDefaults;
    camera.yawDegrees = runtimeDefaults.yaw;
    camera.pitchDegrees = runtimeDefaults.pitch;
    camera.fovDegrees = runtimeDefaults.fov;
    camera.moveSpeed = runtimeDefaults.moveSpeed;
    camera.nearPlane = runtimeDefaults.nearPlane;
    camera.farPlane = runtimeDefaults.farPlane;
}

bool syncEditorCameraToRuntimeStart(const EditorSceneDocument& document, EditorCamera& camera) {
    resetEditorCameraToRuntimeDefaults(camera);
    for (const auto& object : document.objects()) {
        if (object.kind != EditorSceneObjectKind::PlayerSpawn) {
            continue;
        }
        camera.position = std::get<LevelPlayerSpawn>(object.payload).position;
        return true;
    }
    return false;
}

void loadSceneIntoEditor(const std::string& scenePath,
                         EditorUiState& ui,
                         EditorSceneDocument& document,
                         ContentRegistry& content,
                         EditorCommandStack& commandStack,
                         std::vector<std::uint64_t>& selectedIds,
                         EditorSelectionPickerState& selectionPicker,
                         EditorPlacementState& placementState,
                         EditorPendingCommand& widgetCommand,
                         EditorPendingCommand& gizmoCommand,
                         bool& previewDirty,
                         EditorCamera& editCamera,
                         EditorPreviewWorld& previewWorld,
                         std::uint64_t& previewSceneRevision) {
    ui.pendingScenePath = scenePath;
    document.loadFromSceneFile(scenePath, content);
    commandStack.reset(document);
    selectedIds.clear();
    selectionPicker.clear();
    placementState.clear();
    widgetCommand.clear();
    gizmoCommand.clear();
    previewDirty = true;
    previewWorld.rebuild(document, content);
    previewDirty = false;
    previewSceneRevision = document.sceneRevision();
    if (!syncEditorCameraToRuntimeStart(document, editCamera) && previewWorld.sceneBounds().valid) {
        focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
    }
}

std::vector<std::string> sortedScenePaths() {
    namespace fs = std::filesystem;
    std::vector<std::string> results;
    const fs::path sceneDir(resolveProjectPath("assets/scenes"));
    if (!fs::exists(sceneDir)) {
        return results;
    }
    for (const auto& entry : fs::directory_iterator(sceneDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".scene") {
            continue;
        }
        results.push_back(fs::relative(entry.path(), fs::current_path()).generic_string());
    }
    std::sort(results.begin(), results.end());
    return results;
}

std::vector<std::string> sortedMeshNames(const MeshLibrary& library) {
    auto names = library.names();
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> sortedMaterialIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.materials().size());
    for (const auto& [id, definition] : content.materials()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> sortedArchetypeIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.archetypes().size());
    for (const auto& [id, definition] : content.archetypes()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> sortedEnvironmentIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.environments().size());
    for (const auto& [id, definition] : content.environments()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

LightingEnvironment makeLightingEnvironment(const EnvironmentDefinition& environment) {
    LightingEnvironment lighting = environment.lighting;
    const auto normalize = [](const glm::vec3& value, const glm::vec3& fallback) {
        if (glm::dot(value, value) <= 0.0001f) {
            return fallback;
        }
        return glm::normalize(value);
    };
    lighting.sun.direction = normalize(lighting.sun.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    lighting.fill.direction = normalize(lighting.fill.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    return lighting;
}
