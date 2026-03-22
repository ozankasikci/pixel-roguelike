#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/Window.h"
#include "engine/ImGuiLayer.h"
#include <imgui.h>
#include "renderer/Shader.h"
#include "renderer/Framebuffer.h"
#include "renderer/DitherPass.h"
#include "renderer/Mesh.h"
#include "renderer/Renderer.h"
#include "game/CathedralScene.h"

#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <cmath>

// Internal resolution presets (D-02)
static const int RES_W[] = {854, 960, 1280};
static const int RES_H[] = {480, 540,  720};

int main() {
    spdlog::set_level(spdlog::level::info);

    // Create window with OpenGL 4.1 Core Profile context
    Window window(1280, 720, "3D Roguelike");

    // Dear ImGui layer
    ImGuiLayer imguiLayer;
    imguiLayer.init(window.handle());

    // Allow normal cursor so ImGui is usable
    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Internal render resolution: 480p 16:9 (D-02) -- index 0
    Framebuffer sceneFBO;
    sceneFBO.create(RES_W[0], RES_H[0]);

    // Create shaders
    Shader sceneShader("assets/shaders/scene.vert", "assets/shaders/scene.frag");

    // Create renderer and dither pass
    Renderer renderer(&sceneShader);
    DitherPass ditherPass;

    // Build cathedral scene
    CathedralScene scene;
    scene.build();

    // Camera state: position + yaw/pitch (degrees)
    scene.cameraPos() = glm::vec3(0.0f, 2.0f, 5.0f);
    float yaw   = -90.0f;  // looking toward -Z
    float pitch =   0.0f;

    // Debug params (drives ImGui overlay values back into the pipeline)
    DebugParams debugParams;

    // Track time for delta
    float lastTime = static_cast<float>(glfwGetTime());

    spdlog::info("Starting render loop at {}x{} (internal: {}x{})",
                 window.width(), window.height(), RES_W[0], RES_H[0]);

    while (!window.shouldClose()) {
        window.pollEvents();

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime   = currentTime - lastTime;
        if (deltaTime < 0.0001f) deltaTime = 0.0001f;  // guard against zero
        lastTime = currentTime;

        // ------------------------------------------------------------------
        // Camera: compute forward and right vectors from yaw/pitch
        // ------------------------------------------------------------------
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward.y = std::sin(glm::radians(pitch));
        forward.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

        // Only move camera when ImGui doesn't own the mouse
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            float moveSpeed = debugParams.cameraSpeed * deltaTime;
            GLFWwindow* win = window.handle();

            if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
                scene.cameraPos() += forward * moveSpeed;
            if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
                scene.cameraPos() -= forward * moveSpeed;
            if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
                scene.cameraPos() -= right * moveSpeed;
            if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
                scene.cameraPos() += right * moveSpeed;
        }

        // Arrow key look (always active for easy navigation)
        {
            float lookSpeed = 60.0f * deltaTime;
            GLFWwindow* win = window.handle();
            if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) yaw   -= lookSpeed;
            if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw   += lookSpeed;
            if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) pitch += lookSpeed;
            if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) pitch -= lookSpeed;
        }

        // Clamp pitch
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        // Recompute forward after potential yaw/pitch change
        forward.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward.y = std::sin(glm::radians(pitch));
        forward.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward = glm::normalize(forward);

        // View and projection matrices
        glm::mat4 viewMatrix = glm::lookAt(
            scene.cameraPos(),
            scene.cameraPos() + forward,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        float aspect = static_cast<float>(sceneFBO.width()) / static_cast<float>(sceneFBO.height());
        glm::mat4 projection = glm::perspective(
            glm::radians(debugParams.cameraFov), aspect, 0.1f, 100.0f);

        // ------------------------------------------------------------------
        // Scene pass: render to FBO at internal resolution
        // ------------------------------------------------------------------
        sceneFBO.bind();
        glViewport(0, 0, sceneFBO.width(), sceneFBO.height());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        renderer.drawScene(scene.objects(), scene.lights(), viewMatrix, projection, scene.cameraPos());

        sceneFBO.unbind();
        glDisable(GL_DEPTH_TEST);

        // ------------------------------------------------------------------
        // Dither pass: apply Bayer dithering at display resolution
        // ------------------------------------------------------------------
        int displayW = window.width();
        int displayH = window.height();

        glClear(GL_COLOR_BUFFER_BIT);

        glm::mat4 inverseView = glm::inverse(viewMatrix);
        ditherPass.apply(sceneFBO.colorTexture(), inverseView,
                         debugParams.thresholdBias, displayW, displayH,
                         debugParams.patternScale);

        // ------------------------------------------------------------------
        // ImGui overlay: AFTER dither pass, BEFORE swapBuffers (Pitfall 5)
        // ------------------------------------------------------------------
        imguiLayer.beginFrame();

        // Update debug params from current state
        debugParams.cameraPos     = scene.cameraPos();
        debugParams.cameraDir     = forward;
        debugParams.fps           = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        debugParams.frameTimeMs   = deltaTime * 1000.0f;
        debugParams.drawCalls     = static_cast<int>(scene.objects().size());

        ImGuiLayer::renderOverlay(debugParams, scene.lights());

        imguiLayer.endFrame();

        // Handle resolution change from ImGui combo
        if (debugParams.resolutionChanged) {
            int idx = debugParams.internalResIndex;
            sceneFBO.resize(RES_W[idx], RES_H[idx]);
            spdlog::info("Internal resolution changed to {}x{}", RES_W[idx], RES_H[idx]);
            debugParams.resolutionChanged = false;
        }

        window.swapBuffers();
    }

    imguiLayer.shutdown();

    spdlog::info("Shutting down");
    return 0;
}
