#include "engine/camera/effects/TransitionEffect.h"

#include "engine/camera/CameraMath.h"

#include <algorithm>
#include <cmath>

TransitionEffect::TransitionEffect(const CameraState& from, const glm::vec3& targetPos,
                                   float targetYaw, float targetPitch,
                                   float duration, EasingType easing)
    : fromPosition_(from.position)
    , targetPosition_(targetPos)
    , fromYaw_(from.yaw)
    , targetYaw_(targetYaw)
    , fromPitch_(from.pitch)
    , targetPitch_(targetPitch)
    , duration_(std::max(duration, 0.001f))
    , easing_(easing) {}

void TransitionEffect::update(float deltaTime) {
    progress_ = std::min(progress_ + deltaTime / duration_, 1.0f);
}

CameraState TransitionEffect::apply(const CameraState& input) const {
    const float t = evaluateEasing(easing_, progress_);

    CameraState result = input;
    result.position = glm::mix(fromPosition_, targetPosition_, t);
    result.yaw = fromYaw_ + shortestAngleDelta(fromYaw_, targetYaw_) * t;
    result.pitch = glm::mix(fromPitch_, targetPitch_, t);
    return result;
}

bool TransitionEffect::isFinished() const {
    return progress_ >= 1.0f;
}

float TransitionEffect::shortestAngleDelta(float from, float to) {
    float delta = std::fmod(to - from, 360.0f);
    if (delta > 180.0f) {
        delta -= 360.0f;
    } else if (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}
