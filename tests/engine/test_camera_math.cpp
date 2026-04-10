#include "engine/camera/CameraMath.h"

#include <cassert>
#include <cmath>

namespace {

bool approxEqual(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) < epsilon;
}

bool approxEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.001f) {
    return approxEqual(a.x, b.x, epsilon)
        && approxEqual(a.y, b.y, epsilon)
        && approxEqual(a.z, b.z, epsilon);
}

} // namespace

int main() {
    // --- Easing: Linear ---
    {
        assert(approxEqual(evaluateEasing(EasingType::Linear, 0.0f), 0.0f));
        assert(approxEqual(evaluateEasing(EasingType::Linear, 0.5f), 0.5f));
        assert(approxEqual(evaluateEasing(EasingType::Linear, 1.0f), 1.0f));
    }

    // --- Easing: boundaries always 0 and 1 ---
    {
        const EasingType types[] = {
            EasingType::Linear, EasingType::EaseOutQuad, EasingType::EaseOutCubic,
            EasingType::EaseInOutCubic, EasingType::EaseInOutQuad
        };
        for (auto type : types) {
            assert(approxEqual(evaluateEasing(type, 0.0f), 0.0f));
            assert(approxEqual(evaluateEasing(type, 1.0f), 1.0f));
        }
    }

    // --- Easing: EaseOutCubic is fast at start, slow at end ---
    {
        const float mid = evaluateEasing(EasingType::EaseOutCubic, 0.5f);
        assert(mid > 0.5f);
    }

    // --- Easing: clamped input ---
    {
        assert(approxEqual(evaluateEasing(EasingType::Linear, -0.5f), 0.0f));
        assert(approxEqual(evaluateEasing(EasingType::Linear, 1.5f), 1.0f));
    }

    // --- buildCameraForward: looking down -Z at yaw=-90, pitch=0 ---
    {
        const glm::vec3 forward = buildCameraForward(-90.0f, 0.0f);
        assert(approxEqualVec3(forward, glm::vec3(0.0f, 0.0f, -1.0f)));
    }

    // --- buildCameraForward: looking up ---
    {
        const glm::vec3 forward = buildCameraForward(-90.0f, 45.0f);
        assert(forward.y > 0.0f);
        assert(approxEqual(glm::length(forward), 1.0f));
    }

    // --- rebuildCameraMatrices: produces valid matrices ---
    {
        CameraState state;
        state.position = glm::vec3(1.0f, 2.0f, 3.0f);
        state.yaw = -90.0f;
        state.pitch = 0.0f;
        state.fov = 70.0f;
        rebuildCameraMatrices(state, 16.0f / 9.0f);

        assert(approxEqualVec3(state.forward, glm::vec3(0.0f, 0.0f, -1.0f)));
        assert(state.viewMatrix != glm::mat4(1.0f));
        assert(state.projectionMatrix != glm::mat4(1.0f));
    }

    return 0;
}
