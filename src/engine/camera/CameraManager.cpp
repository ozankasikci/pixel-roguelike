#include "engine/camera/CameraManager.h"

#include "engine/camera/CameraMath.h"
#include "engine/camera/effects/ShakeEffect.h"
#include "engine/camera/effects/TransitionEffect.h"
#include "engine/camera/effects/FOVEffect.h"

#include <algorithm>

CameraManager::CameraManager() {
    rebuildCameraMatrices(baseState_, aspectRatio_);
    finalState_ = baseState_;
}

CameraManager::~CameraManager() = default;

CameraManager::CameraManager(CameraManager&&) noexcept = default;
CameraManager& CameraManager::operator=(CameraManager&&) noexcept = default;

void CameraManager::setBaseState(const glm::vec3& position, float yaw, float pitch) {
    baseState_.position = position;
    baseState_.yaw = yaw;
    baseState_.pitch = pitch;
}

void CameraManager::setProjection(float fov, float aspectRatio, float nearPlane, float farPlane) {
    baseState_.fov = fov;
    baseState_.nearPlane = nearPlane;
    baseState_.farPlane = farPlane;
    aspectRatio_ = aspectRatio;
}

void CameraManager::removeEffect(CameraEffect* effect) {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [effect](const auto& ptr) { return ptr.get() == effect; }),
        effects_.end());
}

void CameraManager::clearEffects() {
    effects_.clear();
}

void CameraManager::transitionTo(const glm::vec3& position, float yaw, float pitch,
                                 float duration, EasingType easing) {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const auto& ptr) { return dynamic_cast<TransitionEffect*>(ptr.get()) != nullptr; }),
        effects_.end());

    addEffect<TransitionEffect>(finalState_, position, yaw, pitch, duration, easing);
}

void CameraManager::shake(float trauma) {
    for (auto& effect : effects_) {
        if (auto* shakeEffect = dynamic_cast<ShakeEffect*>(effect.get())) {
            shakeEffect->addTrauma(trauma);
            return;
        }
    }
    auto& effect = addEffect<ShakeEffect>();
    effect.addTrauma(trauma);
}

void CameraManager::punchFOV(float deltaFov, float duration, EasingType easing) {
    addEffect<FOVEffect>(deltaFov, duration, easing);
}

void CameraManager::update(float deltaTime) {
    rebuildCameraMatrices(baseState_, aspectRatio_);

    for (auto& effect : effects_) {
        effect->update(deltaTime);
    }

    CameraState current = baseState_;
    for (auto& effect : effects_) {
        current = effect->apply(current);
    }

    rebuildCameraMatrices(current, aspectRatio_);
    finalState_ = current;

    pruneFinishedEffects();
}

const CameraState& CameraManager::getState() const {
    return finalState_;
}

const CameraState& CameraManager::getBaseState() const {
    return baseState_;
}

bool CameraManager::isTransitioning() const {
    for (const auto& effect : effects_) {
        if (dynamic_cast<const TransitionEffect*>(effect.get()) != nullptr) {
            return true;
        }
    }
    return false;
}

bool CameraManager::hasActiveEffects() const {
    return !effects_.empty();
}

void CameraManager::pruneFinishedEffects() {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const auto& ptr) { return ptr->isFinished(); }),
        effects_.end());
}
