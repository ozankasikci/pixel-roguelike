#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "engine/rendering/DitherPass.h"

// Forward declarations
struct GLFWwindow;
struct PointLight;

struct DebugParams {
    // Dither parameters
    DitherParams dither;

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
};

class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void init(GLFWwindow* window);
    void shutdown();

    void beginFrame();
    void endFrame();

    static void renderOverlay(DebugParams& params, std::vector<PointLight>& lights);
};
