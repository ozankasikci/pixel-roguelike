#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>
#include "engine/rendering/post/PostProcessParams.h"

// Forward declarations
struct GLFWwindow;
struct PlayerMovementComponent;
struct ViewmodelComponent;
class ContentRegistry;
struct EffectiveEquipmentView;
struct InventoryMenuState;
struct RunSession;

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

struct DebugParams {
    // Post-processing parameters
    PostProcessParams post;

    int   internalResIndex  = 2;      // 0=480p, 1=540p, 2=720p
    bool  resolutionChanged = false;

    // Camera info
    glm::vec3 cameraPos = glm::vec3(0.0f);
    glm::vec3 cameraDir = glm::vec3(0.0f);
    float cameraFov   = 70.0f;
    float cameraSpeed = 3.0f;

    // Performance
    float fps         = 0.0f;
    float frameTimeMs = 0.0f;
    int   drawCalls   = 0;

    // Lighting
    bool shadowsEnabled = true;
    int shadowMapResolutionIndex = 1; // 0=512, 1=1024, 2=2048
    float shadowBias = 0.0018f;
    float shadowNormalBias = 0.03f;
    glm::vec3 hemisphereSkyColor{0.17f, 0.18f, 0.20f};
    glm::vec3 hemisphereGroundColor{0.06f, 0.05f, 0.045f};
    float hemisphereStrength = 0.32f;
    bool enableDirectionalLights = true;
    DirectionalLightSlot sunDirectional{
        true,
        glm::vec3(0.24f, 0.88f, 0.26f),
        glm::vec3(0.98f, 0.96f, 0.94f),
        1.0f
    };
    DirectionalLightSlot fillDirectional{
        false,
        glm::vec3(-0.22f, 0.74f, -0.36f),
        glm::vec3(0.72f, 0.80f, 0.92f),
        0.18f
    };
    float playerTorchInnerConeDegrees = 58.0f;
    float playerTorchOuterConeDegrees = 82.0f;
};

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void init(GLFWwindow* window);
    void shutdown();

    void requestFontPreset(ImGuiFontPreset preset);
    ImGuiFontPreset fontPreset() const { return fontPreset_; }

    void beginFrame();
    void endFrame();

    static void renderOverlay(DebugParams& params, std::vector<RenderLight>& lights);
    static void renderMovementOverlay(PlayerMovementComponent& movement, bool grounded);
    static void renderViewmodelOverlay(ViewmodelComponent& vm);
    static void renderInteractionPrompt(const char* text, bool busy);
    static void renderInventory(InventoryMenuState& menu,
                                const RunSession& session,
                                const ContentRegistry& content,
                                const EffectiveEquipmentView& equipment);

private:
    void applyPendingFontPreset();

    bool initialized_ = false;
    ImGuiFontPreset fontPreset_ = ImGuiFontPreset::SystemSans;
    std::optional<ImGuiFontPreset> pendingFontPreset_ = ImGuiFontPreset::SystemSans;
    std::string activeFontPath_;
};
