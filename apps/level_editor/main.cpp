#include "engine/audio/AudioEngine.h"
#include "engine/core/PathUtils.h"
#include "engine/core/ProjectConfig.h"
#include "engine/core/Window.h"
#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/ui/ImGuiLayer.h"
#include "engine/ui/Screenshot.h"
#include "game/ui/GameOverlays.h"
#include "editor/build/EditorBuildSystem.h"
#include "editor/core/EditorCommand.h"
#include "editor/core/EditorUiPreferences.h"
#include "editor/debug/DebugHarness.h"
#include "editor/scene/EditorPreviewWorld.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/scene/EditorSelectionSystem.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/core/EditorRuntimePreviewSession.h"
#include "editor/core/LevelEditorCore.h"
#include "editor/render/EditorScenePreviewRenderer.h"
#include "editor/render/EditorViewportRenderer.h"
#include "editor/ui/EditorOutlinerPanel.h"
#include "editor/ui/EditorPanels.h"
#include "editor/ui/EditorCameraDebugPanel.h"
#include "editor/ui/EditorGameSettingsPanel.h"
#include "editor/ui/EditorPerformancePanel.h"
#include "editor/viewport/EditorViewportController.h"
#include "editor/viewport/EditorViewportInteraction.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"
#include "game/modules/checkpoint/CheckpointModule.h"
#include "game/modules/door/DoorModule.h"
#include "game/modules/player_control/PlayerControlModule.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"
#include "game/session/EquipmentState.h"
#include "game/settings/GameSettings.h"
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
#include <chrono>
#include <cmath>
#ifndef _WIN32
#include <csignal>
#endif
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

void openFolderInOS(const std::string& path) {
#ifdef __APPLE__
    std::string cmd = "open \"" + path + "\" &";
#elif defined(_WIN32)
    std::string cmd = "explorer \"" + path + "\"";
#else
    std::string cmd = "xdg-open \"" + path + "\" &";
#endif
    std::system(cmd.c_str());
}

volatile int g_editorScreenshotRequested = 0;
#ifndef _WIN32
void editorSignalHandler(int) { g_editorScreenshotRequested = 1; }
#endif

using Clock = std::chrono::steady_clock;

constexpr const char* kEditorRootWindowName = "Level Editor Root";
constexpr const char* kViewportWindowName = "Viewport";
constexpr const char* kOutlinerWindowName = "Outliner";
constexpr const char* kInspectorWindowName = "Inspector";
constexpr const char* kAssetBrowserWindowName = "Asset Browser";
constexpr const char* kEnvironmentWindowName = "Environment";
constexpr float kRuntimeViewportAspect = 1280.0f / 720.0f;
constexpr const char* kPreviewModes[] = {
    "Final",
    "Lighting",
    "Sky",
    "Sun Direct",
    "Sun Shadow",
    "CSM UV",
    "Cascade",
};
constexpr const char* kPreviewQualityLabels[] = {"Fast", "Balanced", "High"};
constexpr const char* kWindowGeometryFile = "editor_window.ini";
constexpr const char* kEditorUiConfigFile = "editor_ui.ini";
constexpr const char* kBuildOutputWindowName = "Build Output";
constexpr const char* kBuildConfigFile = "editor_build.ini";
constexpr double kIdleTimeoutSeconds = 1.0 / 15.0;      // ~15 FPS when idle
constexpr double kUnfocusedTimeoutSeconds = 1.0 / 5.0;  // ~5 FPS when unfocused

// Live resize support: on macOS, glfwPollEvents blocks during a resize
// drag.  We store a pointer to the editor's full-frame render lambda and
// call it from the window-refresh callback so the UI re-layouts at the
// new size in real time.
static std::function<void()>* g_editorRenderFrame = nullptr;

void windowRefreshCallback(GLFWwindow*) {
    if (g_editorRenderFrame) {
        (*g_editorRenderFrame)();
    }
}

struct WindowGeometry {
    int x = -1;
    int y = -1;
    int width = 1600;
    int height = 960;
};

WindowGeometry loadWindowGeometry() {
    WindowGeometry geo;
    std::ifstream file(kWindowGeometryFile);
    if (!file.is_open()) {
        return geo;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (std::sscanf(line.c_str(), "x=%d", &geo.x) == 1) continue;
        if (std::sscanf(line.c_str(), "y=%d", &geo.y) == 1) continue;
        if (std::sscanf(line.c_str(), "width=%d", &geo.width) == 1) continue;
        if (std::sscanf(line.c_str(), "height=%d", &geo.height) == 1) continue;
    }
    return geo;
}

void saveWindowGeometry(GLFWwindow* window) {
    int x = 0, y = 0;
    glfwGetWindowPos(window, &x, &y);
    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
    std::ofstream file(kWindowGeometryFile);
    if (!file.is_open()) {
        return;
    }
    file << "x=" << x << "\n";
    file << "y=" << y << "\n";
    file << "width=" << w << "\n";
    file << "height=" << h << "\n";
}

enum class RuntimePreviewDirtyState {
    None,
    EnvironmentOnly,
    GameplayStateReset,
    FullWorldRebuild,
};

struct PlayEnterTraceState {
    bool pending = false;
    Clock::time_point startedAt{};
    RuntimePreviewDirtyState dirtyState = RuntimePreviewDirtyState::None;
    double rebuildMs = 0.0;
    double resetForPlayMs = 0.0;
    double rendererInitMs = 0.0;
    double rendererPrewarmMs = 0.0;
};

double elapsedMilliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

RuntimePreviewDirtyState mergeRuntimePreviewDirtyState(RuntimePreviewDirtyState current,
                                                       RuntimePreviewDirtyState incoming) {
    return static_cast<int>(incoming) > static_cast<int>(current) ? incoming : current;
}

const char* runtimePreviewDirtyStateLabel(RuntimePreviewDirtyState state) {
    switch (state) {
    case RuntimePreviewDirtyState::None:
        return "none";
    case RuntimePreviewDirtyState::EnvironmentOnly:
        return "environment";
    case RuntimePreviewDirtyState::GameplayStateReset:
        return "reset";
    case RuntimePreviewDirtyState::FullWorldRebuild:
        return "rebuild";
    }
    return "unknown";
}

const char* previewModeLabel(EditorPreviewMode mode) {
    const int index = std::clamp(static_cast<int>(mode), 0, 6);
    return kPreviewModes[index];
}

int previewModeDebugView(EditorPreviewMode mode) {
    switch (mode) {
    case EditorPreviewMode::Final:
        return 0;
    case EditorPreviewMode::LightingOnly:
        return 1;
    case EditorPreviewMode::SkyOnly:
        return 4;
    case EditorPreviewMode::SunDirect:
        return 5;
    case EditorPreviewMode::SunShadowVisibility:
        return 6;
    case EditorPreviewMode::CsmUvBounds:
        return 7;
    case EditorPreviewMode::CascadeIndex:
        return 8;
    }
    return 0;
}

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

