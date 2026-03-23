#include "ImGuiLayer.h"
#include "renderer/Renderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

void ImGuiLayer::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410 core");

    spdlog::info("ImGuiLayer initialized");
}

void ImGuiLayer::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    spdlog::info("ImGuiLayer shutdown");
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::renderOverlay(DebugParams& params, std::vector<PointLight>& lights) {
    ImGui::Begin("Debug Overlay");

    // ------------------------------------------------------------------
    // Dither section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Dither", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Threshold Bias", &params.dither.thresholdBias, -0.5f, 0.5f);
        ImGui::SliderFloat("Pattern Scale", &params.dither.patternScale, 64.0f, 512.0f);

        const char* resOptions[] = {"480p (854x480)", "540p (960x540)", "720p (1280x720)"};
        int prevIndex = params.internalResIndex;
        ImGui::Combo("Resolution", &params.internalResIndex, resOptions, 3);
        if (params.internalResIndex != prevIndex) {
            params.resolutionChanged = true;
        }
    }

    // ------------------------------------------------------------------
    // Post-Process section (edges + fog)
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Post-Process", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Edge Threshold", &params.dither.edgeThreshold, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Fog Density", &params.dither.fogDensity, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Fog Start", &params.dither.fogStart, 0.0f, 20.0f, "%.1f");
    }

    // ------------------------------------------------------------------
    // Camera section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::Text("Position: %.1f, %.1f, %.1f",
                    params.cameraPos.x, params.cameraPos.y, params.cameraPos.z);
        ImGui::Text("Direction: %.2f, %.2f, %.2f",
                    params.cameraDir.x, params.cameraDir.y, params.cameraDir.z);
        ImGui::SliderFloat("FOV", &params.cameraFov, 45.0f, 120.0f);
        ImGui::SliderFloat("Speed", &params.cameraSpeed, 0.5f, 10.0f);
    }

    // ------------------------------------------------------------------
    // Performance section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Performance")) {
        ImGui::Text("FPS: %.0f", params.fps);
        ImGui::Text("Frame: %.2f ms", params.frameTimeMs);
        ImGui::Text("Draw calls: %d", params.drawCalls);
    }

    // ------------------------------------------------------------------
    // Lights section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Lights")) {
        for (int i = 0; i < static_cast<int>(lights.size()); ++i) {
            ImGui::PushID(i);
            ImGui::Text("Light %d", i);
            ImGui::DragFloat3("Pos##", &lights[i].position.x, 0.1f);
            ImGui::SliderFloat("Radius##", &lights[i].radius, 1.0f, 20.0f);
            ImGui::SliderFloat("Intensity##", &lights[i].intensity, 0.0f, 2.0f);
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    ImGui::End();
}
