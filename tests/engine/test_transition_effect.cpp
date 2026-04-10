#include "engine/camera/effects/TransitionEffect.h"
#include "engine/camera/CameraMath.h"

#include <cassert>
#include <cmath>

namespace {

bool approxEqual(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) < epsilon;
}

bool approxEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.01f) {
    return approxEqual(a.x, b.x, epsilon)
        && approxEqual(a.y, b.y, epsilon)
        && approxEqual(a.z, b.z, epsilon);
}

} // namespace

int main() {
    // --- At t=0 output matches start state ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f, 0.0f, 0.0f);
        from.yaw = 0.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(10.0f, 5.0f, 0.0f), 90.0f, 30.0f,
                                1.0f, EasingType::Linear);

        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqualVec3(result.position, glm::vec3(0.0f, 0.0f, 0.0f)));
        assert(approxEqual(result.yaw, 0.0f));
    }

    // --- At t=1 output matches target ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f);
        from.yaw = 0.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(10.0f, 5.0f, 0.0f), 90.0f, 30.0f,
                                1.0f, EasingType::Linear);

        effect.update(1.0f);
        assert(effect.isFinished());

        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqualVec3(result.position, glm::vec3(10.0f, 5.0f, 0.0f)));
        assert(approxEqual(result.yaw, 90.0f));
        assert(approxEqual(result.pitch, 30.0f));
    }

    // --- Linear midpoint is halfway ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f);
        from.yaw = 0.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(10.0f, 0.0f, 0.0f), 0.0f, 0.0f,
                                1.0f, EasingType::Linear);

        effect.update(0.5f);
        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.position.x, 5.0f));
    }

    // --- Yaw wraparound: 350 -> 10 goes through 0, not 340 degrees around ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f);
        from.yaw = 350.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(0.0f), 10.0f, 0.0f,
                                1.0f, EasingType::Linear);

        effect.update(0.5f);
        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.yaw, 360.0f));
    }

    return 0;
}
