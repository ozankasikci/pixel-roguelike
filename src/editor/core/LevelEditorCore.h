#pragma once

#include "editor/EditorLayoutPreset.h"
#include "editor/EditorPreviewWorld.h"
#include "editor/LevelEditorUi.h"
#include "editor/viewport/EditorViewportInteraction.h"

#include <imgui.h>

#include <cstdint>
#include <string>
#include <vector>

class ContentRegistry;
class MeshLibrary;
class EditorCommandStack;

struct LightingEnvironment;
struct EnvironmentDefinition;
struct EditorCamera;

EditorLayoutVisibility captureLayoutVisibility(const EditorUiState& ui);
void applyLayoutVisibility(EditorUiState& ui, const EditorLayoutVisibility& visibility);
bool saveLayoutPresetFromUi(const EditorUiState& ui, const std::string& layoutName);
bool loadLayoutPresetIntoUi(EditorUiState& ui, const std::string& layoutName);
void buildDefaultEditorDockLayout(ImGuiID dockspaceId,
                                  const ImVec2& dockspaceSize,
                                  const char* viewportWindowName,
                                  const char* outlinerWindowName,
                                  const char* inspectorWindowName,
                                  const char* assetBrowserWindowName,
                                  const char* environmentWindowName);

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
                         EditorCamera& playCamera,
                         EditorPreviewWorld& previewWorld,
                         std::uint64_t& previewSceneRevision);

std::vector<std::string> sortedScenePaths();
std::vector<std::string> sortedMeshNames(const MeshLibrary& library);
std::vector<std::string> sortedMaterialIds(const ContentRegistry& content);
std::vector<std::string> sortedArchetypeIds(const ContentRegistry& content);
std::vector<std::string> sortedEnvironmentIds(const ContentRegistry& content);

LightingEnvironment makeLightingEnvironment(const EnvironmentDefinition& environment);
