#pragma once

#include <glm/glm.hpp>
#include <vector>

// Forward declarations
struct GLFWwindow;

// Forward declare to avoid including Renderer.h in a header
struct PointLight;

struct DebugParams {
    // Dither parameters (D-07)
    float thresholdBias  = 0.0f;      // range: -0.5 to 0.5
    float patternScale   = 256.0f;    // range: 64 to 512
    int   internalResIndex  = 0;      // 0=480p, 1=540p, 2=720p
    bool  resolutionChanged = false;

    // Camera info (D-07)
    glm::vec3 cameraPos = glm::vec3(0.0f);
    glm::vec3 cameraDir = glm::vec3(0.0f);
    float cameraFov   = 70.0f;        // range: 45 to 120
    float cameraSpeed = 3.0f;         // range: 0.5 to 10.0

    // Performance (D-07)
    float fps         = 0.0f;
    float frameTimeMs = 0.0f;
    int   drawCalls   = 0;

    // Light controls (D-07)
    bool showLightEditor = false;
};

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    // Non-copyable
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void init(GLFWwindow* window);
    void shutdown();

    void beginFrame();
    void endFrame();

    static void renderOverlay(DebugParams& params, std::vector<PointLight>& lights);
};
