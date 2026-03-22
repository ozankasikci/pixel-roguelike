#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "engine/Window.h"
#include "renderer/Shader.h"
#include "renderer/Framebuffer.h"
#include "renderer/DitherPass.h"
#include "renderer/Mesh.h"

#include <spdlog/spdlog.h>
#include <memory>
#include <vector>

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

    // Create dither post-process pass
    DitherPass ditherPass;

    // Create test geometry: floor + 3 cubes
    Mesh floor = Mesh::createPlane(10.0f);
    Mesh cube1 = Mesh::createCube(1.0f);
    Mesh cube2 = Mesh::createCube(0.8f);
    Mesh cube3 = Mesh::createCube(1.2f);

    // Camera setup
    glm::vec3 cameraTarget(0.0f, 0.0f, 0.0f);
    float cameraRadius = 5.0f;
    float cameraHeight = 2.0f;
    float fovDeg = 70.0f;

    spdlog::info("Starting render loop at {}x{} (internal: {}x{})",
                 window.width(), window.height(), INTERNAL_W, INTERNAL_H);

    while (!window.shouldClose()) {
        window.pollEvents();

        // Orbit camera around Y axis based on time (demonstrates dither stability)
        float time = static_cast<float>(glfwGetTime());
        float orbitAngle = time * 0.3f;  // slow orbit

        glm::vec3 cameraPos(
            cameraRadius * std::cos(orbitAngle),
            cameraHeight,
            cameraRadius * std::sin(orbitAngle)
        );

        // View and projection matrices
        glm::mat4 viewMatrix = glm::lookAt(cameraPos, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        float aspect = static_cast<float>(INTERNAL_W) / static_cast<float>(INTERNAL_H);
        glm::mat4 projection = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 100.0f);

        // --- Scene pass: render to FBO at internal resolution ---
        sceneFBO.bind();
        glViewport(0, 0, sceneFBO.width(), sceneFBO.height());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        sceneShader.use();
        sceneShader.setMat4("uView", viewMatrix);
        sceneShader.setMat4("uProjection", projection);

        // Draw floor
        glm::mat4 floorModel = glm::mat4(1.0f);
        sceneShader.setMat4("uModel", floorModel);
        floor.draw();

        // Draw cube 1 at origin (above floor)
        glm::mat4 cube1Model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
        sceneShader.setMat4("uModel", cube1Model);
        cube1.draw();

        // Draw cube 2 offset
        glm::mat4 cube2Model = glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, 0.4f, 1.0f));
        sceneShader.setMat4("uModel", cube2Model);
        cube2.draw();

        // Draw cube 3 offset
        glm::mat4 cube3Model = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.6f, -1.5f));
        sceneShader.setMat4("uModel", cube3Model);
        cube3.draw();

        sceneFBO.unbind();
        glDisable(GL_DEPTH_TEST);

        // --- Dither pass: apply Bayer dithering at display resolution ---
        int displayW = window.width();
        int displayH = window.height();

        glClear(GL_COLOR_BUFFER_BIT);

        // Pass inverse view matrix for world-space dither anchoring (RNDR-03)
        glm::mat4 inverseView = glm::inverse(viewMatrix);
        ditherPass.apply(sceneFBO.colorTexture(), inverseView, 0.0f, displayW, displayH);

        window.swapBuffers();
    }

    spdlog::info("Shutting down");
    return 0;
}