void renderStartupProgress(Window& window,
                           ImGuiLayer& imgui,
                           float progress,
                           const char* stageTitle,
                           const char* stageDetail) {
    window.pollEvents();

    glViewport(0, 0, window.width(), window.height());
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.045f, 0.047f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    imgui.beginFrame();

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);
    ImGui::Begin("##StartupLoadingScreen",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration
                     | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoDocking
                     | ImGuiWindowFlags_NoNav
                     | ImGuiWindowFlags_NoBringToFrontOnFocus);

    const ImVec2 contentArea = ImGui::GetContentRegionAvail();
    const ImVec2 panelSize(520.0f, 140.0f);
    ImGui::SetCursorPos(ImVec2(std::max(0.0f, (contentArea.x - panelSize.x) * 0.5f),
                               std::max(0.0f, (contentArea.y - panelSize.y) * 0.5f)));

    ImGui::BeginChild("##StartupLoadingPanel",
                      panelSize,
                      true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextUnformatted("Level Editor");
    ImGui::Spacing();
    ImGui::TextUnformatted(stageTitle);
    ImGui::TextDisabled("%s", stageDetail);
    ImGui::Spacing();
    ImGui::ProgressBar(std::clamp(progress, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
    ImGui::EndChild();

    ImGui::End();

    imgui.endFrame();
    window.swapBuffers();
}

} // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);
#ifndef _WIN32
    std::signal(SIGUSR1, editorSignalHandler);
#endif

    std::string initialScene;
    if (argc > 1) {
        initialScene = argv[1];
    } else {
        const std::string cfgPath = resolveProjectPath("assets/project.cfg");
        const std::string cfgScene = readProjectCfgLastScene(cfgPath);
        if (!cfgScene.empty()) {
            const std::string resolved = resolveProjectPath("assets/scenes/" + cfgScene);
            if (std::filesystem::exists(resolved)) {
                initialScene = resolved;
            }
        }
    }

    const WindowGeometry savedGeo = loadWindowGeometry();
    Window window(savedGeo.width, savedGeo.height, "Level Editor");
    if (savedGeo.x >= 0 && savedGeo.y >= 0) {
        glfwSetWindowPos(window.handle(), savedGeo.x, savedGeo.y);
    }
    glfwSetWindowRefreshCallback(window.handle(), windowRefreshCallback);
    glfwSwapInterval(1);

    ImGuiLayer imgui;
    imgui.init(window.handle());
    const EditorUiPreferences uiPrefs = loadEditorUiPreferences(kEditorUiConfigFile);
    imgui.requestThemePreset(uiPrefs.themePreset);
    imgui.requestFontPreset(uiPrefs.fontPreset);
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    ContentRegistry content;
    renderStartupProgress(window, imgui, 0.05f, "Loading content registry", "Reading materials, archetypes, and definitions...");
    content.loadDefaults();
    registerDoorModule();
    registerCheckpointModule();
    registerPlayerControlModule();

    static engine::audio::AudioEngine audioEngine;
    audioEngine.init();

    GameSettings gameSettings;
    const std::string gameSettingsPath = resolveProjectPath("assets/game_settings.json");
    loadGameSettings(gameSettingsPath, gameSettings);
    audioEngine.setBusVolume("Master", gameSettings.audio.masterVolume);
    audioEngine.setBusVolume("SFX", gameSettings.audio.sfxVolume);
    audioEngine.setBusVolume("Music", gameSettings.audio.musicVolume);
    audioEngine.setBusVolume("Ambient", gameSettings.audio.ambienceVolume);
    audioEngine.setBusVolume("UI", gameSettings.audio.uiVolume);
    audioEngine.setReverbPreset(gameSettings.audio.reverbPreset);
    bool showGameSettings = false;

    MaterialTextureLibrary materialTextures;
    renderStartupProgress(window, imgui, 0.16f, "Preparing materials", "Uploading material texture data...");
    materialTextures.init(content);
    materialTextures.prewarmAllMaterialMaps();

    EditorSceneDocument document;
    if (!initialScene.empty()) {
        renderStartupProgress(window, imgui, 0.28f, "Opening scene", initialScene.c_str());
        document.loadFromSceneFile(initialScene, content);
    } else {
        renderStartupProgress(window, imgui, 0.28f, "Ready", "No scene loaded — create or open a scene");
        document.clear();
    }
    EditorCommandStack commandStack;
    commandStack.reset(document);

    EditorPreviewWorld previewWorld;
    if (!initialScene.empty()) {
        renderStartupProgress(window, imgui, 0.42f, "Building edit preview", "Creating preview meshes and helpers...");
        previewWorld.rebuild(document, content);
    }
    EditorRuntimePreviewSession runtimePreviewSession;
    runtimePreviewSession.setAudioEngine(&audioEngine);
    runtimePreviewSession.registry().ctx().insert_or_assign<GameSettings>(GameSettings(gameSettings));
    if (!initialScene.empty()) {
        renderStartupProgress(window, imgui, 0.56f, "Building play preview", "Creating the runtime preview session...");
        runtimePreviewSession.rebuild(document, content);
        renderStartupProgress(window, imgui, 0.70f, "Warming renderer", "Compiling shaders and preloading preview resources...");
        runtimePreviewSession.prewarmRenderer(content);
    }

    renderStartupProgress(window, imgui, 0.80f, "Creating render pipeline", "Preparing the editor renderer...");
    EditorViewportRenderer editorViewportRenderer;
    editorViewportRenderer.init();

    Framebuffer finalFbo;
    finalFbo.create(1280, 720);

    EditorCamera editCamera;
    EditorCameraAnimation cameraAnim;
    if (!initialScene.empty()) {
        if (!syncEditorCameraToRuntimeStart(document, editCamera) && previewWorld.sceneBounds().valid) {
            focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
        }
    }
    EditorUiState ui;
    applyEditorPreviewQuality(ui, ui.previewQuality);
    ui.pendingScenePath = initialScene; // empty if no scene loaded
    runtimePreviewSession.debugParams().lighting.shadowMapResolutionIndex = ui.shadowResolutionIndex;
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

    // Viewport dirty-flag tracking: skip re-render when camera/scene/UI unchanged
    struct ViewportDirtyState {
        glm::vec3 cameraPosition{0.0f};
        float cameraYaw = 0.0f;
        float cameraPitch = 0.0f;
        float cameraFov = 0.0f;
        std::uint64_t sceneRevision = 0;
        std::uint64_t environmentRevision = 0;
        int previewMode = -1;
        int viewportWidth = 0;
        int viewportHeight = 0;
        std::size_t selectionHash = 0;
        bool showColliders = false;
        bool showLightHelpers = false;
        bool showSpawnMarker = false;
        bool showTriggers = false;
        int shadowResolutionIndex = -1;
    };
    ViewportDirtyState prevViewportState;
    std::uint64_t persistedHoveredObjectId = 0;
    bool vsyncEnabled = true;

    renderStartupProgress(window, imgui, 0.92f, "Finalizing workspace", "Loading layouts, assets, and editor state...");

    if (!materialIds.empty()) {
        ui.selectedMaterialId = materialIds.front();
    }
    if (!meshIds.empty()) {
        ui.selectedMeshId = meshIds.front();
    }
    if (!archetypeIds.empty()) {
        ui.selectedArchetypeId = archetypeIds.front();
    }
    // On startup, restore only panel visibility from the active layout preset.
    // ImGui auto-loads dock node sizes (panel sizes) from imgui.ini at the first
    // NewFrame() call; calling LoadIniSettingsFromMemory here would overwrite those
    // user-customized sizes with the preset's stored sizes, discarding any
    // resizing the user did in previous sessions.
    if (std::find(layoutPresetNames.begin(), layoutPresetNames.end(), ui.activeLayoutPreset) != layoutPresetNames.end()) {
        try {
            const EditorLayoutPreset preset = loadEditorLayoutPreset(editorLayoutPresetPath(ui.activeLayoutPreset));
            applyLayoutVisibility(ui, preset.visibility);
            std::snprintf(ui.layoutNameBuffer, sizeof(ui.layoutNameBuffer), "%s", preset.name.c_str());
        } catch (const std::exception& ex) {
            spdlog::warn("Failed to load editor layout '{}': {}", ui.activeLayoutPreset, ex.what());
            dockLayoutResetRequested = true;
        }
    } else {
        dockLayoutResetRequested = true;
    }

    renderStartupProgress(window, imgui, 1.0f, "Ready", "Opening editor...");

    bool previewDirty = true;
    std::size_t previewObjectCount = document.objects().size();
    int startupViewportHandoffFramesRemaining = 3;
    bool savePressed = false;
    bool newScenePopupRequested = false;
    bool saveLayoutPopupRequested = false;
    bool packagePopupRequested = false;
    char newSceneNameBuffer[128] = "new_scene";
    char addMeshFilter[128] = {};
    std::string pendingDeleteScenePath;
    std::optional<std::string> pendingSceneSwitch;
    bool focusPressed = false;
    FocusToggleState focusToggle;
    bool resetStartPressed = false;
    bool duplicatePressed = false;
    bool deletePressed = false;
    bool undoPressed = false;
    bool redoPressed = false;
    bool playTogglePressed = false;

    EditorBuildState buildState;
    EditorBuildConfig buildConfig;
    BuildOutputLog buildLog;
    loadBuildConfig(buildConfig, resolveProjectPath(kBuildConfigFile));
    std::vector<std::string> allBuildableScenes = listBuildableScenes();
    bool buildPressed = false;
    bool buildAndRunPressed = false;
    bool packagePressed = false;
    std::string prePackageConfig;
    bool buildConfigurePhase = false;
    bool buildSaveModalPending = false;
    bool buildSaveModalRunAfter = false;
    std::uint64_t previewSceneRevision = document.sceneRevision();
    std::uint64_t previewEnvironmentRevision = document.environmentRevision();
    RuntimePreviewDirtyState runtimePreviewDirtyState = RuntimePreviewDirtyState::None;
    PlayEnterTraceState playEnterTrace;
    EditorPendingCommand widgetCommand;
    EditorPendingCommand gizmoCommand;
    MultiGizmoState multiGizmoState;
    std::vector<std::filesystem::path> pendingDroppedAssetPaths;
    ImGuiThemePreset editorThemePreset = uiPrefs.themePreset;
    ImGuiFontPreset editorFontPreset = uiPrefs.fontPreset;
    auto saveCurrentUiPreferences = [&]() {
        saveEditorUiPreferences(EditorUiPreferences{
                                    .fontPreset = editorFontPreset,
                                    .themePreset = editorThemePreset,
                                },
                                kEditorUiConfigFile);
    };
    // Declared here (rather than inside renderFrame) so DebugHarness can hold a reference
    // to it. The struct has default-initialized members and is populated each frame.
    EditorViewportState viewportState;

    DebugHarness debugHarness(document, selectedIds, ui, commandStack, window.handle(),
                              editCamera, cameraAnim, viewportState, previewWorld, &runtimePreviewSession);
    debugHarness.init();

    // Full-frame render lambda — called from the main loop and from
    // windowRefreshCallback during live resize on macOS.
    auto renderFrame = [&]() {
        {
            auto droppedPaths = window.takeDroppedPaths();
            pendingDroppedAssetPaths.insert(pendingDroppedAssetPaths.end(),
                                            std::make_move_iterator(droppedPaths.begin()),
                                            std::make_move_iterator(droppedPaths.end()));
            if (!pendingDroppedAssetPaths.empty()) {
                ui.showAssetBrowser = true;
            }
        }
        // Poll for .material file changes and hot-reload modified materials.
        // Runs in editor only (not in the runtime game). Cheap: timestamp check every 500ms.
        content.pollMaterialHotReload(materialTextures);
        debugHarness.poll();

        if (ui.playPreview && runtimePreviewSession.captured() && glfwGetWindowAttrib(window.handle(), GLFW_FOCUSED) == 0) {
            runtimePreviewSession.endCapture(window.handle());
        }
        imgui.beginFrame();
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        ImGuizmo::BeginFrame();

        const float deltaTime = 1.0f / std::max(ImGui::GetIO().Framerate, 1.0f);
        ImGuiIO& io = ImGui::GetIO();
        const bool gameplayPreviewCaptured = ui.playPreview && runtimePreviewSession.captured();

        if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_P)) {
            playTogglePressed = true;
        }
        if (ui.playPreviewToggleRequested) {
            playTogglePressed = true;
            ui.playPreviewToggleRequested = false;
        }

        if (!gameplayPreviewCaptured) {
            if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F)) focusPressed = true;
            if (!io.WantTextInput && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) deletePressed = true;
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_D)) duplicatePressed = true;
            if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_S)) savePressed = true;
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
            if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_B)) {
                buildPressed = true;
            }
            if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_R) &&
                glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
                buildAndRunPressed = true;
            }
            if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                placementState.clear();
                widgetCommand.clear();
                gizmoCommand.clear();
                const auto selectionBefore = selectedIds;
                selectedIds.clear();
                selectionPicker.clear();
                commandStack.pushSelectionCommand("Deselect All", &selectedIds, selectionBefore, selectedIds);
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
            }
        }

        std::optional<std::string> requestedScenePath;
        std::optional<std::string> requestedLayoutPresetName;

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
                     | ImGuiWindowFlags_NoDocking
                     | ImGuiWindowFlags_MenuBar);

        if (gameplayPreviewCaptured) {
            ImGui::BeginDisabled();
        }

        auto renderCreateCommands = [&]() {
            if (ImGui::MenuItem("Place Mesh")) {
                beginPlacement(placementState, EditorPlacementKind::Mesh, ui.selectedMeshId, ui.selectedMaterialId);
            }
            if (ImGui::MenuItem("Place Archetype")) {
                beginPlacement(placementState, EditorPlacementKind::Archetype, ui.selectedArchetypeId);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Add Point Light")) {
                beginPlacement(placementState, EditorPlacementKind::PointLight);
            }
            if (ImGui::MenuItem("Add Spot Light")) {
                beginPlacement(placementState, EditorPlacementKind::SpotLight);
            }
            if (ImGui::MenuItem("Add Directional Light")) {
                beginPlacement(placementState, EditorPlacementKind::DirectionalLight);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Place Collider")) {
                if (ImGui::MenuItem("Box")) {
                    beginPlacement(placementState, EditorPlacementKind::Collider, "box");
                }
                if (ImGui::MenuItem("Cylinder")) {
                    beginPlacement(placementState, EditorPlacementKind::Collider, "cylinder");
                }
                if (ImGui::MenuItem("Sphere")) {
                    beginPlacement(placementState, EditorPlacementKind::Collider, "sphere");
                }
                if (ImGui::MenuItem("Capsule")) {
                    beginPlacement(placementState, EditorPlacementKind::Collider, "capsule");
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Place Player Spawn")) {
                beginPlacement(placementState, EditorPlacementKind::PlayerSpawn);
            }
        };
        auto renderLayoutMenuItems = [&]() {
            for (const auto& layoutName : layoutPresetNames) {
                const bool selected = (layoutName == ui.activeLayoutPreset);
                if (ImGui::MenuItem(layoutName.c_str(), nullptr, selected)) {
                    requestedLayoutPresetName = layoutName;
                }
            }
        };

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene...")) {
                    newScenePopupRequested = true;
                    std::strncpy(newSceneNameBuffer, "new_scene", sizeof(newSceneNameBuffer));
                }
                if (ImGui::MenuItem("Save", "Ctrl/Cmd+S", false, document.dirty())) {
                    savePressed = true;
                }
                if (ImGui::BeginMenu("Open Scene")) {
                    for (const auto& scenePath : scenePaths) {
                        const bool selected = (scenePath == ui.pendingScenePath);
                        if (ImGui::MenuItem(scenePath.c_str(), nullptr, selected)) {
                            requestedScenePath = scenePath;
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl/Cmd+Z", false, commandStack.canUndo())) {
                    undoPressed = true;
                }
                if (ImGui::MenuItem("Redo", "Ctrl/Cmd+Shift+Z / Ctrl/Cmd+Y", false, commandStack.canRedo())) {
                    redoPressed = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate", "Ctrl/Cmd+D", false, !selectedIds.empty())) {
                    duplicatePressed = true;
                }
                if (ImGui::MenuItem("Delete", "Delete/Backspace", false, !selectedIds.empty())) {
                    deletePressed = true;
                }
                if (ImGui::MenuItem("Focus Selected", "F", false, selectedIds.size() == 1)) {
                    focusPressed = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Create")) {
                renderCreateCommands();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::BeginMenu("Preview Mode")) {
                    for (int index = 0; index < 7; ++index) {
                        const auto mode = static_cast<EditorPreviewMode>(index);
                        if (ImGui::MenuItem(kPreviewModes[index], nullptr, ui.previewMode == mode)) {
                            ui.previewMode = mode;
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Preview Quality")) {
                    for (int index = 0; index < 3; ++index) {
                        const auto quality = static_cast<EditorPreviewQuality>(index);
                        if (ImGui::MenuItem(kPreviewQualityLabels[index], nullptr, ui.previewQuality == quality)) {
                            applyEditorPreviewQuality(ui, quality);
                            runtimePreviewSession.debugParams().lighting.shadowMapResolutionIndex = ui.shadowResolutionIndex;
                        }
                    }
                    ImGui::Separator();
                    ImGui::TextDisabled("Play render scale: %.0f%%", ui.previewRenderScale * 100.0f);
                    ImGui::TextDisabled("Shadow resolution: %s", ui.shadowResolutionIndex == 0 ? "512" : (ui.shadowResolutionIndex == 1 ? "1024" : "2048"));
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Interface Theme")) {
                    static constexpr std::array<ImGuiThemePreset, 6> kThemePresets{
                        ImGuiThemePreset::WarmStudioDark,
                        ImGuiThemePreset::SpectrumInspiredDark,
                        ImGuiThemePreset::SpectrumCompact,
                        ImGuiThemePreset::GraphiteDark,
                        ImGuiThemePreset::GraphiteDense,
                        ImGuiThemePreset::SoftLightTooling,
                    };
                    for (ImGuiThemePreset preset : kThemePresets) {
                        const bool selected = (editorThemePreset == preset);
                        if (ImGui::MenuItem(imguiThemePresetLabel(preset), nullptr, selected)) {
                            editorThemePreset = preset;
                            imgui.requestThemePreset(preset);
                            saveCurrentUiPreferences();
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Interface Font")) {
                    static constexpr std::array<ImGuiFontPreset, 8> kFontPresets{
                        ImGuiFontPreset::SystemSans,
                        ImGuiFontPreset::InterUnity,
                        ImGuiFontPreset::RobotoUnreal,
                        ImGuiFontPreset::JetBrainsMonoGodot,
                        ImGuiFontPreset::Verdana,
                        ImGuiFontPreset::AvenirNext,
                        ImGuiFontPreset::HelveticaNeue,
                        ImGuiFontPreset::TrebuchetMS,
                    };
                    for (ImGuiFontPreset preset : kFontPresets) {
                        const bool selected = (editorFontPreset == preset);
                        if (ImGui::MenuItem(imguiFontPresetLabel(preset), nullptr, selected)) {
                            editorFontPreset = preset;
                            imgui.requestFontPreset(preset);
                            saveCurrentUiPreferences();
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Snapping")) {
                    ImGui::MenuItem("Enable Snapping", nullptr, &ui.snappingEnabled);
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::DragFloat("Move", &ui.moveSnap, 0.05f, 0.05f, 10.0f, "%.2f");
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::DragFloat("Rotate", &ui.rotateSnap, 1.0f, 1.0f, 90.0f, "%.1f");
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::DragFloat("Scale", &ui.scaleSnap, 0.01f, 0.01f, 2.0f, "%.2f");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Helpers")) {
                    ImGui::MenuItem("Show Colliders", nullptr, &ui.showColliders);
                    ImGui::MenuItem("Show Light Helpers", nullptr, &ui.showLightHelpers);
                    ImGui::MenuItem("Show Spawn Marker", nullptr, &ui.showSpawnMarker);
                    ImGui::Separator();
                    ImGui::MenuItem("Viewport Stats", nullptr, &ui.showViewportStats);
                    ImGui::MenuItem("Performance", nullptr, &ui.showPerformance);
                    ImGui::MenuItem("Camera Debug", nullptr, &ui.showCameraDebug);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            // Build menu — title shows progress during build (D-10)
            {
                std::string buildMenuLabel = "Build";
                if (buildState.running) {
                    char pctBuf[32];
                    std::snprintf(pctBuf, sizeof(pctBuf), "Build [%d%%]", static_cast<int>(buildState.progressPct));
                    buildMenuLabel = pctBuf;
                }
                if (ImGui::BeginMenu(buildMenuLabel.c_str())) {
                    // Build and Build and Run greyed out during build (D-16)
                    ImGui::BeginDisabled(buildState.running);
                    if (ImGui::MenuItem("Build", "Cmd+B")) {
                        buildPressed = true;
                    }
                    if (ImGui::MenuItem("Build and Run", "Cmd+R")) {
                        buildAndRunPressed = true;
                    }
                    ImGui::EndDisabled();
                    if (ImGui::MenuItem("Package for Sharing...")) {
                        packagePopupRequested = true;
                    }
                    if (ImGui::MenuItem("Open Build Folder")) {
                        auto absPath = resolveProjectPath(buildConfig.buildDir);
                        if (std::filesystem::exists(absPath)) {
                            openFolderInOS(absPath);
                        }
                    }
                    ImGui::Separator();
                    // Configuration submenu (D-04)
                    if (ImGui::BeginMenu("Configuration")) {
                        const char* configs[] = {"Debug", "Release", "RelWithDebInfo"};
                        for (const char* cfg : configs) {
                            const bool selected = (buildConfig.buildConfig == cfg);
                            if (ImGui::MenuItem(cfg, nullptr, selected)) {
                                if (buildConfig.buildConfig != cfg) {
                                    buildConfig.buildConfig = cfg;
                                    // Switching config triggers reconfigure (D-04)
                                    buildLog.clear();
                                    ui.showBuildOutput = true;
                                    startConfigure(buildState, buildConfig);
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
            }

            if (ImGui::BeginMenu("Window")) {
                if (ImGui::BeginMenu("Panels")) {
                    ImGui::MenuItem("Outliner", nullptr, &ui.showOutliner);
                    ImGui::MenuItem("Inspector", nullptr, &ui.showInspector);
                    ImGui::MenuItem("Asset Browser", nullptr, &ui.showAssetBrowser);
                    ImGui::MenuItem("Environment", nullptr, &ui.showEnvironment);
                    ImGui::MenuItem("Viewport", nullptr, &ui.showViewport);
                    ImGui::MenuItem("Build Output", nullptr, &ui.showBuildOutput);
                    ImGui::MenuItem("Game Settings", nullptr, &showGameSettings);
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Layouts")) {
                    renderLayoutMenuItems();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save Current Layout As...")) {
                        saveLayoutPopupRequested = true;
                    }
                    if (ImGui::MenuItem("Reload Current Layout")) {
                        requestedLayoutPresetName = ui.activeLayoutPreset;
                    }
                    if (ImGui::MenuItem("Reset Dock Layout")) {
                        dockLayoutResetRequested = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("Shortcuts and Controls")) {
                    ImGui::OpenPopup("Editor Shortcuts");
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        const float toolbarWidth = ImGui::GetContentRegionAvail().x;
        const bool collapseLayoutToMore = toolbarWidth < 900.0f;
        const bool collapseResetToMore = toolbarWidth < 760.0f;
        const float sceneComboWidth = toolbarWidth < 860.0f ? 170.0f : 220.0f;
        const float layoutComboWidth = toolbarWidth < 980.0f ? 130.0f : 150.0f;
        const std::string sceneToolbarLabel = std::filesystem::path(ui.pendingScenePath).filename().string().empty()
            ? ui.pendingScenePath
            : std::filesystem::path(ui.pendingScenePath).filename().string();

        ImGui::BeginDisabled(!document.dirty());
        if (ImGui::Button("Save")) {
            savePressed = true;
        }
        ImGui::EndDisabled();
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
        ImGui::SetNextItemWidth(sceneComboWidth);
        if (ImGui::BeginCombo("Scene", sceneToolbarLabel.c_str())) {
            for (const auto& scenePath : scenePaths) {
                const bool selected = (scenePath == ui.pendingScenePath);
                if (ImGui::Selectable(scenePath.c_str(), selected)) {
                    requestedScenePath = scenePath;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button(ui.playPreview ? "Stop Preview" : "Play Preview")) {
            playTogglePressed = true;
        }
        if (!collapseResetToMore) {
            ImGui::SameLine();
            if (ImGui::Button("Reset Start")) {
                resetStartPressed = true;
            }
        }
        if (collapseResetToMore || collapseLayoutToMore) {
            ImGui::SameLine();
            if (ImGui::Button("More")) {
                ImGui::OpenPopup("GlobalToolbarMore");
            }
            if (ImGui::BeginPopup("GlobalToolbarMore")) {
                if (collapseResetToMore && ImGui::MenuItem("Reset Start")) {
                    resetStartPressed = true;
                }
                if (collapseLayoutToMore && ImGui::BeginMenu("Layouts")) {
                    renderLayoutMenuItems();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save Current Layout As...")) {
                        saveLayoutPopupRequested = true;
                    }
                    if (ImGui::MenuItem("Reload Current Layout")) {
                        requestedLayoutPresetName = ui.activeLayoutPreset;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
        }

        const float currentCursorX = ImGui::GetCursorPosX();
        const float contentMaxX = ImGui::GetWindowContentRegionMax().x;
        const ImGuiStyle& style = ImGui::GetStyle();
        const char* layoutLabelText = "Layout";
        const float layoutLabelWidth = ImGui::CalcTextSize(layoutLabelText).x;
        const float layoutGroupWidth = collapseLayoutToMore ? 0.0f : (layoutLabelWidth + style.ItemInnerSpacing.x + layoutComboWidth);
        const float rightGroupWidth = layoutGroupWidth;
        const float targetX = std::max(currentCursorX, contentMaxX - rightGroupWidth);
        if (targetX > currentCursorX) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(targetX);
        }
        if (!collapseLayoutToMore) {
            ImGui::TextUnformatted(layoutLabelText);
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::SetNextItemWidth(layoutComboWidth);
            if (ImGui::BeginCombo("##layout_toolbar", ui.activeLayoutPreset.c_str())) {
                for (const auto& layoutName : layoutPresetNames) {
                    const bool selected = (layoutName == ui.activeLayoutPreset);
                    if (ImGui::Selectable(layoutName.c_str(), selected)) {
                        requestedLayoutPresetName = layoutName;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (saveLayoutPopupRequested) {
            ImGui::OpenPopup("Save Layout As");
            saveLayoutPopupRequested = false;
        }
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Save Layout As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::TextUnformatted("Save the current panel arrangement as a reusable layout preset.");
            ImGui::Separator();
            ImGui::InputText("Layout Name", ui.layoutNameBuffer, sizeof(ui.layoutNameBuffer));
            const bool hasLayoutName = std::strlen(ui.layoutNameBuffer) > 0;
            ImGui::BeginDisabled(!hasLayoutName);
            if (ImGui::Button("Save")) {
                try {
                    const std::string layoutName(ui.layoutNameBuffer);
                    if (saveLayoutPresetFromUi(ui, layoutName)) {
                        ui.activeLayoutPreset = std::filesystem::path(editorLayoutPresetPath(layoutName)).stem().string();
                        layoutPresetNames = listEditorLayoutPresetNames();
                    }
                    ImGui::CloseCurrentPopup();
                } catch (const std::exception& ex) {
                    spdlog::error("Layout save failed: {}", ex.what());
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Editor Shortcuts", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Editor Shortcuts");
            ImGui::Separator();
            ImGui::BulletText("Save: Ctrl/Cmd+S");
            ImGui::BulletText("Build: Cmd+B");
            ImGui::BulletText("Build and Run: Cmd+R");
            ImGui::BulletText("Play Preview: Ctrl/Cmd+P");
            ImGui::BulletText("Undo / Redo: Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z, Ctrl/Cmd+Y");
            ImGui::BulletText("Duplicate / Delete: Ctrl/Cmd+D, Delete");
            ImGui::BulletText("Focus Selected: F");
            ImGui::BulletText("Transform Tools: W / E / R");
            ImGui::BulletText("Cancel Placement or Release Preview Capture: Esc");
            ImGui::Separator();
            ImGui::TextUnformatted("Viewport Controls");
            ImGui::BulletText("Select: Left Click");
            ImGui::BulletText("Frame Selected: Double-click an object or press F");
            ImGui::BulletText("Orbit: Alt + Left Drag");
            ImGui::BulletText("Pan: Middle Drag");
            ImGui::BulletText("Dolly: Alt + Right Drag or mouse wheel");
            ImGui::BulletText("Fly: Right Mouse + WASD / QE");
            ImGui::Separator();
            ImGui::Text("Current UI Font: %s", imguiFontPresetLabel(editorFontPreset));
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

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
                                         kEnvironmentWindowName,
                                         kBuildOutputWindowName);
            dockLayoutResetRequested = false;
        }
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();

        if (requestedLayoutPresetName.has_value()) {
            try {
                loadLayoutPresetIntoUi(ui, *requestedLayoutPresetName);
            } catch (const std::exception& ex) {
                spdlog::error("Layout load failed: {}", ex.what());
            }
        }

        if (gameplayPreviewCaptured) {
            ImGui::BeginDisabled();
        }
        const std::vector<std::uint64_t> outlinerDeleteRequests = ui.viewportFullscreen
            ? std::vector<std::uint64_t>{}
            : renderOutliner(document, ui, selectedIds, &ui.showOutliner, commandStack);
        const InspectorActionResult inspectorActions = ui.viewportFullscreen
            ? InspectorActionResult{}
            : renderInspector(ui,
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
            runtimePreviewSession.prewarmRenderer(content);
            previewDirty = true;
        }
        if (inspectorActions.previewDirty) {
            previewDirty = true;
        }
        if (!ui.viewportFullscreen && renderEnvironmentPanel(document, content, environmentIds, &ui.showEnvironment, widgetCommand, commandStack)) {
            previewDirty = true;
        }
        if (ui.showPerformance) {
            renderPerformancePanel(
                runtimePreviewSession.performanceStats(),
                runtimePreviewSession.pipelineStats(),
                ui.playPreview,
                &ui.showPerformance);
        }
        if (ui.showCameraDebug && ui.playPreview) {
            renderCameraDebugPanel(runtimePreviewSession.cameraManager(),
                                   &ui.showCameraDebug);
        }
        if (showGameSettings) {
            if (renderGameSettingsPanel(gameSettings, &showGameSettings)) {
                saveGameSettings(gameSettingsPath, gameSettings);
                audioEngine.setBusVolume("Master", gameSettings.audio.masterVolume);
                audioEngine.setBusVolume("SFX", gameSettings.audio.sfxVolume);
                audioEngine.setBusVolume("Music", gameSettings.audio.musicVolume);
                audioEngine.setBusVolume("Ambient", gameSettings.audio.ambienceVolume);
                audioEngine.setBusVolume("UI", gameSettings.audio.uiVolume);
                audioEngine.setReverbPreset(gameSettings.audio.reverbPreset);
                runtimePreviewSession.registry().ctx().insert_or_assign<GameSettings>(GameSettings(gameSettings));
            }
        }
        const AssetBrowserActionResult assetBrowserActions = ui.viewportFullscreen
            ? AssetBrowserActionResult{}
            : renderAssetBrowser(ui,
                                 placementState,
                                 document,
                                 selectedIds,
                                 content,
                                 meshIds,
                                 materialIds,
                                 archetypeIds,
                                 pendingDroppedAssetPaths,
                                 &ui.showAssetBrowser,
                                 commandStack);
        if (gameplayPreviewCaptured) {
            ImGui::EndDisabled();
        }
        if (assetBrowserActions.openScenePath.has_value()) {
            requestedScenePath = *assetBrowserActions.openScenePath;
        }
        if (assetBrowserActions.previewDirty) {
            previewDirty = true;
        }
        if (assetBrowserActions.assetCatalogChanged) {
            previewWorld.reloadMeshAssets();
            meshIds = sortedMeshNames(previewWorld.meshLibrary());
            previewDirty = true;
        }
        if (assetBrowserActions.newMaterialId.has_value()) {
            const std::string newId = *assetBrowserActions.newMaterialId;
            const std::string matPath = resolveProjectPath("assets/materials/" + newId + ".material");
            try {
                MaterialDefinition def = loadMaterialDefinitionAsset(matPath);
                content.addMaterial(std::move(def), matPath);
                materialIds = sortedMaterialIds(content);
                materialTextures.init(content);
            } catch (const std::exception& ex) {
                spdlog::error("Failed to register new material '{}': {}", newId, ex.what());
            }
        }
        if (assetBrowserActions.deletedMaterialId.has_value()) {
            content.removeMaterial(*assetBrowserActions.deletedMaterialId);
            materialIds = sortedMaterialIds(content);
        }
        if (assetBrowserActions.renamedMaterialOldId.has_value() && assetBrowserActions.renamedMaterialNewId.has_value()) {
            const std::string oldId = *assetBrowserActions.renamedMaterialOldId;
            const std::string newId = *assetBrowserActions.renamedMaterialNewId;
            const std::string newPath = resolveProjectPath("assets/materials/" + newId + ".material");
            content.removeMaterial(oldId);
            try {
                MaterialDefinition def = loadMaterialDefinitionAsset(newPath);
                content.addMaterial(std::move(def), newPath);
                materialIds = sortedMaterialIds(content);
                materialTextures.init(content);
            } catch (const std::exception& ex) {
                spdlog::error("Failed to register renamed material '{}': {}", newId, ex.what());
            }
        }
        if (assetBrowserActions.consumedExternalDrops) {
            pendingDroppedAssetPaths.clear();
        }
        auto doLoadScene = [&](const std::string& scenePath) {
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
            const std::string cfgPath = resolveProjectPath("assets/project.cfg");
            const std::string filename = std::filesystem::path(scenePath).filename().string();
            writeProjectCfgLastScene(cfgPath, filename);
            previewEnvironmentRevision = document.environmentRevision();
            runtimePreviewDirtyState = RuntimePreviewDirtyState::FullWorldRebuild;
            if (ui.playPreview) {
                runtimePreviewSession.endCapture(window.handle());
            }
        };

        if (requestedScenePath.has_value()) {
            if (document.dirty()) {
                pendingSceneSwitch = requestedScenePath;
                ImGui::OpenPopup("UnsavedChangesOnSwitch");
            } else {
                doLoadScene(*requestedScenePath);
            }
        }

        // Unsaved changes guard modal (D-13)
        if (ImGui::BeginPopupModal("UnsavedChangesOnSwitch", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const std::string currentName = std::filesystem::path(ui.pendingScenePath).filename().string();
            ImGui::Text("Save changes to '%s' before opening another scene?", currentName.c_str());
            ImGui::Separator();
            if (ImGui::Button("Save", ImVec2(100, 0))) {
                document.save(content);
                if (pendingSceneSwitch.has_value()) {
                    doLoadScene(*pendingSceneSwitch);
                }
                pendingSceneSwitch.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
                if (pendingSceneSwitch.has_value()) {
                    doLoadScene(*pendingSceneSwitch);
                }
                pendingSceneSwitch.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                pendingSceneSwitch.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // New Scene popup (D-01, D-02, D-03, D-04)
        if (newScenePopupRequested || assetBrowserActions.newSceneRequested) {
            if (assetBrowserActions.newSceneRequested) {
                std::strncpy(newSceneNameBuffer, "new_scene", sizeof(newSceneNameBuffer));
            }
            ImGui::OpenPopup("NewScenePopup");
            newScenePopupRequested = false;
        }
        if (ImGui::BeginPopupModal("NewScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::Text("Create a new scene in assets/scenes/");
            ImGui::Separator();
            ImGui::SetNextItemWidth(250);
            const bool newSceneEnterPressed = ImGui::InputText("Name", newSceneNameBuffer, sizeof(newSceneNameBuffer),
                                                               ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            ImGui::TextDisabled(".scene");
            const bool hasNewSceneName = std::strlen(newSceneNameBuffer) > 0;
            ImGui::BeginDisabled(!hasNewSceneName);
            if (ImGui::Button("Create", ImVec2(80, 0)) || (newSceneEnterPressed && hasNewSceneName)) {
                std::string name(newSceneNameBuffer);
                std::string filename = name + ".scene";
                std::string fullPath = resolveProjectPath("assets/scenes/" + filename);
                int counter = 1;
                while (std::filesystem::exists(fullPath)) {
                    filename = name + "_" + std::to_string(counter) + ".scene";
                    fullPath = resolveProjectPath("assets/scenes/" + filename);
                    ++counter;
                }
                LevelDef newSceneDef;
                newSceneDef.environmentId = "default";
                newSceneDef.hasPlayerSpawn = true;
                saveLevelDef(fullPath, newSceneDef);
                scenePaths = sortedScenePaths();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Delete Scene confirmation modal (D-10, D-11)
        if (assetBrowserActions.deleteScenePath.has_value()) {
            pendingDeleteScenePath = *assetBrowserActions.deleteScenePath;
            ImGui::OpenPopup("DeleteSceneConfirm");
        }
        if (ImGui::BeginPopupModal("DeleteSceneConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const std::string deleteSceneName = std::filesystem::path(pendingDeleteScenePath).filename().string();
            ImGui::Text("Delete '%s'?", deleteSceneName.c_str());
            ImGui::TextWrapped("This cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Delete", ImVec2(80, 0))) {
                const std::string absDeletePath = resolveProjectPath(pendingDeleteScenePath);
                std::error_code deleteEc;
                std::filesystem::remove(absDeletePath, deleteEc);
                if (!deleteEc) {
                    if (pendingDeleteScenePath == ui.pendingScenePath) {
                        document.clear();
                        ui.pendingScenePath.clear();
                    }
                    const std::string cfgPath = resolveProjectPath("assets/project.cfg");
                    const std::string lastScene = readProjectCfgLastScene(cfgPath);
                    if (lastScene == deleteSceneName) {
                        writeProjectCfgLastScene(cfgPath, "");
                    }
                    scenePaths = sortedScenePaths();
                }
                pendingDeleteScenePath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                pendingDeleteScenePath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Package for Sharing popup — combined target, scene selection, and package trigger
        if (packagePopupRequested) {
            ImGui::OpenPopup("Package for Sharing");
            packagePopupRequested = false;
        }
        ImGui::SetNextWindowSize(ImVec2(400.0f, 380.0f), ImGuiCond_Appearing);
        {
            bool packagePopupOpen = true;
            if (ImGui::BeginPopupModal("Package for Sharing", &packagePopupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
                // --- Target platform ---
                ImGui::TextUnformatted("Target Platform");
                ImGui::Separator();
                ImGui::TextUnformatted("  macOS (.app)");
                ImGui::Spacing();

                // --- Scene selection ---
                ImGui::TextUnformatted("Scenes to Include");
                ImGui::Separator();

                for (const auto& scene : allBuildableScenes) {
                    bool included = std::find(buildConfig.buildScenes.begin(),
                                              buildConfig.buildScenes.end(), scene) !=
                                    buildConfig.buildScenes.end();
                    if (ImGui::Checkbox(scene.c_str(), &included)) {
                        if (included) {
                            buildConfig.buildScenes.push_back(scene);
                            std::sort(buildConfig.buildScenes.begin(),
                                      buildConfig.buildScenes.end());
                        } else {
                            auto it = std::find(buildConfig.buildScenes.begin(),
                                                buildConfig.buildScenes.end(), scene);
                            if (it != buildConfig.buildScenes.end()) {
                                buildConfig.buildScenes.erase(it);
                            }
                        }
                    }
                }

                ImGui::Separator();
                int selectedCount = static_cast<int>(buildConfig.buildScenes.size());
                int totalCount = static_cast<int>(allBuildableScenes.size());
                ImGui::Text("%d of %d scenes selected", selectedCount, totalCount);
                ImGui::Spacing();

                // --- Buttons ---
                bool noScenesSelected = buildConfig.buildScenes.empty();
                ImGui::BeginDisabled(noScenesSelected || buildState.running);
                if (ImGui::Button("Package", ImVec2(120, 0))) {
                    saveBuildConfig(buildConfig, resolveProjectPath(kBuildConfigFile));
                    packagePressed = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                if (noScenesSelected) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Select at least one scene");
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    saveBuildConfig(buildConfig, resolveProjectPath(kBuildConfigFile));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        // viewportState is declared outside renderFrame (before DebugHarness construction)
        // so the harness can hold a reference to it. Reset it at the start of each frame.
        viewportState = {};
        bool viewportWindowBegun = false;
        bool viewportWindowVisible = false;
        if (ui.showViewport) {
            viewportWindowBegun = true;
            viewportWindowVisible = beginCompactEditorPanelWindow("Viewport",
                                                                  &ui.showViewport,
                                                                  ImGuiWindowFlags_NoScrollbar
                                                                  | ImGuiWindowFlags_NoScrollWithMouse);
            if (viewportWindowVisible) {
                // Double-click on title bar toggles fullscreen viewport
                {
                    const ImVec2 winPos = ImGui::GetWindowPos();
                    const float titleBarHeight = ImGui::GetCurrentWindowRead()->TitleBarHeight;
                    const float winWidth = ImGui::GetWindowWidth();
                    const ImVec2 mousePos = io.MousePos;
                    if (ImGui::IsMouseDoubleClicked(0)
                        && mousePos.x >= winPos.x && mousePos.x <= winPos.x + winWidth
                        && mousePos.y >= winPos.y && mousePos.y <= winPos.y + titleBarHeight) {
                        ui.viewportFullscreen = !ui.viewportFullscreen;
                    }
                }
                viewportState.focused = ImGui::IsWindowFocused();
                if (!ui.playPreview && !ui.pendingScenePath.empty()) {
                    auto renderToolButton = [&](const char* label, EditorTransformTool tool) {
                        if (ui.tool == tool) {
                            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(72, 92, 124, 255));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(86, 108, 142, 255));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(62, 82, 112, 255));
                        }
                        const bool pressed = ImGui::Button(label);
                        if (ui.tool == tool) {
                            ImGui::PopStyleColor(3);
                        }
                        if (pressed) {
                            ui.tool = tool;
                        }
                    };

                    if (ImGui::Button("Add")) {
                        ImGui::OpenPopup("ViewportCreateMenu");
                    }
                    if (ImGui::BeginPopup("ViewportCreateMenu")) {
                        renderCreateCommands();
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add Mesh")) {
                        addMeshFilter[0] = '\0';
                        ImGui::OpenPopup("AddMeshPicker");
                    }
                    ImGui::SetNextWindowSize(ImVec2(300.0f, 400.0f), ImGuiCond_Appearing);
                    if (ImGui::BeginPopup("AddMeshPicker")) {
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetKeyboardFocusHere();
                        }
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::InputText("##addmesh_filter", addMeshFilter, sizeof(addMeshFilter));
                        ImGui::Separator();
                        const std::string filterStr(addMeshFilter);
                        if (ImGui::BeginChild("##addmesh_list", ImVec2(0.0f, 0.0f), false)) {
                            for (const auto& id : meshIds) {
                                if (!filterStr.empty()) {
                                    std::string lowerId = id;
                                    std::string lowerFilter = filterStr;
                                    std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
                                    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                                    if (lowerId.find(lowerFilter) == std::string::npos) {
                                        continue;
                                    }
                                }
                                if (ImGui::Selectable(id.c_str())) {
                                    ui.selectedMeshId = id;
                                    beginPlacement(placementState, EditorPlacementKind::Mesh, id, ui.selectedMaterialId);
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        }
                        ImGui::EndChild();
                        ImGui::EndPopup();
                    }
                    if (placementState.active()) {
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel Placement")) {
                            placementState.clear();
                        }
                    }
                    ImGui::SameLine();
                    renderToolButton("W", EditorTransformTool::Translate);
                    ImGui::SameLine();
                    renderToolButton("E", EditorTransformTool::Rotate);
                    ImGui::SameLine();
                    renderToolButton("R", EditorTransformTool::Scale);
                    ImGui::SameLine();
                    int previewModeIndex = static_cast<int>(ui.previewMode);
                    ImGui::SetNextItemWidth(110.0f);
                    if (ImGui::Combo("##viewport_previewmode", &previewModeIndex, kPreviewModes, 7)) {
                        ui.previewMode = static_cast<EditorPreviewMode>(previewModeIndex);
                    }
                    ImGui::SameLine();
                    int previewQualityIndex = static_cast<int>(ui.previewQuality);
                    ImGui::SetNextItemWidth(110.0f);
                    if (ImGui::Combo("##viewport_previewquality", &previewQualityIndex, kPreviewQualityLabels, 3)) {
                        applyEditorPreviewQuality(ui, static_cast<EditorPreviewQuality>(previewQualityIndex));
                        runtimePreviewSession.debugParams().lighting.shadowMapResolutionIndex = ui.shadowResolutionIndex;
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("Snap", &ui.snappingEnabled);
                    ImGui::SameLine();
                    if (ImGui::Button("Snap Settings")) {
                        ImGui::OpenPopup("ViewportSnapSettings");
                    }
                    if (ImGui::BeginPopup("ViewportSnapSettings")) {
                        ImGui::SetNextItemWidth(110.0f);
                        ImGui::DragFloat("Move", &ui.moveSnap, 0.05f, 0.05f, 10.0f, "%.2f");
                        ImGui::SetNextItemWidth(110.0f);
                        ImGui::DragFloat("Rotate", &ui.rotateSnap, 1.0f, 1.0f, 90.0f, "%.1f");
                        ImGui::SetNextItemWidth(110.0f);
                        ImGui::DragFloat("Scale", &ui.scaleSnap, 0.01f, 0.01f, 2.0f, "%.2f");
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Helpers")) {
                        ImGui::OpenPopup("ViewportHelpers");
                    }
                    if (ImGui::BeginPopup("ViewportHelpers")) {
                        ImGui::Checkbox("Show Colliders", &ui.showColliders);
                        ImGui::Checkbox("Show Light Helpers", &ui.showLightHelpers);
                        ImGui::Checkbox("Show Spawn Marker", &ui.showSpawnMarker);
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    ImGui::BeginDisabled(selectedIds.size() != 1);
                    if (ImGui::Button("Focus")) {
                        focusPressed = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                }
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                viewportState.size = ImVec2(std::max(avail.x, 64.0f), std::max(avail.y, 64.0f));
                viewportState.origin = ImGui::GetCursorScreenPos();
                viewportState.hovered = pointInViewport(viewportState, io.MousePos);

                // Empty state overlay: shown when no scene is loaded (D-08)
                if (ui.pendingScenePath.empty()) {
                    const ImVec2 windowSize = ImGui::GetContentRegionAvail();
                    ImGui::SetCursorPos(ImVec2((windowSize.x - 200.0f) * 0.5f, (windowSize.y - 60.0f) * 0.5f));
                    ImGui::TextUnformatted("No scene loaded");
                    ImGui::SetCursorPosX((windowSize.x - 200.0f) * 0.5f);
                    if (ImGui::Button("New Scene...", ImVec2(95, 0))) {
                        newScenePopupRequested = true;
                        std::strncpy(newSceneNameBuffer, "new_scene", sizeof(newSceneNameBuffer));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Open Scene...", ImVec2(95, 0))) {
                        ImGui::OpenPopup("OpenScenePickerPopup");
                    }
                    if (ImGui::BeginPopup("OpenScenePickerPopup")) {
                        for (const auto& sp : scenePaths) {
                            const std::string label = std::filesystem::path(sp).filename().string();
                            if (ImGui::MenuItem(label.c_str())) {
                                doLoadScene(sp);
                            }
                        }
                        if (scenePaths.empty()) {
                            ImGui::TextDisabled("No scenes found");
                        }
                        ImGui::EndPopup();
                    }
                }
            }
        }

        EditorViewportState renderViewportState = viewportState;
        if (ui.playPreview) {
            renderViewportState = fitViewportToAspect(viewportState, kRuntimeViewportAspect);
            renderViewportState.hovered = pointInViewport(renderViewportState, io.MousePos);
        }
        const bool startupViewportHandoffActive = startupViewportHandoffFramesRemaining > 0;

        const ImVec2 fbScale = io.DisplayFramebufferScale;
        const int targetW = std::max(1, static_cast<int>(std::max(renderViewportState.size.x, 64.0f) * fbScale.x));
        const int targetH = std::max(1, static_cast<int>(std::max(renderViewportState.size.y, 64.0f) * fbScale.y));
        const float previewRenderScale = ui.playPreview ? std::clamp(ui.previewRenderScale, 0.50f, 1.0f) : 1.0f;
        const int internalTargetW = std::max(1, static_cast<int>(std::lround(static_cast<float>(targetW) * previewRenderScale)));
        const int internalTargetH = std::max(1, static_cast<int>(std::lround(static_cast<float>(targetH) * previewRenderScale)));
        if (!startupViewportHandoffActive && (finalFbo.width() != targetW || finalFbo.height() != targetH)) {
            finalFbo.resize(targetW, targetH);
        }

        if (!ui.pendingScenePath.empty()) {
            if (previewDirty) {
                previewWorld.rebuild(document, content);
                previewDirty = false;
                previewSceneRevision = document.sceneRevision();
                previewObjectCount = document.objects().size();
                runtimePreviewDirtyState = RuntimePreviewDirtyState::FullWorldRebuild;
            } else if (previewSceneRevision != document.sceneRevision()) {
                if (document.objects().size() != previewObjectCount) {
                    // Build current document object-id set
                    std::unordered_set<std::uint64_t> docIds;
                    for (const auto& obj : document.objects()) { docIds.insert(obj.id); }

                    // Build preview world object-id set from ownerMap
                    std::unordered_set<std::uint64_t> worldIds;
                    for (const auto& [entity, oid] : previewWorld.ownerMap()) { worldIds.insert(oid); }

                    std::vector<std::uint64_t> toAdd, toRemove;
                    for (const auto id : docIds)   { if (!worldIds.count(id)) { toAdd.push_back(id); } }
                    for (const auto id : worldIds) { if (!docIds.count(id))   { toRemove.push_back(id); } }

                    if (!toRemove.empty()) { previewWorld.removeObjects(toRemove); }
                    if (!toAdd.empty())    { previewWorld.addObjects(toAdd, document, content); }

                    previewSceneRevision = document.sceneRevision();
                    previewObjectCount = document.objects().size();
                    runtimePreviewDirtyState = mergeRuntimePreviewDirtyState(runtimePreviewDirtyState,
                                                                             RuntimePreviewDirtyState::FullWorldRebuild);
                } else {
                    previewWorld.syncTransforms(document);
                    previewWorld.syncMaterials(document, content);
                    previewWorld.syncLights(document);
                    previewSceneRevision = document.sceneRevision();
                    runtimePreviewDirtyState = mergeRuntimePreviewDirtyState(runtimePreviewDirtyState,
                                                                             RuntimePreviewDirtyState::FullWorldRebuild);
                }
            }
            if (previewEnvironmentRevision != document.environmentRevision()) {
                previewEnvironmentRevision = document.environmentRevision();
                runtimePreviewDirtyState = mergeRuntimePreviewDirtyState(runtimePreviewDirtyState,
                                                                         RuntimePreviewDirtyState::EnvironmentOnly);
                runtimePreviewSession.syncEnvironment(document);
                if (runtimePreviewDirtyState == RuntimePreviewDirtyState::EnvironmentOnly) {
                    runtimePreviewDirtyState = RuntimePreviewDirtyState::None;
                }
            }
        }
        const auto selectionHandles = buildEditorSelectionHandles(document, previewWorld);
        const auto viewportSelectionHandles = buildViewportSelectionHandles(selectionHandles, ui);
        std::optional<glm::vec3> placementPoint;
        glm::mat4 view(1.0f);
        glm::mat4 projection(1.0f);
        glm::mat4 inverseViewProjection(1.0f);

        // Compute a simple hash of selectedIds for change detection
        const std::size_t selectionHash = [&]() {
            std::size_t h = selectedIds.size();
            for (auto id : selectedIds)
                h ^= std::hash<std::uint64_t>{}(id) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }();

        const bool viewportDirty = (editCamera.position != prevViewportState.cameraPosition)
            || (editCamera.yawDegrees != prevViewportState.cameraYaw)
            || (editCamera.pitchDegrees != prevViewportState.cameraPitch)
            || (editCamera.fovDegrees != prevViewportState.cameraFov)
            || (document.sceneRevision() != prevViewportState.sceneRevision)
            || (document.environmentRevision() != prevViewportState.environmentRevision)
            || (static_cast<int>(ui.previewMode) != prevViewportState.previewMode)
            || (targetW != prevViewportState.viewportWidth)
            || (targetH != prevViewportState.viewportHeight)
            || (selectionHash != prevViewportState.selectionHash)
            || (ui.showColliders != prevViewportState.showColliders)
            || (ui.showLightHelpers != prevViewportState.showLightHelpers)
            || (ui.showSpawnMarker != prevViewportState.showSpawnMarker)
            || (ui.showTriggers != prevViewportState.showTriggers)
            || (ui.shadowResolutionIndex != prevViewportState.shadowResolutionIndex)
            || cameraAnim.active
            || editorGizmoIsHot()
            || placementState.active()
            || (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType("EDITOR_PLACE"))
            || (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType("EDITOR_MATERIAL"))
            || (renderViewportState.hovered && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
            || (io.MouseWheel != 0.0f)
            || (glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

        // Always compute camera matrices so gizmos render correctly even when
        // the viewport is not dirty (cached FBO frame is reused).
        if (!ui.pendingScenePath.empty() && !startupViewportHandoffActive && !ui.playPreview) {
            tickCameraAnimation(editCamera, cameraAnim, deltaTime);
            if (!cameraAnim.active) {
                updateEditorFlyCamera(editCamera, window.handle(), renderViewportState, deltaTime);
            } else if (glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
                       || glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS
                       || (io.KeyAlt && glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                       || io.MouseWheel != 0.0f) {
                cameraAnim.active = false;
                updateEditorFlyCamera(editCamera, window.handle(), renderViewportState, deltaTime);
            }

            view = editorCameraView(editCamera);
            projection = editorCameraProjection(editCamera, static_cast<float>(targetW) / static_cast<float>(targetH));
            inverseViewProjection = glm::inverse(projection * view);
        }

        if (!ui.pendingScenePath.empty() && !startupViewportHandoffActive && !ui.playPreview
            && viewportDirty) {
            // Detect material drag for live preview
            MaterialDragPreview materialDragPreview;
            if (renderViewportState.hovered) {
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                    payload != nullptr && payload->IsDataType("EDITOR_MATERIAL") && payload->DataSize == sizeof(EditorMaterialDragPayload)) {
                    const auto& matPayload = *static_cast<const EditorMaterialDragPayload*>(payload->Data);
                    const EditorRay ray = buildEditorRay(
                        inverseViewProjection,
                        glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                        glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                        glm::vec2(io.MousePos.x, io.MousePos.y));
                    if (const auto hit = pickEditorObject(viewportSelectionHandles, ray)) {
                        if (hit->objectKind == EditorSceneObjectKind::Mesh) {
                            materialDragPreview.objectId = hit->objectId;
                            materialDragPreview.materialId = matPayload.materialId;
                        }
                    }
                }
            }

            std::vector<RenderObject> objects = collectRenderObjects(previewWorld, materialTextures, selectedIds, materialDragPreview);
            appendHelperObjects(objects, previewWorld, document, materialTextures, selectedIds,
                                ui.showColliders, ui.showLightHelpers, ui.showSpawnMarker, ui.showTriggers);
            appendSelectionOverlays(objects, previewWorld, materialTextures, selectedIds);

            // Per-frame hover highlight: blue-white wireframe on unselected objects under cursor
            std::uint64_t hoveredObjectId = 0;
            const bool suppressHover = gameplayPreviewCaptured
                || placementState.active()
                || editorGizmoIsHot()
                || glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS
                || glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS
                || (io.KeyAlt && glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

            const bool mouseMovedThisFrame = (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f);

            if (!suppressHover && renderViewportState.hovered && mouseMovedThisFrame) {
                const EditorRay hoverRay = buildEditorRay(
                    inverseViewProjection,
                    glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                    glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                    glm::vec2(io.MousePos.x, io.MousePos.y));
                if (const auto hit = pickEditorObject(viewportSelectionHandles, hoverRay)) {
                    hoveredObjectId = hit->objectId;
                }
                persistedHoveredObjectId = hoveredObjectId;
            } else if (!suppressHover && renderViewportState.hovered) {
                // Mouse didn't move — reuse previous hover result
                hoveredObjectId = persistedHoveredObjectId;
            } else {
                persistedHoveredObjectId = 0;
            }
            appendHoverOverlay(objects, previewWorld, materialTextures, hoveredObjectId, selectedIds);

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
            previewEnvironment.post.sunDirection = safeNormalize(previewEnvironment.lighting.sun.direction, previewEnvironment.post.sunDirection);
            previewEnvironment.post.sunColor = previewEnvironment.lighting.sun.color;
            previewEnvironment.post.debugViewMode = previewModeDebugView(ui.previewMode);

            std::vector<RenderLight> lights = collectLights(previewWorld.registry(), previewEnvironment);

            EditorViewportRenderParams renderParams;
            renderParams.viewMatrix = view;
            renderParams.projectionMatrix = projection;
            renderParams.cameraPosition = editCamera.position;
            renderParams.nearPlane = editCamera.nearPlane;
            renderParams.farPlane = editCamera.farPlane;
            renderParams.objects = &objects;
            renderParams.lights = &lights;
            renderParams.environment = &previewEnvironment;
            renderParams.shadowsEnabled = previewEnvironment.lighting.enableShadows;
            renderParams.shadowResolutionIndex = ui.shadowResolutionIndex;

            editorViewportRenderer.render(renderParams, targetW, targetH, targetW, targetH, finalFbo.framebuffer());

            prevViewportState.cameraPosition = editCamera.position;
            prevViewportState.cameraYaw = editCamera.yawDegrees;
            prevViewportState.cameraPitch = editCamera.pitchDegrees;
            prevViewportState.cameraFov = editCamera.fovDegrees;
            prevViewportState.sceneRevision = document.sceneRevision();
            prevViewportState.environmentRevision = document.environmentRevision();
            prevViewportState.previewMode = static_cast<int>(ui.previewMode);
            prevViewportState.viewportWidth = targetW;
            prevViewportState.viewportHeight = targetH;
            prevViewportState.selectionHash = selectionHash;
            prevViewportState.showColliders = ui.showColliders;
            prevViewportState.showLightHelpers = ui.showLightHelpers;
            prevViewportState.showSpawnMarker = ui.showSpawnMarker;
            prevViewportState.showTriggers = ui.showTriggers;
            prevViewportState.shadowResolutionIndex = ui.shadowResolutionIndex;
        } else if (!ui.pendingScenePath.empty() && !startupViewportHandoffActive && ui.playPreview) {
            runtimePreviewSession.debugParams().lighting.shadowMapResolutionIndex = ui.shadowResolutionIndex;
            runtimePreviewSession.debugParams().post.debugViewMode = previewModeDebugView(ui.previewMode);
            runtimePreviewSession.updateInput(window.handle(), io);
            runtimePreviewSession.tick(deltaTime, kRuntimeViewportAspect);
            runtimePreviewSession.syncCursor(window.handle());
            if (runtimePreviewSession.input().isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                runtimePreviewSession.endCapture(window.handle());
            }
            runtimePreviewSession.render(deltaTime,
                                         internalTargetW,
                                         internalTargetH,
                                         targetW,
                                         targetH,
                                         finalFbo.framebuffer());
            if (playEnterTrace.pending) {
                const RuntimeSessionPerformanceStats& stats = runtimePreviewSession.performanceStats();
                const double totalMs = elapsedMilliseconds(playEnterTrace.startedAt, Clock::now());
                spdlog::info(
                    "Play preview ready in {:.1f} ms (mode={}, rebuild={:.1f} ms, reset={:.1f} ms, renderer init={:.1f} ms, renderer prewarm={:.1f} ms, first render={:.1f} ms)",
                    totalMs,
                    runtimePreviewDirtyStateLabel(playEnterTrace.dirtyState),
                    playEnterTrace.rebuildMs,
                    playEnterTrace.resetForPlayMs,
                    playEnterTrace.rendererInitMs,
                    playEnterTrace.rendererPrewarmMs,
                    stats.lastRenderMs);
                playEnterTrace.pending = false;
            }
        }

        if (viewportWindowVisible && !ui.pendingScenePath.empty()) {
            if (ui.playPreview) {
                ImGui::SetCursorScreenPos(renderViewportState.origin);
            }
            if (startupViewportHandoffActive) {
                ImGui::SetCursorScreenPos(renderViewportState.origin);
                ImGui::Dummy(renderViewportState.size);

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 boxMin = renderViewportState.origin;
                const ImVec2 boxMax(renderViewportState.origin.x + renderViewportState.size.x,
                                    renderViewportState.origin.y + renderViewportState.size.y);
                drawList->AddRectFilled(boxMin, boxMax, IM_COL32(16, 18, 22, 255), 6.0f);
                drawList->AddRect(boxMin, boxMax, IM_COL32(56, 62, 74, 255), 6.0f);

                const char* title = "Opening workspace...";
                const char* detail = "Preparing the first editor frame";
                const ImVec2 titleSize = ImGui::CalcTextSize(title);
                const ImVec2 detailSize = ImGui::CalcTextSize(detail);
                const ImVec2 textPos(renderViewportState.origin.x + (renderViewportState.size.x - titleSize.x) * 0.5f,
                                     renderViewportState.origin.y + (renderViewportState.size.y - (titleSize.y + detailSize.y + 10.0f)) * 0.5f);
                drawList->AddText(textPos, IM_COL32(232, 236, 244, 255), title);
                drawList->AddText(ImVec2(textPos.x + (titleSize.x - detailSize.x) * 0.5f, textPos.y + titleSize.y + 10.0f),
                                  IM_COL32(168, 176, 192, 255),
                                  detail);
            } else {
                ImGui::SetNextItemAllowOverlap();
                ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(finalFbo.colorTexture())),
                             renderViewportState.size,
                             ImVec2(0.0f, 1.0f),
                             ImVec2(1.0f, 0.0f));
            }

            // Viewport performance stats overlay (bottom-left corner)
            if (ui.showViewportStats && !startupViewportHandoffActive) {
                ImDrawList* statsDraw = ImGui::GetWindowDrawList();
                const float frameMs = 1000.0f / std::max(ImGui::GetIO().Framerate, 1.0f);
                const float fps = ImGui::GetIO().Framerate;
                if (ui.playPreview) {
                    const auto& perf = runtimePreviewSession.performanceStats();
                    const auto& ps = runtimePreviewSession.pipelineStats();
                    char line1[128], line2[128], line3[128];
                    std::snprintf(line1, sizeof(line1),
                                  "%.1f ms  %.0f FPS  render: %.1f ms  scale: %.0f%%", frameMs, fps, perf.lastRenderMs, ui.previewRenderScale * 100.0f);
                    std::snprintf(line2, sizeof(line2),
                                  "shadow: %.1f  scene: %.1f  ssao: %.1f  bloom: %.1f  comp: %.1f ms",
                                  ps.shadowPassMs, ps.scenePassMs, ps.ssaoMs, ps.bloomMs, ps.compositeMs);
                    std::snprintf(line3, sizeof(line3),
                                  "objects: %d  draws: %d  culled: %d  shadow_culled: %d  lights: %d  %s",
                                  ps.objectCount, ps.drawCalls, ps.culledCount, ps.shadowCulledCount, ps.lightCount, editorPreviewQualityLabel(ui.previewQuality));
                    const float lineH = ImGui::GetTextLineHeight();
                    const ImVec2 statsPos(renderViewportState.origin.x + 8.0f,
                                          renderViewportState.origin.y + renderViewportState.size.y - 16.0f - lineH * 3.0f);
                    const float maxW = std::max({ImGui::CalcTextSize(line1).x,
                                                 ImGui::CalcTextSize(line2).x,
                                                 ImGui::CalcTextSize(line3).x});
                    statsDraw->AddRectFilled(
                        ImVec2(statsPos.x - 6.0f, statsPos.y - 4.0f),
                        ImVec2(statsPos.x + maxW + 6.0f, statsPos.y + lineH * 3.0f + 4.0f),
                        IM_COL32(0, 0, 0, 160), 3.0f);
                    statsDraw->AddText(statsPos, IM_COL32(220, 228, 240, 255), line1);
                    statsDraw->AddText(ImVec2(statsPos.x, statsPos.y + lineH), IM_COL32(180, 190, 200, 255), line2);
                    statsDraw->AddText(ImVec2(statsPos.x, statsPos.y + lineH * 2.0f), IM_COL32(180, 190, 200, 255), line3);
                } else {
                    const ImVec2 statsPos(renderViewportState.origin.x + 8.0f,
                                          renderViewportState.origin.y + renderViewportState.size.y - 52.0f);
                    char statsBuf[128];
                    std::snprintf(statsBuf, sizeof(statsBuf), "%.1f ms  %.0f FPS", frameMs, fps);
                    const ImVec2 textSize = ImGui::CalcTextSize(statsBuf);
                    statsDraw->AddRectFilled(
                        ImVec2(statsPos.x - 6.0f, statsPos.y - 4.0f),
                        ImVec2(statsPos.x + textSize.x + 6.0f, statsPos.y + textSize.y + 4.0f),
                        IM_COL32(0, 0, 0, 160), 3.0f);
                    statsDraw->AddText(statsPos, IM_COL32(220, 228, 240, 255), statsBuf);
                }
            }

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

            if (ui.playPreview && !startupViewportHandoffActive) {
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

                if (runtimePreviewDirtyState == RuntimePreviewDirtyState::FullWorldRebuild) {
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

            if (!ui.playPreview && !startupViewportHandoffActive && !selectedIds.empty()) {
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
            if (!ui.playPreview && !startupViewportHandoffActive) {
                applyGizmoToSelectedObject(document, selectedIds, renderViewportState, view, projection, ui, previewWorld, multiGizmoState);
                if (editorGizmoIsHot() && !gizmoCommand.active) {
                    beginPendingCommand(gizmoCommand, gizmoBeforeState, "Transform Object");
                } else if (gizmoCommand.active && !editorGizmoIsHot()) {
                    finalizePendingCommand(gizmoCommand, commandStack, document);
                }
            }

            if (!ui.playPreview && !startupViewportHandoffActive) {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_PLACE", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                        if (payload->Delivery && placementPoint.has_value() && payload->DataSize == sizeof(EditorDragPayload)) {
                            const EditorPlacementState droppedState = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
                            const EditorSceneDocumentState beforeState = document.captureState();
                            const auto placedId = commitPlacement(document, droppedState, *placementPoint, content, editCamera);
                            commandStack.pushDocumentStateCommand(
                                "Place Object",
                                beforeState,
                                document.captureState(),
                                document);
                            selectionPicker.clear();
                            if (placedId.has_value()) {
                                selectedIds = { *placedId };
                                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                                ui.scrollToSelection = true;
                            }
                            previewDirty = true;
                        }
                    }
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_MATERIAL")) {
                        if (payload->DataSize == sizeof(EditorMaterialDragPayload)) {
                            const auto& matPayload = *static_cast<const EditorMaterialDragPayload*>(payload->Data);
                            const std::string droppedMaterialId = matPayload.materialId;
                            const EditorRay ray = buildEditorRay(
                                inverseViewProjection,
                                glm::vec2(renderViewportState.origin.x, renderViewportState.origin.y),
                                glm::vec2(renderViewportState.size.x, renderViewportState.size.y),
                                glm::vec2(io.MousePos.x, io.MousePos.y));
                            if (const auto hit = pickEditorObject(viewportSelectionHandles, ray)) {
                                if (hit->objectKind == EditorSceneObjectKind::Mesh) {
                                    if (EditorSceneObject* object = document.findObject(hit->objectId)) {
                                        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
                                        if (mesh.materialId != droppedMaterialId) {
                                            const EditorSceneDocumentState beforeState = document.captureState();
                                            mesh.materialId = droppedMaterialId;
                                            document.markSceneDirty();
                                            commandStack.pushDocumentStateCommand(
                                                "Assign Material",
                                                beforeState,
                                                document.captureState(),
                                                document);
                                            previewDirty = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                const bool orbitModifierActive = io.KeyAlt;
                if (placementState.active()
                    && placementPoint.has_value()
                    && renderViewportState.hovered
                    && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                    && !orbitModifierActive
                    && !editorGizmoIsHot()) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    const auto placedId = commitPlacement(document, placementState, *placementPoint, content, editCamera);
                    commandStack.pushDocumentStateCommand(
                        "Place Object",
                        beforeState,
                        document.captureState(),
                        document);
                    placementState.clear();
                    selectionPicker.clear();
                    if (placedId.has_value()) {
                        selectedIds = { *placedId };
                        ui.inspectorContext = EditorInspectorContext::SceneSelection;
                        ui.scrollToSelection = true;
                    }
                    previewDirty = true;
                } else if (renderViewportState.hovered
                           && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                           && !orbitModifierActive
                           && !editorGizmoIsHot()) {
                    const auto selectionBefore = selectedIds;
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
                        applySelectionHit(selectedIds, selectionPicker, io.KeyShift, document);
                        ui.inspectorContext = EditorInspectorContext::SceneSelection;
                        ui.scrollToSelection = true;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            ui.frameSelectionRequested = true;
                        }
                    } else {
                        selectionPicker.clear();
                        if (!io.KeyShift) {
                            selectedIds.clear();
                        }
                        ui.inspectorContext = EditorInspectorContext::SceneSelection;
                    }
                    commandStack.pushSelectionCommand("Select", &selectedIds, selectionBefore, selectedIds);
                }
            }

            // Selection picker overlay removed — hover highlight replaces it
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
                GameOverlays::renderInventory(menu, runtimePreviewSession.runSession(), content, equipment);
            }
            if (previewRegistry.ctx().contains<InteractionPromptState>()) {
                const auto& prompt = previewRegistry.ctx().get<InteractionPromptState>();
                if (prompt.visible && !inventoryOpen) {
                    GameOverlays::renderInteractionPrompt(prompt.text.c_str(), prompt.busy);
                }
            }
        }

        if (playTogglePressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            ui.playPreview = !ui.playPreview;
            if (ui.playPreview) {
                runtimePreviewSession.endCapture(window.handle());
                const RuntimePreviewDirtyState requestedDirtyState = runtimePreviewDirtyState;
                if (runtimePreviewDirtyState == RuntimePreviewDirtyState::FullWorldRebuild) {
                    runtimePreviewSession.rebuild(document, content);
                    runtimePreviewDirtyState = RuntimePreviewDirtyState::None;
                } else if (runtimePreviewDirtyState == RuntimePreviewDirtyState::GameplayStateReset) {
                    runtimePreviewSession.resetForPlay();
                    runtimePreviewDirtyState = RuntimePreviewDirtyState::None;
                } else if (runtimePreviewDirtyState == RuntimePreviewDirtyState::EnvironmentOnly) {
                    runtimePreviewSession.syncEnvironment(document);
                    runtimePreviewDirtyState = RuntimePreviewDirtyState::None;
                }
                const RuntimeSessionPerformanceStats& stats = runtimePreviewSession.performanceStats();
                playEnterTrace.pending = true;
                playEnterTrace.startedAt = Clock::now();
                playEnterTrace.dirtyState = requestedDirtyState;
                playEnterTrace.rebuildMs = requestedDirtyState == RuntimePreviewDirtyState::FullWorldRebuild
                    ? stats.rebuildMs
                    : 0.0;
                playEnterTrace.resetForPlayMs = requestedDirtyState == RuntimePreviewDirtyState::GameplayStateReset
                    ? stats.resetForPlayMs
                    : 0.0;
                playEnterTrace.rendererInitMs = stats.rendererInitMs;
                playEnterTrace.rendererPrewarmMs = stats.rendererPrewarmMs;
                if (ui.showViewport) {
                    runtimePreviewSession.beginCapture(window.handle());
                }
            } else {
                runtimePreviewSession.endCapture(window.handle());
                runtimePreviewDirtyState = mergeRuntimePreviewDirtyState(runtimePreviewDirtyState,
                                                                        RuntimePreviewDirtyState::GameplayStateReset);
            }
            playTogglePressed = false;
        }

        if (resetStartPressed) {
            if (ui.playPreview) {
                runtimePreviewSession.endCapture(window.handle());
                runtimePreviewSession.rebuild(document, content);
                runtimePreviewSession.prewarmRenderer(content);
                runtimePreviewDirtyState = RuntimePreviewDirtyState::None;
            } else if (!syncEditorCameraToRuntimeStart(document, editCamera) && previewWorld.sceneBounds().valid) {
                focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
            }
            resetStartPressed = false;
        }

        if (ui.frameSelectionRequested) {
            focusPressed = true;
            ui.frameSelectionRequested = false;
        }

        if (focusPressed && !selectedIds.empty()) {
            // Reset toggle if selection changed since last focus
            if (focusToggle.lastFocusedSelection != selectedIds) {
                focusToggle.wasFocused = false;
                focusToggle.lastFocusedSelection = selectedIds;
            }

            EditorObjectBounds unionBounds;
            for (const auto id : selectedIds) {
                if (const EditorObjectBounds* b = previewWorld.findObjectBounds(id)) {
                    unionBounds.expand(*b);
                } else if (const EditorSceneObject* obj = document.findObject(id)) {
                    unionBounds.expand(editorSceneObjectAnchor(*obj));
                }
            }
            if (unionBounds.valid) {
                if (focusToggle.wasFocused) {
                    // Second F: tight zoom — 1-unit cube around center
                    const glm::vec3 center = unionBounds.center();
                    const glm::vec3 halfUnit(0.5f);
                    beginFocusAnimation(editCamera, cameraAnim,
                                        center - halfUnit, center + halfUnit);
                } else {
                    // First F: full bounds
                    beginFocusAnimation(editCamera, cameraAnim,
                                        unionBounds.min, unionBounds.max);
                }
                focusToggle.wasFocused = !focusToggle.wasFocused;
            }
            focusPressed = false;
        } else {
            focusPressed = false;
        }

        if (undoPressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            const std::size_t objectCountBefore = document.objects().size();
            if (commandStack.undo(document)) {
                pruneSelection(document, selectedIds);
                selectionPicker.clear();
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                if (document.objects().size() != objectCountBefore) {
                    previewDirty = true;
                }
            }
            undoPressed = false;
        }

        if (redoPressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            const std::size_t objectCountBefore = document.objects().size();
            if (commandStack.redo(document)) {
                pruneSelection(document, selectedIds);
                selectionPicker.clear();
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
                if (document.objects().size() != objectCountBefore) {
                    previewDirty = true;
                }
            }
            redoPressed = false;
        }

        if (duplicatePressed) {
            const EditorSceneDocumentState beforeState = document.captureState();
            std::vector<std::uint64_t> duplicated;
            for (auto id : selectedIds) {
                const std::uint64_t newId = document.duplicateObject(id);
                if (newId != 0) {
                    const glm::mat4 currentWorld = document.worldTransformMatrix(newId);
                    const glm::mat4 offsetWorld = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 0.0f)) * currentWorld;
                    document.applyWorldTransform(newId, offsetWorld);
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
                commandStack.markSaved(document);
                scenePaths = sortedScenePaths();
            } catch (const std::exception& ex) {
                spdlog::error("Save failed: {}", ex.what());
            }
            savePressed = false;
        }

        // Update window title with dirty indicator
        {
            const std::string sceneFile = std::filesystem::path(ui.pendingScenePath).filename().string();
            const std::string title = document.dirty()
                ? "Level Editor — " + sceneFile + " *"
                : "Level Editor — " + sceneFile;
            glfwSetWindowTitle(window.handle(), title.c_str());
        }

        // Build trigger lambda — clears log, shows panel, starts configure or build
        auto triggerBuild = [&](bool runAfter) {
            buildLog.clear();
            ui.showBuildOutput = true;
            buildState.runAfterBuild = runAfter;
            buildState.currentScenePath = ui.pendingScenePath;
            buildState.exitCode = -1;
            if (needsConfigure(buildConfig)) {
                buildConfigurePhase = true;
                startConfigure(buildState, buildConfig);
            } else {
                startBuild(buildState, buildConfig, "pixel-roguelike");
            }
        };

        // Handle build and build-and-run requests (check unsaved changes first)
        if ((buildPressed || buildAndRunPressed) && !buildState.running) {
            const bool runAfter = buildAndRunPressed;
            if (document.dirty()) {
                buildSaveModalPending = true;
                buildSaveModalRunAfter = runAfter;
            } else {
                triggerBuild(runAfter);
            }
            buildPressed = false;
            buildAndRunPressed = false;
        }

        // Handle Package for Sharing request
        if (packagePressed && !buildState.running) {
            prePackageConfig = buildConfig.buildConfig;
            buildConfig.buildConfig = "Release";
            buildState.packageAfterBuild = true;
            if (document.dirty()) {
                buildSaveModalPending = true;
                buildSaveModalRunAfter = false;
            } else {
                triggerBuild(false);
            }
            packagePressed = false;
        }

        // Build Output panel
        if (ui.showBuildOutput && !ui.viewportFullscreen) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
            if (beginCompactEditorPanelWindow(kBuildOutputWindowName, &ui.showBuildOutput)) {
                // Stop Build button in header (D-14)
                if (buildState.running) {
                    if (ImGui::Button("Stop Build")) {
                        cancelBuild(buildState);
                    }
                    ImGui::SameLine();
                    ImGui::Text("Building... %d%%", static_cast<int>(buildState.progressPct));
                } else if (buildState.exitCode == 0 && buildLog.lineCount() > 1) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Build Succeeded");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Open Folder")) {
                        auto absPath = resolveProjectPath(buildConfig.buildDir);
                        if (std::filesystem::exists(absPath)) {
                            openFolderInOS(absPath);
                        }
                    }
                } else if (buildState.exitCode > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Build Failed (exit code %d)", buildState.exitCode);
                }
                ImGui::Separator();

                // Scrollable log area with per-line coloring (D-08/D-09)
                ImGui::BeginChild("BuildLogScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

                const char* bufBegin = buildLog.buf.begin();
                const char* bufEnd = buildLog.buf.end();
                ImGuiListClipper clipper;
                clipper.Begin(buildLog.lineOffsets.Size);
                while (clipper.Step()) {
                    for (int lineNo = clipper.DisplayStart; lineNo < clipper.DisplayEnd; lineNo++) {
                        const char* lineStart = bufBegin + buildLog.lineOffsets[lineNo];
                        const char* lineEnd = (lineNo + 1 < buildLog.lineOffsets.Size)
                            ? (bufBegin + buildLog.lineOffsets[lineNo + 1] - 1) : bufEnd;
                        if (lineNo < buildLog.lineKinds.Size) {
                            BuildLineKind kind = buildLog.lineKinds[lineNo];
                            if (kind == BuildLineKind::Error) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                            } else if (kind == BuildLineKind::Warning) {
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
                            }
                        }
                        ImGui::TextUnformatted(lineStart, lineEnd);
                        if (lineNo < buildLog.lineKinds.Size) {
                            BuildLineKind kind = buildLog.lineKinds[lineNo];
                            if (kind == BuildLineKind::Error || kind == BuildLineKind::Warning) {
                                ImGui::PopStyleColor();
                            }
                        }
                    }
                }
                clipper.End();

                // Auto-scroll (D-12)
                if (buildLog.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
                // Jump to first error (D-12)
                if (buildLog.scrollToError && buildLog.firstErrorLine >= 0) {
                    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(static_cast<float>(buildLog.firstErrorLine) * lineHeight);
                    buildLog.scrollToError = false;
                }

                ImGui::EndChild();
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        // Poll build process each frame
        if (buildState.running) {
            pollBuild(buildState, buildLog);
        }

        // Handle configure-then-build chaining (D-03)
        if (!buildState.running && buildConfigurePhase) {
            if (buildState.exitCode == 0) {
                buildConfigurePhase = false;
                startBuild(buildState, buildConfig, "pixel-roguelike");
            } else {
                buildConfigurePhase = false; // configure failed, don't build
                // Clean up packaging state on configure failure
                if (buildState.packageAfterBuild) {
                    buildState.packageAfterBuild = false;
                    if (!prePackageConfig.empty()) {
                        buildConfig.buildConfig = prePackageConfig;
                        prePackageConfig.clear();
                    }
                }
            }
        }

        // Handle build completion: launch game if Build and Run succeeded (D-17)
        if (!buildState.running && !buildConfigurePhase && buildState.exitCode == 0 && buildState.runAfterBuild) {
            buildState.runAfterBuild = false;
            launchGame(buildConfig, buildState.currentScenePath);
        }

        // Handle build completion: package game if Package for Sharing succeeded
        if (!buildState.running && !buildConfigurePhase && buildState.packageAfterBuild) {
            buildState.packageAfterBuild = false;
            if (buildState.exitCode == 0) {
                std::string packageDir = resolveProjectPath(".") + "/build-package";
                bool ok = packageGame(buildConfig, packageDir, buildLog);
                if (ok) {
                    buildLog.addLine("", BuildLineKind::Normal);
                    buildLog.addLine("=== Package created: build-package/PixelRoguelike.app ===",
                                     BuildLineKind::Normal);
                    openFolderInOS(packageDir);
                }
            }
            // Restore original build config whether build succeeded or failed
            if (!prePackageConfig.empty()) {
                buildConfig.buildConfig = prePackageConfig;
                prePackageConfig.clear();
            }
        }

        // Unsaved changes modal (D-15)
        if (buildSaveModalPending) {
            ImGui::OpenPopup("Save Before Building?");
            buildSaveModalPending = false;
        }
        if (ImGui::BeginPopupModal("Save Before Building?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("The scene has unsaved changes.");
            ImGui::TextUnformatted("Save before building?");
            ImGui::Separator();
            if (ImGui::Button("Save")) {
                savePressed = true;
                triggerBuild(buildSaveModalRunAfter);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save")) {
                triggerBuild(buildSaveModalRunAfter);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                // Clean up packaging state if user cancels
                if (buildState.packageAfterBuild) {
                    buildState.packageAfterBuild = false;
                    if (!prePackageConfig.empty()) {
                        buildConfig.buildConfig = prePackageConfig;
                        prePackageConfig.clear();
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        imgui.endFrame();
        std::string requestedScreenshotPath = debugHarness.consumePendingScreenshotPath();
        if (requestedScreenshotPath.empty() && g_editorScreenshotRequested) {
            g_editorScreenshotRequested = 0;
            requestedScreenshotPath = "/tmp/editor_screenshot.png";
        }
        if (!requestedScreenshotPath.empty()) {
            const std::filesystem::path outputPath(requestedScreenshotPath);
            if (outputPath.has_parent_path()) {
                std::error_code ec;
                std::filesystem::create_directories(outputPath.parent_path(), ec);
            }
            // Read from viewport FBO, not window framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, finalFbo.framebuffer());
            saveScreenshot(finalFbo.width(), finalFbo.height(), requestedScreenshotPath);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        window.swapBuffers();
        if (startupViewportHandoffFramesRemaining > 0) {
            --startupViewportHandoffFramesRemaining;
        }
    };

    std::function<void()> renderFrameFn = renderFrame;
    g_editorRenderFrame = &renderFrameFn;

    while (!window.shouldClose()) {
        const bool needsContinuousRendering = (ui.playPreview && runtimePreviewSession.captured())
            || cameraAnim.active;

        if (needsContinuousRendering) {
            if (!vsyncEnabled) {
                glfwSwapInterval(1);
                vsyncEnabled = true;
            }
            glfwPollEvents();
        } else if (glfwGetWindowAttrib(window.handle(), GLFW_FOCUSED) == 0) {
            if (vsyncEnabled) {
                glfwSwapInterval(0);
                vsyncEnabled = false;
            }
            glfwWaitEventsTimeout(kUnfocusedTimeoutSeconds);
        } else {
            if (vsyncEnabled) {
                glfwSwapInterval(0);
                vsyncEnabled = false;
            }
            glfwWaitEventsTimeout(kIdleTimeoutSeconds);
        }

        renderFrame();
    }

    g_editorRenderFrame = nullptr;
    saveWindowGeometry(window.handle());
    saveCurrentUiPreferences();
    saveBuildConfig(buildConfig, resolveProjectPath(kBuildConfigFile));
    // Explicitly save ImGui state before shutdown so panel sizes are always
    // persisted, even if the user closes within the periodic auto-save window.
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    runtimePreviewSession.endCapture(window.handle());
    debugHarness.shutdown();
    imgui.shutdown();
    return 0;
}
