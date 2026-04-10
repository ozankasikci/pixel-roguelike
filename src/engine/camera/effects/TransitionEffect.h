#pragma once

#include "engine/camera/CameraEffect.h"

class TransitionEffect : public CameraEffect {
public:
    TransitionEffect(const CameraState& from, const glm::vec3& targetPos,
                     float targetYaw, float targetPitch,
                     float duration, EasingType easing);

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    static float shortestAngleDelta(float from, float to);

    glm::vec3 fromPosition_;
    glm::vec3 targetPosition_;
    float fromYaw_;
    float targetYaw_;
    float fromPitch_;
    float targetPitch_;
    float duration_;
    float progress_ = 0.0f;
    EasingType easing_;
};
