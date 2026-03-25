#include "ImGuiLayer.h"
#include "engine/rendering/Renderer.h"

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

#include "game/components/PlayerMovementComponent.h"

void ImGuiLayer::renderMovementOverlay(PlayerMovementComponent& movement, bool grounded) {
    ImGui::Begin("Movement");

    // Ground state
    const char* stateStr = grounded ? "ON GROUND" : "IN AIR";
    ImGui::Text("State: %s", stateStr);
    ImGui::Text("Velocity: %.1f, %.1f, %.1f",
        movement.velocity.x, movement.velocity.y, movement.velocity.z);
    ImGui::Text("Speed: %.1f", glm::length(glm::vec2(movement.velocity.x, movement.velocity.z)));

    ImGui::Separator();

    // Ground movement
    if (ImGui::CollapsingHeader("Ground Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Max Speed", &movement.maxGroundSpeed, 1.0f, 15.0f);
        ImGui::SliderFloat("Acceleration", &movement.acceleration, 5.0f, 60.0f);
        ImGui::SliderFloat("Deceleration", &movement.deceleration, 5.0f, 60.0f);
    }

    // Air movement
    if (ImGui::CollapsingHeader("Air Movement")) {
        ImGui::SliderFloat("Air Accel", &movement.airAcceleration, 1.0f, 30.0f);
    }

    // Gravity & jump
    if (ImGui::CollapsingHeader("Jump & Gravity")) {
        ImGui::SliderFloat("Gravity", &movement.gravity, -30.0f, -5.0f);
        ImGui::SliderFloat("Jump Impulse", &movement.jumpImpulse, 2.0f, 12.0f);
        ImGui::SliderFloat("Hold Gravity Scale", &movement.jumpHoldGravityScale, 0.1f, 1.0f);
        ImGui::SliderFloat("Max Hold Time", &movement.maxJumpHoldTime, 0.1f, 0.6f);
    }

    ImGui::End();
}

#include "game/components/ViewmodelComponent.h"

void ImGuiLayer::renderViewmodelOverlay(ViewmodelComponent& vm) {
    ImGui::Begin("Viewmodel");

    ImGui::DragFloat3("Position", &vm.viewOffset.x, 0.005f, -1.0f, 1.0f, "%.3f");
    ImGui::DragFloat3("Rotation", &vm.rotation.x, 0.5f, -360.0f, 360.0f, "%.1f");
    ImGui::DragFloat("Scale", &vm.scale, 0.0001f, 0.0001f, 0.1f, "%.4f");

    ImGui::Separator();
    ImGui::DragFloat3("Mesh Center", &vm.meshCenter.x, 0.5f, -500.0f, 500.0f, "%.1f");

    if (ImGui::CollapsingHeader("Bob Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Bob Speed", &vm.bobSpeed, 0.1f, 0.0f, 10.0f, "%.1f");
        ImGui::DragFloat("Bob Amplitude", &vm.bobAmplitude, 0.001f, 0.0f, 0.1f, "%.3f");
    }

    ImGui::End();
}

void ImGuiLayer::renderInteractionPrompt(const char* text, bool busy) {
    if (text == nullptr || text[0] == '\0') return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 promptSize(360.0f, 54.0f);
    const ImVec2 promptPos(
        viewport->Pos.x + (viewport->Size.x - promptSize.x) * 0.5f,
        viewport->Pos.y + viewport->Size.y - 118.0f
    );

    ImGui::SetNextWindowPos(promptPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(promptSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(busy ? 0.62f : 0.48f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
    ImGui::Begin("Interaction Prompt", nullptr, flags);
    ImGui::SetWindowFontScale(busy ? 1.18f : 1.1f);
    ImGui::TextUnformatted(text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
    ImGui::PopStyleVar(2);
}
