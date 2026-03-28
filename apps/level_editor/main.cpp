#include "engine/core/PathUtils.h"
#include "engine/core/Window.h"
#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/lighting/ShadowMap.h"
#include "engine/rendering/post/CompositePass.h"
#include "engine/rendering/post/PostProcessParams.h"
#include "engine/rendering/post/StylizePass.h"
#include "engine/ui/ImGuiLayer.h"
#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorPreviewWorld.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/scene/EditorSelectionSystem.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/core/EditorRuntimePreviewSession.h"
#include "editor/core/LevelEditorCore.h"
#include "editor/render/EditorScenePreviewRenderer.h"
#include "editor/ui/EditorOutlinerPanel.h"
#include "editor/ui/EditorPanels.h"
#include "editor/viewport/EditorViewportController.h"
#include "editor/viewport/EditorViewportInteraction.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"
#include "game/session/EquipmentState.h"
#include "game/ui/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr int kMaxShadowedSpotLightsLocal = 2;
constexpr int kShadowResolutions[] = {512, 1024, 2048};
constexpr const char* kEditorRootWindowName = "Level Editor Root";
constexpr const char* kViewportWindowName = "Viewport";
constexpr const char* kOutlinerWindowName = "Outliner";
constexpr const char* kInspectorWindowName = "Inspector";
constexpr const char* kAssetBrowserWindowName = "Asset Browser";
constexpr const char* kEnvironmentWindowName = "Environment";
constexpr float kRuntimeViewportAspect = 1280.0f / 720.0f;

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

