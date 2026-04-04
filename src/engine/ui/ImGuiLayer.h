#pragma once

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
    SoftLightTooling,
};

const char* imguiThemePresetLabel(ImGuiThemePreset preset);

struct DebugParams {
    // Post-processing parameters
    PostProcessParams post;

    CameraDebugInfo camera;               // was cameraPos/Dir/Fov/Speed
    RuntimeLightingOverride lighting;     // was shadow*/hemisphere*/directional*

    int   internalResIndex  = 2;      // 0=480p, 1=540p, 2=720p
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
    ImGuiThemePreset themePreset_ = ImGuiThemePreset::WarmStudioDark;
    std::optional<ImGuiThemePreset> pendingThemePreset_ = ImGuiThemePreset::WarmStudioDark;
    ImGuiFontPreset fontPreset_ = ImGuiFontPreset::SystemSans;
    std::optional<ImGuiFontPreset> pendingFontPreset_ = ImGuiFontPreset::SystemSans;
    std::string activeFontPath_;
};
