#pragma once

#include "engine/camera/CameraEffect.h"
#include "engine/camera/CameraState.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

class ShakeEffect;

class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    CameraManager(CameraManager&&) noexcept;
    CameraManager& operator=(CameraManager&&) noexcept;

    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    void setBaseState(const glm::vec3& position, float yaw, float pitch);
    void setProjection(float fov, float aspectRatio, float nearPlane, float farPlane);

    template<typename T, typename... Args>
    T& addEffect(Args&&... args) {
        auto effect = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *effect;
        effects_.push_back(std::move(effect));
        return ref;
    }

    void removeEffect(CameraEffect* effect);
    void clearEffects();

    void transitionTo(const glm::vec3& position, float yaw, float pitch,
                      float duration, EasingType easing = EasingType::EaseOutCubic);
    void shake(float trauma);
    void punchFOV(float deltaFov, float duration,
                  EasingType easing = EasingType::EaseOutCubic);

    void update(float deltaTime);
    const CameraState& getState() const;
    const CameraState& getBaseState() const;

    bool isTransitioning() const;
    bool hasActiveEffects() const;
    float currentTrauma() const;

private:
    CameraState baseState_;
    CameraState finalState_;
    float aspectRatio_ = 16.0f / 9.0f;
    std::vector<std::unique_ptr<CameraEffect>> effects_;

    void pruneFinishedEffects();
};
