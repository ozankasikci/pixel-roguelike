#pragma once

#include "engine/rendering/RenderQualitySettings.h"
#include "engine/rendering/CameraDebugInfo.h"
#include "engine/rendering/post/PostProcessParams.h"
#include "game/rendering/RuntimeLightingOverride.h"
#include <optional>
#include <string>
#include <vector>

// Forward declarations
struct GLFWwindow;

enum class ImGuiFontPreset {
    SystemSans,
    Verdana,
    AvenirNext,
    HelveticaNeue,
    TrebuchetMS,
    InterUnity,
    RobotoUnreal,
    JetBrainsMonoGodot,
};

const char* imguiFontPresetLabel(ImGuiFontPreset preset);

enum class ImGuiThemePreset {
    WarmStudioDark,
    SpectrumInspiredDark,
    SpectrumCompact,
    GraphiteDark,
    GraphiteDense,
    SoftLightTooling,
};

const char* imguiThemePresetLabel(ImGuiThemePreset preset);

struct CameraDebugControl {
    // Shake
    float shakeTrauma = 0.5f;
    bool triggerShake = false;

    // FOV Punch
    float fovDelta = 8.0f;
    float fovDuration = 0.4f;
    bool triggerFOV = false;

    // Transition
    float targetPosition[3] = {0.0f, 2.0f, 0.0f};
    float targetYaw = -90.0f;
    float targetPitch = 0.0f;
    float transitionDuration = 1.5f;
    bool triggerTransition = false;
    bool triggerReturn = false;

    // Read-only state (populated by game code)
    float currentTrauma = 0.0f;
    bool isTransitioning = false;
};

struct DebugParams {
    // Post-processing parameters
    PostProcessParams post;

    CameraDebugInfo camera;               // was cameraPos/Dir/Fov/Speed
    CameraDebugControl cameraControl;
    RuntimeLightingOverride lighting;     // was shadow*/hemisphere*/directional*

    int   internalResIndex  = kDefaultInternalResolutionIndex;
    bool  resolutionChanged = false;

    // Performance
    float fps         = 0.0f;
    float frameTimeMs = 0.0f;
    int   drawCalls   = 0;
};

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void init(GLFWwindow* window);
    void shutdown();

    void requestThemePreset(ImGuiThemePreset preset);
    ImGuiThemePreset themePreset() const { return themePreset_; }
    void requestFontPreset(ImGuiFontPreset preset);
    ImGuiFontPreset fontPreset() const { return fontPreset_; }

    void beginFrame();
    void endFrame();

    static void renderOverlay(DebugParams& params, std::vector<RenderLight>& lights);

private:
    void applyPendingThemePreset();
    void applyPendingFontPreset();

    bool initialized_ = false;
    ImGuiThemePreset themePreset_ = ImGuiThemePreset::GraphiteDense;
    std::optional<ImGuiThemePreset> pendingThemePreset_ = ImGuiThemePreset::GraphiteDense;
    ImGuiFontPreset fontPreset_ = ImGuiFontPreset::SystemSans;
    std::optional<ImGuiFontPreset> pendingFontPreset_ = ImGuiFontPreset::SystemSans;
    std::string activeFontPath_;
};
