#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/Window.h"
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

int main() {
    spdlog::set_level(spdlog::level::info);

    // Create window with OpenGL 4.1 Core Profile context
    Window window(1280, 720, "3D Roguelike");

    // Internal render resolution: 480p 16:9 (D-02)
    constexpr int INTERNAL_W = 854;
    constexpr int INTERNAL_H = 480;

    // Create offscreen FBO at internal resolution
    Framebuffer sceneFBO;
    sceneFBO.create(INTERNAL_W, INTERNAL_H);

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
    float fovDeg = 70.0f;
    float cameraSpeed = 3.0f;

    // Track time for delta
    float lastTime = static_cast<float>(glfwGetTime());

    spdlog::info("Starting render loop at {}x{} (internal: {}x{})",
                 window.width(), window.height(), INTERNAL_W, INTERNAL_H);

    while (!window.shouldClose()) {
        window.pollEvents();

        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime   = currentTime - lastTime;
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

        // WASD movement
        GLFWwindow* win = window.handle();
        float moveSpeed = cameraSpeed * deltaTime;

        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
            scene.cameraPos() += forward * moveSpeed;
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)
            scene.cameraPos() -= forward * moveSpeed;
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)
            scene.cameraPos() -= right * moveSpeed;
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)
            scene.cameraPos() += right * moveSpeed;

        // Arrow key look
        float lookSpeed = 60.0f * deltaTime;
        if (glfwGetKey(win, GLFW_KEY_LEFT)  == GLFW_PRESS) yaw   -= lookSpeed;
        if (glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw   += lookSpeed;
        if (glfwGetKey(win, GLFW_KEY_UP)    == GLFW_PRESS) pitch += lookSpeed;
        if (glfwGetKey(win, GLFW_KEY_DOWN)  == GLFW_PRESS) pitch -= lookSpeed;

        // Clamp pitch
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        // View and projection matrices
        glm::mat4 viewMatrix = glm::lookAt(
            scene.cameraPos(),
            scene.cameraPos() + forward,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
        float aspect = static_cast<float>(sceneFBO.width()) / static_cast<float>(sceneFBO.height());
        glm::mat4 projection = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 100.0f);

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
        ditherPass.apply(sceneFBO.colorTexture(), inverseView, 0.0f, displayW, displayH);

        window.swapBuffers();
    }

    spdlog::info("Shutting down");
    return 0;
}
