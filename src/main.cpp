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

#include "engine/Screenshot.h"

#include <spdlog/spdlog.h>
#include <memory>
#include <vector>
#include <cmath>
#include <string>

// Internal resolution presets
static const int RES_W[] = {854, 960, 1280};
static const int RES_H[] = {480, 540,  720};

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    // Parse --screenshot <path> for automated capture
    std::string autoScreenshotPath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            autoScreenshotPath = argv[i + 1];
            ++i;
        }
    }

    Window window(1280, 720, "3D Roguelike");

    ImGuiLayer imguiLayer;
    imguiLayer.init(window.handle());

    glfwSetInputMode(window.handle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // FBO with color + normals + depth (all as textures for post-process)
    Framebuffer sceneFBO;
    sceneFBO.create(RES_W[2], RES_H[2]);  // default 720p

    Shader sceneShader("assets/shaders/scene.vert", "assets/shaders/scene.frag");

    Renderer renderer(&sceneShader);
    DitherPass ditherPass;

    CathedralScene scene;
    scene.build();

    scene.cameraPos() = glm::vec3(0.0f, 2.0f, 5.0f);
    float yaw   = -90.0f;
    float pitch =   0.0f;

    DebugParams debugParams;

    // Auto-screenshot for automated testing
    AutoScreenshot autoCapture;
    if (!autoScreenshotPath.empty()) {
        // Use default dither params for screenshot
        autoCapture.enable(autoScreenshotPath, 10);
        spdlog::info("Auto-screenshot enabled: {}", autoScreenshotPath);
    }

    bool f12Pressed = false;

    float lastTime = static_cast<float>(glfwGetTime());

    spdlog::info("Starting render loop at {}x{} (internal: {}x{})",
                 window.width(), window.height(), RES_W[0], RES_H[0]);

    while (!window.shouldClose()) {
        window.pollEvents();

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime   = currentTime - lastTime;
        if (deltaTime < 0.0001f) deltaTime = 0.0001f;
        lastTime = currentTime;

        // Camera direction from yaw/pitch
        glm::vec3 forward;
        forward.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward.y = std::sin(glm::radians(pitch));
        forward.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward = glm::normalize(forward);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

        // Movement (only when ImGui doesn't own mouse)
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

        // Arrow key look
        {
            float lookSpeed = 60.0f * deltaTime;
            GLFWwindow* win = window.handle();
            if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) yaw   -= lookSpeed;
            if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw   += lookSpeed;
            if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) pitch += lookSpeed;
            if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) pitch -= lookSpeed;
        }

        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        // Recompute forward after yaw/pitch change
        forward.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward.y = std::sin(glm::radians(pitch));
        forward.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        forward = glm::normalize(forward);

        glm::mat4 viewMatrix = glm::lookAt(
            scene.cameraPos(),
            scene.cameraPos() + forward,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        float aspect = static_cast<float>(sceneFBO.width()) / static_cast<float>(sceneFBO.height());
        glm::mat4 projection = glm::perspective(
            glm::radians(debugParams.cameraFov), aspect, 0.1f, 100.0f);

        // ---- Scene pass: render to FBO (color + normals + depth) ----
        sceneFBO.bind();
        glViewport(0, 0, sceneFBO.width(), sceneFBO.height());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        renderer.drawScene(scene.objects(), scene.lights(), viewMatrix, projection, scene.cameraPos());

        sceneFBO.unbind();
        glDisable(GL_DEPTH_TEST);

        // ---- Dither pass: edges + fog + Bayer dithering ----
        int displayW = window.width();
        int displayH = window.height();
        glClear(GL_COLOR_BUFFER_BIT);

        // Sync near/far with projection
        debugParams.dither.nearPlane = 0.1f;
        debugParams.dither.farPlane  = 100.0f;

        ditherPass.apply(sceneFBO.colorTexture(),
                         sceneFBO.depthTexture(),
                         sceneFBO.normalTexture(),
                         debugParams.dither,
                         displayW, displayH);

        // ---- ImGui overlay ----
        imguiLayer.beginFrame();

        debugParams.cameraPos     = scene.cameraPos();
        debugParams.cameraDir     = forward;
        debugParams.fps           = (deltaTime > 0.0f) ? (1.0f / deltaTime) : 0.0f;
        debugParams.frameTimeMs   = deltaTime * 1000.0f;
        debugParams.drawCalls     = static_cast<int>(scene.objects().size());

        ImGuiLayer::renderOverlay(debugParams, scene.lights());

        imguiLayer.endFrame();

        // Handle resolution change
        if (debugParams.resolutionChanged) {
            int idx = debugParams.internalResIndex;
            sceneFBO.resize(RES_W[idx], RES_H[idx]);
            spdlog::info("Internal resolution changed to {}x{}", RES_W[idx], RES_H[idx]);
            debugParams.resolutionChanged = false;
        }

        // F12: manual screenshot
        if (glfwGetKey(window.handle(), GLFW_KEY_F12) == GLFW_PRESS && !f12Pressed) {
            f12Pressed = true;
            saveScreenshot(displayW, displayH);
        }
        if (glfwGetKey(window.handle(), GLFW_KEY_F12) == GLFW_RELEASE) {
            f12Pressed = false;
        }

        // Auto-screenshot: capture and exit
        if (autoCapture.tick(displayW, displayH)) {
            spdlog::info("Auto-screenshot captured, exiting");
            break;
        }

        window.swapBuffers();
    }

    imguiLayer.shutdown();

    spdlog::info("Shutting down");
    return 0;
}
