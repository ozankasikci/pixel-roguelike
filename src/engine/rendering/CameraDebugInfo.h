#pragma once
#include <glm/glm.hpp>

// Read-only camera state for debug overlay display.
// Written by RuntimeSceneRenderer, read by ImGuiLayer.
struct CameraDebugInfo {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f};
    float fov = 70.0f;
    float moveSpeed = 3.0f;
};