EditorViewportState fitViewportToAspect(const EditorViewportState& viewport, float aspect) {
    EditorViewportState fitted = viewport;
    if (viewport.size.x <= 1.0f || viewport.size.y <= 1.0f || aspect <= 0.0f) {
        return fitted;
    }

    const float currentAspect = viewport.size.x / viewport.size.y;
    if (currentAspect > aspect) {
        fitted.size.x = viewport.size.y * aspect;
        fitted.origin.x += (viewport.size.x - fitted.size.x) * 0.5f;
    } else if (currentAspect < aspect) {
        fitted.size.y = viewport.size.x / aspect;
        fitted.origin.y += (viewport.size.y - fitted.size.y) * 0.5f;
    }
    return fitted;
}

} // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    const std::string initialScene = argc > 1 ? argv[1] : "assets/scenes/silos_cloister.scene";

    Window window(1600, 960, "Level Editor");
    glfwSwapInterval(1);

    ImGuiLayer imgui;
    imgui.init(window.handle());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    ContentRegistry content;
    content.loadDefaults();

    MaterialTextureLibrary materialTextures;
    materialTextures.init(content);

    EditorSceneDocument document;
    document.loadFromSceneFile(initialScene, content);
    EditorCommandStack commandStack;
    commandStack.reset(document);

    EditorPreviewWorld previewWorld;
    previewWorld.rebuild(document, content);
    EditorRuntimePreviewSession runtimePreviewSession;
    runtimePreviewSession.rebuild(document, content);

    std::unique_ptr<Shader> sceneShader = std::make_unique<Shader>(
        "assets/shaders/game/scene.vert",
        "assets/shaders/game/scene.frag"
    );
    std::unique_ptr<Shader> shadowShader = std::make_unique<Shader>(
        "assets/shaders/engine/shadow_depth.vert",
        "assets/shaders/engine/shadow_depth.frag"
    );
    Renderer renderer(sceneShader.get());
    CompositePass compositePass;
    StylizePass stylizePass;

    Framebuffer sceneFbo;
    Framebuffer compositeFbo;
    Framebuffer finalFbo;
    sceneFbo.create(1280, 720);
    compositeFbo.create(1280, 720);
    finalFbo.create(1280, 720);

    std::array<ShadowMap, kMaxShadowedSpotLightsLocal> shadowMaps;

    EditorCamera editCamera;
    if (!syncEditorCameraToRuntimeStart(document, editCamera) && previewWorld.sceneBounds().valid) {
        focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
    }
    EditorUiState ui;
    ui.pendingScenePath = initialScene;
    EditorPlacementState placementState;
    std::vector<std::uint64_t> selectedIds;
    EditorSelectionPickerState selectionPicker;

    auto scenePaths = sortedScenePaths();
    auto meshIds = sortedMeshNames(previewWorld.meshLibrary());
    auto materialIds = sortedMaterialIds(content);
    auto archetypeIds = sortedArchetypeIds(content);
    auto environmentIds = sortedEnvironmentIds(content);
    auto layoutPresetNames = listEditorLayoutPresetNames();
    bool dockLayoutResetRequested = false;

    if (!materialIds.empty()) {
        ui.selectedMaterialId = materialIds.front();
    }
    if (!meshIds.empty()) {
        ui.selectedMeshId = meshIds.front();
    }
    if (!archetypeIds.empty()) {
        ui.selectedArchetypeId = archetypeIds.front();
    }
    if (std::find(layoutPresetNames.begin(), layoutPresetNames.end(), ui.activeLayoutPreset) != layoutPresetNames.end()) {
        try {
            loadLayoutPresetIntoUi(ui, ui.activeLayoutPreset);
        } catch (const std::exception& ex) {
            spdlog::warn("Failed to load editor layout '{}': {}", ui.activeLayoutPreset, ex.what());
            dockLayoutResetRequested = true;
        }
    } else {
        dockLayoutResetRequested = true;
    }

    bool previewDirty = true;
    bool savePressed = false;
    bool focusPressed = false;
    bool resetStartPressed = false;
    bool duplicatePressed = false;
    bool deletePressed = false;
    bool undoPressed = false;
    bool redoPressed = false;
    bool playTogglePressed = false;
    std::uint64_t previewSceneRevision = document.sceneRevision();
    bool runtimePreviewNeedsRebuild = false;
    EditorPendingCommand widgetCommand;
    EditorPendingCommand gizmoCommand;

    while (!window.shouldClose()) {
        window.pollEvents();
        if (ui.playPreview && runtimePreviewSession.captured() && glfwGetWindowAttrib(window.handle(), GLFW_FOCUSED) == 0) {
            runtimePreviewSession.endCapture(window.handle());
        }
        imgui.beginFrame();
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        ImGuizmo::BeginFrame();

        const float deltaTime = 1.0f / std::max(ImGui::GetIO().Framerate, 1.0f);
        ImGuiIO& io = ImGui::GetIO();
        const bool gameplayPreviewCaptured = ui.playPreview && runtimePreviewSession.captured();

        if (!gameplayPreviewCaptured) {
            if (ImGui::IsKeyPressed(ImGuiKey_F)) focusPressed = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_D)) duplicatePressed = true;
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                if (io.KeyShift) {
                    redoPressed = true;
                } else {
                    undoPressed = true;
                }
            }
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                redoPressed = true;
            }
            if (!io.WantTextInput && glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
                if (ImGui::IsKeyPressed(ImGuiKey_W)) ui.tool = EditorTransformTool::Translate;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) ui.tool = EditorTransformTool::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) ui.tool = EditorTransformTool::Scale;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                placementState.clear();
                widgetCommand.clear();
                gizmoCommand.clear();
            }
        }

        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        ImGui::Begin(kEditorRootWindowName, nullptr,
                     ImGuiWindowFlags_NoDecoration
                     | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoBringToFrontOnFocus
                     | ImGuiWindowFlags_NoNavFocus
                     | ImGuiWindowFlags_NoDocking);

        if (gameplayPreviewCaptured) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Save")) {
            savePressed = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!commandStack.canUndo());
        if (ImGui::Button("Undo")) {
            undoPressed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!commandStack.canRedo());
        if (ImGui::Button("Redo")) {
            redoPressed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(ui.playPreview ? "Stop Preview" : "Play Preview")) {
            playTogglePressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Start")) {
            resetStartPressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Focus")) {
            focusPressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            duplicatePressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            deletePressed = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(document.dirty() ? "* Unsaved" : "Saved");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::BeginCombo("Layout", ui.activeLayoutPreset.c_str())) {
            for (const auto& layoutName : layoutPresetNames) {
                const bool selected = (layoutName == ui.activeLayoutPreset);
                if (ImGui::Selectable(layoutName.c_str(), selected)) {
                    try {
                        loadLayoutPresetIntoUi(ui, layoutName);
                    } catch (const std::exception& ex) {
                        spdlog::error("Layout load failed: {}", ex.what());
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("Layout Name", ui.layoutNameBuffer, sizeof(ui.layoutNameBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Save Layout")) {
            try {
                const std::string layoutName(ui.layoutNameBuffer);
                if (saveLayoutPresetFromUi(ui, layoutName)) {
                    ui.activeLayoutPreset = std::filesystem::path(editorLayoutPresetPath(layoutName)).stem().string();
                    layoutPresetNames = listEditorLayoutPresetNames();
                }
            } catch (const std::exception& ex) {
                spdlog::error("Layout save failed: {}", ex.what());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Layout")) {
            try {
                loadLayoutPresetIntoUi(ui, ui.activeLayoutPreset);
            } catch (const std::exception& ex) {
                spdlog::error("Layout reload failed: {}", ex.what());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Dock Layout")) {
            dockLayoutResetRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Panels")) {
            ImGui::OpenPopup("PanelVisibility");
        }
        if (ImGui::BeginPopup("PanelVisibility")) {
            ImGui::Checkbox("Outliner", &ui.showOutliner);
            ImGui::Checkbox("Inspector", &ui.showInspector);
            ImGui::Checkbox("Asset Browser", &ui.showAssetBrowser);
            ImGui::Checkbox("Environment", &ui.showEnvironment);
            ImGui::Checkbox("Viewport", &ui.showViewport);
            ImGui::EndPopup();
        }

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Scene", ui.pendingScenePath.c_str())) {
            for (const auto& scenePath : scenePaths) {
                const bool selected = (scenePath == ui.pendingScenePath);
                if (ImGui::Selectable(scenePath.c_str(), selected)) {
                    loadSceneIntoEditor(scenePath,
                                        ui,
                                        document,
                                        content,
                                        commandStack,
                                        selectedIds,
                                        selectionPicker,
                                        placementState,
                                        widgetCommand,
                                        gizmoCommand,
                                        previewDirty,
                                        editCamera,
                                        previewWorld,
                                        previewSceneRevision);
                    runtimePreviewNeedsRebuild = true;
                    if (ui.playPreview) {
                        runtimePreviewSession.endCapture(window.handle());
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        const char* toolLabel = ui.tool == EditorTransformTool::Translate ? "Translate"
            : ui.tool == EditorTransformTool::Rotate ? "Rotate"
            : "Scale";
        ImGui::Text("Tool: %s", toolLabel);
        ImGui::SameLine();
        if (ImGui::Button("W")) ui.tool = EditorTransformTool::Translate;
        ImGui::SameLine();
        if (ImGui::Button("E")) ui.tool = EditorTransformTool::Rotate;
        ImGui::SameLine();
        if (ImGui::Button("R")) ui.tool = EditorTransformTool::Scale;
        ImGui::SameLine();
        const char* previewModes[] = {"Final", "Lighting", "Sky"};
        int previewModeIndex = static_cast<int>(ui.previewMode);
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##previewmode", &previewModeIndex, previewModes, 3)) {
            ui.previewMode = static_cast<EditorPreviewMode>(previewModeIndex);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &ui.snappingEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Move", &ui.moveSnap, 0.05f, 0.05f, 10.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Rot", &ui.rotateSnap, 1.0f, 1.0f, 90.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Scale", &ui.scaleSnap, 0.01f, 0.01f, 2.0f, "%.2f");
        ImGui::SameLine();
        ImGui::Checkbox("Colliders", &ui.showColliders);
        ImGui::SameLine();
        ImGui::Checkbox("Light Helpers", &ui.showLightHelpers);
        ImGui::SameLine();
        ImGui::Checkbox("Spawn Marker", &ui.showSpawnMarker);
        ImGui::SameLine();
        if (ImGui::Button("Mesh")) beginPlacement(placementState, EditorPlacementKind::Mesh, ui.selectedMeshId, ui.selectedMaterialId);
        ImGui::SameLine();
        if (ImGui::Button("Point")) beginPlacement(placementState, EditorPlacementKind::PointLight);
        ImGui::SameLine();
        if (ImGui::Button("Spot")) beginPlacement(placementState, EditorPlacementKind::SpotLight);
        ImGui::SameLine();
        if (ImGui::Button("Dir")) beginPlacement(placementState, EditorPlacementKind::DirectionalLight);
        ImGui::SameLine();
        if (ImGui::Button("Box")) beginPlacement(placementState, EditorPlacementKind::BoxCollider);
        ImGui::SameLine();
        if (ImGui::Button("Cyl")) beginPlacement(placementState, EditorPlacementKind::CylinderCollider);
        ImGui::SameLine();
        if (ImGui::Button("Spawn")) beginPlacement(placementState, EditorPlacementKind::PlayerSpawn);
        ImGui::SameLine();
        if (ImGui::Button("Archetype")) beginPlacement(placementState, EditorPlacementKind::Archetype, ui.selectedArchetypeId);

        if (gameplayPreviewCaptured) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();
        const ImVec2 dockspaceSize = ImGui::GetContentRegionAvail();
        const ImGuiID dockspaceId = ImGui::GetID("LevelEditorDockspace");
        if (dockLayoutResetRequested || ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
            buildDefaultEditorDockLayout(dockspaceId,
                                         dockspaceSize,
                                         kViewportWindowName,
                                         kOutlinerWindowName,
                                         kInspectorWindowName,
                                         kAssetBrowserWindowName,
                                         kEnvironmentWindowName);
            dockLayoutResetRequested = false;
        }
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();

        if (gameplayPreviewCaptured) {
            ImGui::BeginDisabled();
        }
        const std::vector<std::uint64_t> outlinerDeleteRequests = renderOutliner(document, ui, selectedIds, &ui.showOutliner, commandStack);
        const InspectorActionResult inspectorActions = renderInspector(ui,
                                                                       document,
                                                                       selectedIds,
                                                                       content,
                                                                       meshIds,
                                                                       materialIds,
                                                                       archetypeIds,
                                                                       &ui.showInspector,
                                                                       widgetCommand,
                                                                       commandStack);
        if (inspectorActions.contentReloaded) {
            materialIds = sortedMaterialIds(content);
            archetypeIds = sortedArchetypeIds(content);
            environmentIds = sortedEnvironmentIds(content);
            materialTextures.init(content);
            previewDirty = true;
        }
        if (inspectorActions.previewDirty) {
            previewDirty = true;
        }
        if (renderEnvironmentPanel(document, content, environmentIds, &ui.showEnvironment, widgetCommand, commandStack)) {
            previewDirty = true;
        }
        const AssetBrowserActionResult assetBrowserActions = renderAssetBrowser(ui,
                                                                                placementState,
                                                                                document,
                                                                                selectedIds,
                                                                                content,
                                                                                meshIds,
                                                                                materialIds,
                                                                                archetypeIds,
                                                                                &ui.showAssetBrowser,
                                                                                commandStack);
        if (gameplayPreviewCaptured) {
            ImGui::EndDisabled();
        }
        if (assetBrowserActions.openScenePath.has_value()) {
            loadSceneIntoEditor(*assetBrowserActions.openScenePath,
                                ui,
                                document,
                                content,
                                commandStack,
                                selectedIds,
                                selectionPicker,
                                placementState,
                                widgetCommand,
                                gizmoCommand,
                                previewDirty,
                                editCamera,
                                previewWorld,
                                previewSceneRevision);
            runtimePreviewNeedsRebuild = true;
            if (ui.playPreview) {
                runtimePreviewSession.endCapture(window.handle());
            }
        }
        if (assetBrowserActions.previewDirty) {
            previewDirty = true;
        }

        EditorViewportState viewportState;
        bool viewportWindowBegun = false;
        bool viewportWindowVisible = false;
        if (ui.showViewport) {
            viewportWindowBegun = true;
            viewportWindowVisible = ImGui::Begin("Viewport",
                                                 &ui.showViewport,
                                                 ImGuiWindowFlags_NoScrollbar
                                                 | ImGuiWindowFlags_NoScrollWithMouse);
            if (viewportWindowVisible) {
                viewportState.focused = ImGui::IsWindowFocused();
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                viewportState.size = ImVec2(std::max(avail.x, 64.0f), std::max(avail.y, 64.0f));
                viewportState.origin = ImGui::GetCursorScreenPos();
                viewportState.hovered = pointInViewport(viewportState, io.MousePos);
            }
        }

        EditorViewportState renderViewportState = viewportState;
        if (ui.playPreview) {
            renderViewportState = fitViewportToAspect(viewportState, kRuntimeViewportAspect);
            renderViewportState.hovered = pointInViewport(renderViewportState, io.MousePos);
        }

        const int targetW = std::max(1, static_cast<int>(std::max(renderViewportState.size.x, 64.0f)));
        const int targetH = std::max(1, static_cast<int>(std::max(renderViewportState.size.y, 64.0f)));
        if (sceneFbo.width() != targetW || sceneFbo.height() != targetH) {
            sceneFbo.resize(targetW, targetH);
            compositeFbo.resize(targetW, targetH);
            finalFbo.resize(targetW, targetH);
        }

        if (previewDirty || previewSceneRevision != document.sceneRevision()) {
            previewWorld.rebuild(document, content);
            previewDirty = false;
            previewSceneRevision = document.sceneRevision();
            runtimePreviewNeedsRebuild = true;
        }
        const auto selectionHandles = buildEditorSelectionHandles(document, previewWorld);
        const auto viewportSelectionHandles = buildViewportSelectionHandles(selectionHandles, ui);
        std::optional<glm::vec3> placementPoint;
        glm::mat4 view(1.0f);
        glm::mat4 projection(1.0f);
        glm::mat4 inverseViewProjection(1.0f);

        if (!ui.playPreview) {
            updateEditorFlyCamera(editCamera, window.handle(), renderViewportState, deltaTime);

            view = editorCameraView(editCamera);
            projection = editorCameraProjection(editCamera, static_cast<float>(targetW) / static_cast<float>(targetH));
            inverseViewProjection = glm::inverse(projection * view);

            std::vector<RenderObject> objects = collectRenderObjects(previewWorld, materialTextures, selectedIds);
            appendHelperObjects(objects, previewWorld, document, materialTextures, selectedIds,
                                ui.showColliders, ui.showLightHelpers, ui.showSpawnMarker);
            appendSelectionOverlays(objects, previewWorld, materialTextures, selectedIds);

            EditorPlacementState dragPlacement;
            if (viewportState.hovered) {
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                    payload != nullptr && payload->IsDataType("EDITOR_PLACE") && payload->DataSize == sizeof(EditorDragPayload)) {
                    dragPlacement = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
                }
            }

            const EditorPlacementState effectivePlacement = dragPlacement.active() ? dragPlacement : placementState;
            if (effectivePlacement.active() && pointInViewport(renderViewportState, io.MousePos)) {
                const EditorRay ray = buildEditorRay(inverseViewProjection,
                                                     glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                                                     glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                                                     glm::vec2(io.MousePos.x, io.MousePos.y));
                placementPoint = computePlacementPoint(selectionHandles, ray, editCamera, ui.snappingEnabled, ui.moveSnap);
                if (placementPoint.has_value()) {
                    appendPlacementGhost(objects, effectivePlacement, *placementPoint, previewWorld, materialTextures, content);
                }
            }

            EnvironmentDefinition previewEnvironment = document.environment();
            previewEnvironment.post.timeSeconds = static_cast<float>(glfwGetTime());
            previewEnvironment.post.nearPlane = editCamera.nearPlane;
            previewEnvironment.post.farPlane = editCamera.farPlane;
            previewEnvironment.post.inverseViewProjection = inverseViewProjection;
            previewEnvironment.post.sky = previewEnvironment.sky;
            previewEnvironment.post.sky.sunDirection = safeNormalize(previewEnvironment.lighting.sun.direction, previewEnvironment.post.sky.sunDirection);
            previewEnvironment.post.sky.sunColor = previewEnvironment.lighting.sun.color;
            switch (ui.previewMode) {
            case EditorPreviewMode::Final:
                previewEnvironment.post.debugViewMode = 0;
                break;
            case EditorPreviewMode::LightingOnly:
                previewEnvironment.post.debugViewMode = 1;
                break;
            case EditorPreviewMode::SkyOnly:
                previewEnvironment.post.debugViewMode = 4;
                break;
            }

            std::vector<RenderLight> lights = collectLights(previewWorld.registry(), previewEnvironment);
            ShadowRenderData shadowData;
            if (previewEnvironment.lighting.enableShadows) {
                renderShadowPass(objects, lights, *shadowShader, shadowMaps, kShadowResolutions[ui.shadowResolutionIndex], shadowData);
            } else {
                shadowData.shadowCount = 0;
                shadowData.matrices = {glm::mat4(1.0f), glm::mat4(1.0f)};
                shadowData.textures = {0, 0};
            }

            sceneFbo.bind();
            glViewport(0, 0, targetW, targetH);
            glEnable(GL_DEPTH_TEST);
            glClearColor(0.04f, 0.04f, 0.045f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.drawScene(objects,
                               lights,
                               makeLightingEnvironment(previewEnvironment),
                               shadowData,
                               view,
                               projection,
                               editCamera.position);
            sceneFbo.unbind();
            glDisable(GL_DEPTH_TEST);

            compositePass.apply(sceneFbo.colorTexture(),
                                sceneFbo.depthTexture(),
                                sceneFbo.normalTexture(),
                                compositeFbo.framebuffer(),
                                previewEnvironment.post,
                                targetW,
                                targetH);
            stylizePass.apply(compositeFbo.colorTexture(),
                              sceneFbo.colorTexture(),
                              sceneFbo.depthTexture(),
                              sceneFbo.normalTexture(),
                              previewEnvironment.post,
                              targetW,
                              targetH,
                              finalFbo.framebuffer());
        } else {
            runtimePreviewSession.updateInput(window.handle(), io);
            runtimePreviewSession.tick(deltaTime, kRuntimeViewportAspect);
            runtimePreviewSession.syncCursor(window.handle());
            if (runtimePreviewSession.input().isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                runtimePreviewSession.endCapture(window.handle());
            }
            runtimePreviewSession.render(deltaTime,
                                         targetW,
                                         targetH,
                                         targetW,
                                         targetH,
                                         finalFbo.framebuffer());
        }

        if (viewportWindowVisible) {
            if (ui.playPreview) {
                ImGui::SetCursorScreenPos(renderViewportState.origin);
            }
            ImGui::SetNextItemAllowOverlap();
            ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(finalFbo.colorTexture())),
                         renderViewportState.size,
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));

            {
                const char* modeLabel = ui.playPreview ? "Game Preview" : "Edit View";
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 badgePos(renderViewportState.origin.x + 12.0f, renderViewportState.origin.y + 12.0f);
                const ImVec2 badgeSize = ImGui::CalcTextSize(modeLabel);
                drawList->AddRectFilled(ImVec2(badgePos.x - 8.0f, badgePos.y - 6.0f),
                                        ImVec2(badgePos.x + badgeSize.x + 8.0f, badgePos.y + badgeSize.y + 6.0f),
                                        ui.playPreview ? IM_COL32(24, 34, 48, 220) : IM_COL32(26, 32, 24, 220),
                                        4.0f);
                drawList->AddText(badgePos,
                                  ui.playPreview ? IM_COL32(184, 216, 255, 255) : IM_COL32(200, 236, 168, 255),
                                  modeLabel);
            }

            if (ui.playPreview) {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (!runtimePreviewSession.captured()) {
                    const char* captureLabel = "Click to Capture";
                    const char* releaseLabel = "Esc releases back to the editor";
                    const ImVec2 captureSize = ImGui::CalcTextSize(captureLabel);
                    const ImVec2 releaseSize = ImGui::CalcTextSize(releaseLabel);
                    const float boxWidth = std::max(captureSize.x, releaseSize.x) + 28.0f;
                    const float boxHeight = captureSize.y + releaseSize.y + 28.0f;
                    const ImVec2 boxMin(renderViewportState.origin.x + (renderViewportState.size.x - boxWidth) * 0.5f,
                                        renderViewportState.origin.y + (renderViewportState.size.y - boxHeight) * 0.5f);
                    const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);
                    drawList->AddRectFilled(boxMin, boxMax, IM_COL32(12, 14, 18, 210), 6.0f);
                    drawList->AddText(ImVec2(boxMin.x + 14.0f, boxMin.y + 9.0f),
                                      IM_COL32(230, 236, 255, 255),
                                      captureLabel);
                    drawList->AddText(ImVec2(boxMin.x + 14.0f, boxMin.y + 15.0f + captureSize.y),
                                      IM_COL32(176, 184, 208, 255),
                                      releaseLabel);
                } else {
                    const char* releaseLabel = "Esc to Release";
                    const ImVec2 releasePos(renderViewportState.origin.x + 12.0f, renderViewportState.origin.y + 42.0f);
                    const ImVec2 releaseSize = ImGui::CalcTextSize(releaseLabel);
                    drawList->AddRectFilled(ImVec2(releasePos.x - 8.0f, releasePos.y - 6.0f),
                                            ImVec2(releasePos.x + releaseSize.x + 8.0f, releasePos.y + releaseSize.y + 6.0f),
                                            IM_COL32(22, 24, 28, 210),
                                            4.0f);
                    drawList->AddText(releasePos, IM_COL32(230, 236, 255, 255), releaseLabel);
                }

                if (runtimePreviewNeedsRebuild) {
                    const char* rebuildLabel = "Scene changes pending: Reset Start to apply";
                    const ImVec2 textSize = ImGui::CalcTextSize(rebuildLabel);
                    const ImVec2 textPos(renderViewportState.origin.x + renderViewportState.size.x - textSize.x - 22.0f,
                                         renderViewportState.origin.y + 14.0f);
                    drawList->AddRectFilled(ImVec2(textPos.x - 8.0f, textPos.y - 6.0f),
                                            ImVec2(textPos.x + textSize.x + 8.0f, textPos.y + textSize.y + 6.0f),
                                            IM_COL32(44, 34, 18, 220),
                                            4.0f);
                    drawList->AddText(textPos, IM_COL32(255, 224, 168, 255), rebuildLabel);
                }

                if (!runtimePreviewSession.captured()
                    && renderViewportState.hovered
                    && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    runtimePreviewSession.beginCapture(window.handle());
                }
            }

            if (!ui.playPreview && !selectedIds.empty()) {
                const std::uint64_t activeId = selectedIds.back();
                std::string label = selectedIds.size() == 1
                    ? "Selected: "
                    : (std::to_string(selectedIds.size()) + " selected | Active: ");
                if (const EditorSceneObject* selectedObject = document.findObject(activeId)) {
                    label += editorSceneObjectLabel(*selectedObject);
                } else {
                    label += "Object #" + std::to_string(activeId);
                }

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 textPos(renderViewportState.origin.x + 12.0f, renderViewportState.origin.y + 42.0f);
                const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                const ImVec2 padding(8.0f, 6.0f);
                drawList->AddRectFilled(ImVec2(textPos.x - padding.x, textPos.y - padding.y),
                                        ImVec2(textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y),
                                        IM_COL32(18, 20, 24, 215),
                                        4.0f);
                drawList->AddText(textPos, IM_COL32(255, 236, 168, 255), label.c_str());
            }

            const EditorSceneDocumentState gizmoBeforeState = document.captureState();
            if (!ui.playPreview) {
                if (applyGizmoToSelectedObject(document, selectedIds, renderViewportState, view, projection, ui, previewWorld)) {
                    previewDirty = true;
                }
                if (editorGizmoIsHot() && !gizmoCommand.active) {
                    beginPendingCommand(gizmoCommand, gizmoBeforeState, "Transform Object");
                } else if (gizmoCommand.active && !editorGizmoIsHot()) {
                    finalizePendingCommand(gizmoCommand, commandStack, document);
                }
            }

            if (!ui.playPreview) {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_PLACE", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                        if (payload->Delivery && placementPoint.has_value() && payload->DataSize == sizeof(EditorDragPayload)) {
                            const EditorPlacementState droppedState = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
                            const EditorSceneDocumentState beforeState = document.captureState();
                            commitPlacement(document, droppedState, *placementPoint, content, editCamera);
                            commandStack.pushDocumentStateCommand(
                                "Place Object",
                                beforeState,
                                document.captureState(),
                                document);
                            selectionPicker.clear();
                            previewDirty = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (placementState.active() && placementPoint.has_value() && renderViewportState.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editorGizmoIsHot()) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    commitPlacement(document, placementState, *placementPoint, content, editCamera);
                    commandStack.pushDocumentStateCommand(
                        "Place Object",
                        beforeState,
                        document.captureState(),
                        document);
                    placementState.clear();
                    selectionPicker.clear();
                    previewDirty = true;
                } else if (renderViewportState.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editorGizmoIsHot()) {
                    const EditorRay ray = buildEditorRay(inverseViewProjection,
                                                         glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                                                         glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                                                         glm::vec2(io.MousePos.x, io.MousePos.y));
                    const std::vector<EditorHitResult> hits = pickEditorObjects(viewportSelectionHandles, ray);
                    if (!hits.empty()) {
                        refreshSelectionPicker(selectionPicker,
                                               hits,
                                               io.MousePos,
                                               glfwGetTime(),
                                               hits.size() > 1);
                        applySelectionHit(selectedIds, selectionPicker, io.KeyShift);
                        ui.inspectorContext = EditorInspectorContext::SceneSelection;
                    } else {
                        selectionPicker.clear();
                        if (!io.KeyShift) {
                            selectedIds.clear();
                        }
                        ui.inspectorContext = EditorInspectorContext::SceneSelection;
                    }
                }
            }

            if (!ui.playPreview) {
                renderSelectionPicker(selectionPicker, document, selectedIds, glfwGetTime());
            }
        }

        if (viewportWindowBegun) {
            ImGui::End();
        }

        if (ui.playPreview) {
            auto& previewRegistry = runtimePreviewSession.registry();
            const bool inventoryOpen = previewRegistry.ctx().contains<InventoryMenuState>()
                && previewRegistry.ctx().get<InventoryMenuState>().open;
            if (inventoryOpen) {
                auto& menu = previewRegistry.ctx().get<InventoryMenuState>();
                const auto equipment = resolveEffectiveEquipment(runtimePreviewSession.runSession(), content);
                ImGuiLayer::renderInventory(menu, runtimePreviewSession.runSession(), content, equipment);
            }
            if (previewRegistry.ctx().contains<InteractionPromptState>()) {
                const auto& prompt = previewRegistry.ctx().get<InteractionPromptState>();
                if (prompt.visible && !inventoryOpen) {
                    ImGuiLayer::renderInteractionPrompt(prompt.text.c_str(), prompt.busy);
                }
            }
        }

        if (playTogglePressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            ui.playPreview = !ui.playPreview;
            if (ui.playPreview) {
                runtimePreviewSession.endCapture(window.handle());
                if (runtimePreviewNeedsRebuild) {
                    runtimePreviewSession.rebuild(document, content);
                    runtimePreviewNeedsRebuild = false;
                }
            } else {
                runtimePreviewSession.endCapture(window.handle());
                runtimePreviewNeedsRebuild = true;
            }
            playTogglePressed = false;
        }

        if (resetStartPressed) {
            if (ui.playPreview) {
                runtimePreviewSession.endCapture(window.handle());
                runtimePreviewSession.rebuild(document, content);
                runtimePreviewNeedsRebuild = false;
            } else if (!syncEditorCameraToRuntimeStart(document, editCamera) && previewWorld.sceneBounds().valid) {
                focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
            }
            resetStartPressed = false;
        }

        if (focusPressed && selectedIds.size() == 1) {
            if (const EditorObjectBounds* bounds = previewWorld.findObjectBounds(selectedIds.front())) {
                focusEditorCameraOnBounds(editCamera, bounds->min, bounds->max);
            } else if (const EditorSceneObject* object = document.findObject(selectedIds.front())) {
                focusEditorCameraOnPoint(editCamera, editorSceneObjectAnchor(*object));
            }
            focusPressed = false;
        } else {
            focusPressed = false;
        }

        if (undoPressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            if (commandStack.undo(document)) {
                pruneSelection(document, selectedIds);
                selectionPicker.clear();
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                previewDirty = true;
            }
            undoPressed = false;
        }

        if (redoPressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            if (commandStack.redo(document)) {
                pruneSelection(document, selectedIds);
                selectionPicker.clear();
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                previewDirty = true;
            }
            redoPressed = false;
        }

        if (duplicatePressed) {
            const EditorSceneDocumentState beforeState = document.captureState();
            std::vector<std::uint64_t> duplicated;
            for (auto id : selectedIds) {
                const std::uint64_t newId = document.duplicateObject(id);
                if (newId != 0) {
                    duplicated.push_back(newId);
                }
            }
            if (!duplicated.empty()) {
                selectedIds = duplicated;
                selectionPicker.clear();
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                commandStack.pushDocumentStateCommand(
                    "Duplicate Selection",
                    beforeState,
                    document.captureState(),
                    document);
                previewDirty = true;
            }
            duplicatePressed = false;
        }

        if (deletePressed) {
            if (!selectedIds.empty()) {
                const EditorSceneDocumentState beforeState = document.captureState();
                document.eraseObjects(selectedIds);
                commandStack.pushDocumentStateCommand(
                    "Delete Selection",
                    beforeState,
                    document.captureState(),
                    document);
                previewDirty = true;
            }
            selectedIds.clear();
            selectionPicker.clear();
            ui.inspectorContext = EditorInspectorContext::SceneSelection;
            deletePressed = false;
        }

        if (!outlinerDeleteRequests.empty()) {
            const EditorSceneDocumentState beforeState = document.captureState();
            document.eraseObjects(outlinerDeleteRequests);
            commandStack.pushDocumentStateCommand(
                outlinerDeleteRequests.size() == 1 ? "Delete Object" : "Delete Objects",
                beforeState,
                document.captureState(),
                document);
            pruneSelection(document, selectedIds);
            selectionPicker.clear();
            previewDirty = true;
        }

        if (savePressed) {
            try {
                document.save(content);
                content.loadDefaults();
                materialTextures.init(content);
                commandStack.markSaved(document);
                scenePaths = sortedScenePaths();
                materialIds = sortedMaterialIds(content);
                archetypeIds = sortedArchetypeIds(content);
                environmentIds = sortedEnvironmentIds(content);
                previewDirty = true;
            } catch (const std::exception& ex) {
                spdlog::error("Save failed: {}", ex.what());
            }
            savePressed = false;
        }

        imgui.endFrame();
        window.swapBuffers();
    }

    runtimePreviewSession.endCapture(window.handle());
    imgui.shutdown();
    return 0;
}
