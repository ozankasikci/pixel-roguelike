#include "editor/ui/EditorCameraDebugPanel.h"

#include "engine/camera/CameraManager.h"
#include "engine/camera/CameraState.h"

#include <imgui.h>

#include <cstdio>

namespace {

float s_shakeTrauma = 0.5f;
float s_fovDelta = 8.0f;
float s_fovDuration = 0.4f;
float s_targetPos[3] = {0.0f, 2.0f, 0.0f};
float s_targetYaw = -90.0f;
float s_targetPitch = 0.0f;
float s_transitionDuration = 1.5f;

} // namespace

void renderCameraDebugPanel(CameraManager& cameraManager, bool* open) {
    if (!ImGui::Begin("Camera Debug", open)) {
        ImGui::End();
        return;
    }

    const auto& state = cameraManager.getState();
    const auto& base = cameraManager.getBaseState();

    ImGui::Text("Position: %.1f, %.1f, %.1f", state.position.x, state.position.y, state.position.z);
    ImGui::Text("Yaw: %.1f  Pitch: %.1f", state.yaw, state.pitch);
    ImGui::Text("FOV: %.1f", state.fov);

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Shake", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Trauma", &s_shakeTrauma, 0.0f, 1.0f, "%.2f");
        if (ImGui::Button("Trigger Shake")) {
            cameraManager.shake(s_shakeTrauma);
        }
        ImGui::Text("Current trauma: %.2f", cameraManager.currentTrauma());
    }

    if (ImGui::CollapsingHeader("FOV Punch")) {
        ImGui::SliderFloat("Delta FOV", &s_fovDelta, -20.0f, 20.0f, "%.1f");
        ImGui::SliderFloat("Duration##fov", &s_fovDuration, 0.1f, 3.0f, "%.2f");
        if (ImGui::Button("Punch FOV")) {
            cameraManager.punchFOV(s_fovDelta, s_fovDuration);
        }
    }

    if (ImGui::CollapsingHeader("Transition")) {
        ImGui::DragFloat3("Target Pos", s_targetPos, 0.1f);
        ImGui::SliderFloat("Target Yaw", &s_targetYaw, -180.0f, 180.0f, "%.1f");
        ImGui::SliderFloat("Target Pitch", &s_targetPitch, -89.0f, 89.0f, "%.1f");
        ImGui::SliderFloat("Duration##trans", &s_transitionDuration, 0.1f, 5.0f, "%.2f");
        if (ImGui::Button("Transition To")) {
            const glm::vec3 target(s_targetPos[0], s_targetPos[1], s_targetPos[2]);
            cameraManager.transitionTo(target, s_targetYaw, s_targetPitch, s_transitionDuration);
        }
        ImGui::SameLine();
        if (ImGui::Button("Return To Player")) {
            cameraManager.transitionTo(base.position, base.yaw, base.pitch, s_transitionDuration);
        }
        ImGui::Text("Status: %s", cameraManager.isTransitioning() ? "Transitioning" : "Idle");
    }

    ImGui::End();
}
