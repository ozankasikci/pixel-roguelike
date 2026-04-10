#pragma once

#include "engine/camera/CameraEffect.h"

class FOVEffect : public CameraEffect {
public:
    FOVEffect(float deltaFov, float duration, EasingType easing);

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    float deltaFov_;
    float duration_;
    float progress_ = 0.0f;
    EasingType easing_;
};
