#include "game/systems/PlayerTorchSystem.h"

#include "engine/rendering/lighting/RenderLight.h"
#include "game/components/CameraComponent.h"
#include "game/components/PlayerTorchComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

float playerTorchVisualFlicker(float timeSeconds) {
    const float pulseA = std::sin(timeSeconds * 5.7f) * 0.5f + 0.5f;
    const float pulseB = std::sin(timeSeconds * 11.9f + 1.7f) * 0.5f + 0.5f;
    const float pulseC = std::sin(timeSeconds * 18.3f + 0.4f) * 0.5f + 0.5f;
    const float shaped = pulseA * 0.50f + pulseB * 0.32f + pulseC * 0.18f;
    return 0.90f + shaped * 0.24f;
}

float playerTorchLightFlicker(float timeSeconds) {
    const float drift = std::sin(timeSeconds * 2.2f + 0.7f) * 0.5f + 0.5f;
    const float flutter = std::sin(timeSeconds * 7.4f + 2.1f) * 0.5f + 0.5f;
    const float shaped = drift * 0.62f + flutter * 0.38f;
    return 0.92f + shaped * 0.16f;
}

float clampInnerCone(float innerConeDegrees, float outerConeDegrees) {
    return std::clamp(innerConeDegrees, 2.0f, std::max(outerConeDegrees - 1.0f, 3.0f));
}

float clampOuterCone(float innerConeDegrees, float outerConeDegrees) {
    return std::clamp(outerConeDegrees, innerConeDegrees + 1.0f, 85.0f);
}

} // namespace

void updatePlayerTorch(entt::registry& registry, float deltaTime) {
    (void)deltaTime;

    auto view = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag, PlayerTorchComponent>();
    for (auto [entity, transform, camera, torch] : view.each()) {
        const float timeSeconds = static_cast<float>(glfwGetTime());
        const float visualFlicker = playerTorchVisualFlicker(timeSeconds);
        const float lightFlicker = playerTorchLightFlicker(timeSeconds);
        const glm::vec3 cameraUp = safeNormalize(glm::cross(camera.right, camera.forward), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 torchDirection = safeNormalize(
            camera.forward * 0.14f + camera.right * 0.03f + cameraUp * -0.48f,
            glm::vec3(0.0f, 0.0f, -1.0f)
        );

        glm::vec3 flamePosition = transform.position
            + camera.forward * torch.torchForwardOffset
            + camera.right * torch.torchRightOffset
            + cameraUp * -torch.torchDownOffset;
        flamePosition += camera.right * (std::sin(timeSeconds * 6.3f) * 0.010f)
            + cameraUp * (std::sin(timeSeconds * 8.7f + 0.8f) * 0.012f);

        glm::vec3 spillPosition = transform.position
            + camera.right * torch.spillOffset.x
            + cameraUp * torch.spillOffset.y
            + camera.forward * torch.spillOffset.z;

        torch.computedLights.clear();

        RenderLight torchSpill;
        torchSpill.type = LightType::Point;
        torchSpill.position = spillPosition;
        torchSpill.color = torch.spillColor * (0.92f + visualFlicker * 0.14f);
        torchSpill.radius = torch.spillRadius * (0.95f + visualFlicker * 0.08f);
        torchSpill.intensity = torch.spillIntensity * (0.88f + visualFlicker * 0.22f);
        torch.computedLights.push_back(torchSpill);

        RenderLight torchHalo;
        torchHalo.type = LightType::Point;
        torchHalo.position = transform.position + cameraUp * -0.22f;
        torchHalo.color = torch.haloColor * (0.92f + visualFlicker * 0.10f);
        torchHalo.radius = torch.haloRadius * (0.97f + visualFlicker * 0.05f);
        torchHalo.intensity = torch.haloIntensity * (0.92f + visualFlicker * 0.12f);
        torch.computedLights.push_back(torchHalo);

        RenderLight torchLight;
        torchLight.type = LightType::Spot;
        torchLight.position = flamePosition;
        torchLight.direction = torchDirection;
        torchLight.color = torch.torchColor * (0.95f + lightFlicker * 0.04f);
        torchLight.radius = torch.torchRadius * (0.98f + lightFlicker * 0.05f);
        torchLight.intensity = torch.torchIntensity * lightFlicker;
        torchLight.innerConeDegrees = clampInnerCone(torch.innerConeDegrees, torch.outerConeDegrees);
        torchLight.outerConeDegrees = clampOuterCone(torchLight.innerConeDegrees, torch.outerConeDegrees);
        torchLight.castsShadows = true;
        torch.computedLights.push_back(torchLight);

        glm::vec3 handGlowPosition = transform.position
            + camera.forward * torch.handGlowForwardOffset
            + camera.right * torch.handGlowRightOffset
            + glm::vec3(0.0f, -torch.handGlowDownOffset, 0.0f);
        RenderLight handGlow;
        handGlow.type = LightType::Point;
        handGlow.position = handGlowPosition;
        handGlow.color = torch.handGlowColor * (0.98f + visualFlicker * 0.03f);
        handGlow.radius = torch.handGlowRadius * (0.99f + lightFlicker * 0.03f);
        handGlow.intensity = torch.handGlowIntensity * (0.96f + lightFlicker * 0.05f);
        torch.computedLights.push_back(handGlow);

        break;
    }
}
