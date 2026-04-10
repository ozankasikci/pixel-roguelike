#include "engine/camera/effects/ShakeEffect.h"

#include <glm/gtc/noise.hpp>

#include <algorithm>

void ShakeEffect::addTrauma(float amount) {
    trauma_ = std::clamp(trauma_ + amount, 0.0f, 1.0f);
}

float ShakeEffect::getTrauma() const {
    return trauma_;
}

void ShakeEffect::update(float deltaTime) {
    trauma_ = std::max(trauma_ - traumaDecayRate * deltaTime, 0.0f);
    noiseTime_ += deltaTime;
}

CameraState ShakeEffect::apply(const CameraState& input) const {
    if (trauma_ <= 0.0001f) {
        return input;
    }

    const float magnitude = trauma_ * trauma_;
    const float t = noiseTime_ * noiseFrequency;

    const float offsetX = glm::perlin(glm::vec2(t, 0.0f)) * maxTranslationOffset * magnitude;
    const float offsetY = glm::perlin(glm::vec2(0.0f, t)) * maxTranslationOffset * magnitude;
    const float yawOffset = glm::perlin(glm::vec2(t, 100.0f)) * maxYawOffset * magnitude;
    const float pitchOffset = glm::perlin(glm::vec2(t, 200.0f)) * maxPitchOffset * magnitude;

    CameraState result = input;
    result.position.x += offsetX;
    result.position.y += offsetY;
    result.yaw += yawOffset;
    result.pitch += pitchOffset;
    return result;
}

bool ShakeEffect::isFinished() const {
    return false;
}
