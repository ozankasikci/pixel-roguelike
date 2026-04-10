#include "engine/camera/effects/FOVEffect.h"

#include "engine/camera/CameraMath.h"

#include <algorithm>

FOVEffect::FOVEffect(float deltaFov, float duration, EasingType easing)
    : deltaFov_(deltaFov)
    , duration_(std::max(duration, 0.001f))
    , easing_(easing) {}

void FOVEffect::update(float deltaTime) {
    progress_ = std::min(progress_ + deltaTime / duration_, 1.0f);
}

CameraState FOVEffect::apply(const CameraState& input) const {
    const float easedProgress = evaluateEasing(easing_, progress_);
    const float fovOffset = deltaFov_ * (1.0f - easedProgress);

    CameraState result = input;
    result.fov += fovOffset;
    return result;
}

bool FOVEffect::isFinished() const {
    return progress_ >= 1.0f;
}
